//cs335 - Spring 2013
//program: Rain drops
//author:  Gordon Griesel

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <glib.h>
#include <GL/glfw.h>

#include "constants.h"

//These components can be turned on and off
#define USE_FONTS
#define USE_LOG

#ifdef USE_LOG
#include "log.h"
#endif //USE_LOG
#include "defs.h"

#ifdef USE_FONTS
#include "fonts.h"
#endif //USE_FONTS

//macros
#define rnd() (((double)rand())/(double)RAND_MAX)
#define random(a) (rand()%a)

//constants
const float timeslice = 1.0f/60.0f;

//prototypes
void init(void);
int InitGL(GLvoid);
void checkkey(int k1, int k2);
void physics(void);
void render(void);
extern GLuint loadBMP(const char *imagepath);
extern GLuint tex_readgl_bmp(char *fileName, int alpha_channel);

//global variables and constants
int time_control=1;
int input_directions=0; //bitmask of arrow keys currently down, see INPUT_* macros

typedef struct t_laser {
	int team;
	int linewidth;
	Vec pos;
	Vec lastpos;
	Vec vel;
	Vec maxvel;
	float length;
	float color[4];
} Laser;

typedef struct t_ship {
	int team; // TEAM_REBELS or TEAM_EMPIRE
	int shiptype;
	int health;
	int shields;
	int damage;
	int shotfreq;
	int is_vulnerable; // Is ship vulnerable to hostile fire?
	int is_visible; // Are we drawing the ship?
	int can_attack; // Can the ship fire?
	int can_move; // Can the ship move?
	Vec vel;
	Vec maxvel;
	Vec pos;
	Vec lastpos;
	float edge_length; // Edge size of square texture
	float hitbox_radius; // Radius of circular hitbox
} Ship;


GList *laser_list = NULL;

Ship player_ship;
GLuint umbrella_texture;
GLuint background_texture;
void render_ship(Ship *ship);

void laser_move_frame(Laser *node);
void laser_check_collision(Laser *node);
void laser_render(Laser *node);
void laser_fire(Ship *ship);

int show_lasers      = 1;
int show_text      = 1;


int main(int argc, char **argv)
{
	int i, nmodes;
	GLFWvidmode glist[256];
	open_log_file();
	srand((unsigned int)time(NULL));
	//
	glfwInit();
	srand(time(NULL));
	nmodes = glfwGetVideoModes(glist, 100);
	xres = glist[nmodes-1].Width;
	yres = glist[nmodes-1].Height;
	Log("setting window to: %i x %i\n",xres,yres);
	//if (!glfwOpenWindow(xres, yres, 0, 0, 0, 0, 0, 0, GLFW_WINDOW)) {
	if (!glfwOpenWindow(xres,yres,8,8,8,0,32,0,GLFW_FULLSCREEN)) {
		close_log_file();
		glfwTerminate();
		return 0;
	}

	glfwSetWindowTitle("STAR WARS Galaga");
	glfwSetWindowPos(0, 0);
	init();
	InitGL();
	glfwSetKeyCallback((GLFWkeyfun)(checkkey));
	glfwDisable( GLFW_MOUSE_CURSOR );
	
	#ifdef USE_FONTS
	//glShadeModel(GL_FLAT);
	glShadeModel(GL_SMOOTH);
	//texture maps must be enabled to draw fonts
	glEnable(GL_TEXTURE_2D);
	initialize_fonts();
	#endif //USE_FONTS
	
	while(1) {
		for (i=0; i<time_control; i++)
			physics();

		render();
		glfwSwapBuffers();
		if (glfwGetKey(GLFW_KEY_ESC)) break;
		if (!glfwGetWindowParam(GLFW_OPENED)) break;
	}
	glfwSetKeyCallback((GLFWkeyfun)NULL);
	glfwSetMousePosCallback((GLFWmouseposfun)NULL);
	close_log_file();
	glfwTerminate();
	#ifdef USE_FONTS
	cleanup_fonts();
	#endif //USE_FONTS

	g_list_foreach(laser_list, (GFunc)free, NULL);
	g_list_free(laser_list);
	return 0;
}

void checkkey(int k1, int k2)
{
	if (k2 == GLFW_PRESS) {
		// Set the flag for the given arrow key if pressed
		if(k1 == GLFW_KEY_LEFT) input_directions |= INPUT_LEFT;
		if(k1 == GLFW_KEY_RIGHT) input_directions |= INPUT_RIGHT;

	} else if (k2 == GLFW_RELEASE) {
		// Unset the flag for the given arrow key if released
		if(k1 == GLFW_KEY_LEFT) input_directions &= ~INPUT_LEFT;
		if(k1 == GLFW_KEY_RIGHT) input_directions &= ~INPUT_RIGHT;

		//don't process any other keys on a release
		return;
	}

	if (k1 == ' ' && player_ship.can_attack) {
		laser_fire(&player_ship);
		return;
	}

	if (k1 == '`') {
		if (--time_control < 0)
			time_control = 0;
		return;
	}
	if (k1 == '1') {
		if (++time_control > 32)
			time_control = 32;
		return;
	}
	if (k1 == '2') {
		//shrink the player_ship
		player_ship.edge_length *= (1.0 / 1.05);
		player_ship.hitbox_radius = player_ship.edge_length * 0.5;
		return;
	}
	if (k1 == '3') {
		//enlarge the player_ship
		player_ship.edge_length *= 1.05;
		player_ship.hitbox_radius = player_ship.edge_length * 0.5;
		return;
	}
	if (k1 == '4') {
		show_lasers ^= 1;
		return;
	}
	if (k1 == '5') {
		show_text ^= 1;
		return;
	}
	if (k1 == '6') {
		player_ship.is_visible ^= 1;
		return;
	}
	if (k1 == '7') {
		player_ship.is_vulnerable ^= 1;
		return;
	}
	if (k1 == '8') {
		player_ship.can_attack ^= 1;
		return;
	}
	if (k1 == '9') {
		player_ship.can_move ^= 1;
		return;
	}
}

void init(void)
{
	player_ship.pos[0] = xres/2;
	player_ship.pos[1] = 100.0;
	VecCopy(player_ship.pos, player_ship.lastpos);
	player_ship.edge_length = 75.0;
	player_ship.hitbox_radius = player_ship.edge_length * .75;
	player_ship.team = TEAM_REBELS;
	player_ship.is_vulnerable = 1;
	player_ship.is_visible = 1;
	player_ship.can_attack = 1;
	player_ship.can_move = 1;
}

int InitGL(GLvoid)
{
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FOG);
	glDisable(GL_CULL_FACE);
	glClearColor(0.1f, 0.2f, 0.3f, 0.0f);
	
	background_texture = loadBMP("bg.bmp");
    umbrella_texture = tex_readgl_bmp("XWing.bmp", 1.0);
	return 1;
}

void render(GLvoid)
{
	//Log("render()...\n");
	glfwGetWindowSize(&xres, &yres);
	glViewport(halfpad, 0, xres-pad, yres);
	//clear color buffer
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode (GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glOrtho(0, xres, 0, yres, -1, 1);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	//background 
	glBegin(GL_QUADS);
		glColor3f(0.0f, 0.0f, 0.0f);
		glVertex2i(xres-1, 0);
		glVertex2i(xres-halfpad, 0);
		glVertex2i(xres-halfpad, yres-1);
		glVertex2i(xres-1, yres-1);

		glColor3f(0.0f, 0.0f, 0.0f);
		glVertex2i(halfpad, 0);
		glVertex2i(0, 0);
		glVertex2i(0, yres-1);
		glVertex2i(halfpad, yres-1);
	glEnd();
 

	glBindTexture(GL_TEXTURE_2D, background_texture);
	glBegin(GL_QUADS);
		glColor3f(1.0f, 1.0f, 1.0f);
	 	glTexCoord2f(1.0f, 0.0f); glVertex2i(xres-halfpad, 0);
		glTexCoord2f(0.0f, 0.0f); glVertex2i(halfpad,      0);
		glTexCoord2f(0.0f, 1.0f); glVertex2i(halfpad,      yres-1);
		glTexCoord2f(1.0f, 1.0f); glVertex2i(xres-halfpad, yres-1);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);


	if (show_lasers) {
		g_list_foreach(laser_list, (GFunc)laser_render, NULL);
		glLineWidth(1);
	}
	glDisable(GL_BLEND);

	if(player_ship.is_visible)
		render_ship(&player_ship);

	if (show_text) {
		//draw some text
		Rect r;
		r.left   = 10;
		r.bot    = 200;
		r.center = 0;
		ggprint16(&r, 16, 0x00cc6622, "< > Fire laser", NULL);
		ggprint16(&r, 16, 0x00aa00aa, "<`> Slow game", NULL);
		ggprint16(&r, 16, 0x00aa00aa, "<1> Speedup game", NULL);
		ggprint16(&r, 16, 0x0000aaaa, "<2> Shrink ship", NULL);
		ggprint16(&r, 16, 0x0000aaaa, "<3> Grow ship", NULL);
		ggprint16(&r, 16, 0x00aaaa00, "<4> Render lasers: %s",show_lasers==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<5> Render text: %s",show_text==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<6> Render ship: %s",player_ship.is_visible==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<7> Enable vulnerablility: %s",player_ship.is_vulnerable==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<8> Enable attack: %s",player_ship.can_attack==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<9> Enable movement: %s",player_ship.can_move==1?"On":"Off");
	}
}

void render_ship(Ship *ship)
{
	glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
	glPushMatrix();
	glTranslatef(ship->pos[0], ship->pos[1], ship->pos[2]);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glBindTexture(GL_TEXTURE_2D, umbrella_texture); //TODO: vary texture bmp/orientation based on ship->shiptype
	glBegin(GL_QUADS);
		float w = ship->edge_length * 0.5;
		glTexCoord2f(0.0f, 0.0f); glVertex2f(-w, -w);
		glTexCoord2f(1.0f, 0.0f); glVertex2f( w, -w);
		glTexCoord2f(1.0f, 1.0f); glVertex2f( w,  w);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(-w,  w);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_ALPHA_TEST);
	glPopMatrix();
}

void laser_render(Laser *node) {
	glPushMatrix();
	glTranslated(node->pos[0],node->pos[1],node->pos[2]);
	glColor4fv(node->color);
	glLineWidth(node->linewidth);
	glBegin(GL_LINES);
		glVertex2f(0.0f, 0.0f);
		glVertex2f(0.0f, node->length);
	glEnd();
	glPopMatrix();
}

void laser_move_frame(Laser *node) {
	VecCopy(node->pos, node->lastpos);
	node->pos[0] += node->vel[0] * timeslice;
	node->pos[1] += node->vel[1] * timeslice;
}

void laser_check_collision(Laser *node) {
	//TODO: This only check against the player's ship. We actually need
	//      to check the player's ship for every enemy laser and every
	//      enemy ship for every player's laser.

	if(node->team == TEAM_REBELS) {
		//TODO: iterate over enemy ships, check hitboxes
	} else if(node->team == TEAM_EMPIRE) {
		if(player_ship.is_vulnerable) {
			//collision detection for raindrop on player_ship
			float d0 = node->pos[0] - player_ship.pos[0];
			float d1 = node->pos[1] - player_ship.pos[1];
			float distance = sqrt((d0*d0)+(d1*d1));
			if (distance <= player_ship.hitbox_radius) {
				//TODO: damage shields/health/explode here
				laser_list = g_list_remove(laser_list, node);
				free(node);
				return;
			}
		}
	}

	if (node->pos[1] <= -1.0f * node->length || node->pos[1] >= yres + node->length) {
		// Laser is above or below screen, remove it.
		laser_list = g_list_remove(laser_list, node);
		free(node);
	}
}

double VecNormalize(Vec vec) {
	Flt len, tlen;
	Flt xlen = vec[0];
	Flt ylen = vec[1];
	Flt zlen = vec[2];
	len = xlen*xlen + ylen*ylen + zlen*zlen;
	if (len == 0.0) {
		MakeVector(0.0,0.0,1.0,vec);
		return 1.0;
	}
	len = sqrt(len);
	tlen = 1.0 / len;
	vec[0] = xlen * tlen;
	vec[1] = ylen * tlen;
	vec[2] = zlen * tlen;
	return(len);
}

void laser_fire(Ship *ship) {
	// Creates and fires laser from the given ship

	Laser *node = (Laser *)malloc(sizeof(Laser));
	if (node == NULL) {
		Log("error allocating node.\n");
		exit(EXIT_FAILURE);
	}
	node->team = ship->team;

	node->pos[0] = ship->pos[0];
	node->pos[1] = ship->pos[1] + (0.5 * ship->edge_length * ship->team);
	VecCopy(node->pos, node->lastpos);

	// Set laser velocity (assuming constant for now)
	node->vel[0] = 0.0f;
	node->vel[1] = node->team * 500.0f;

	// Set color info (rgba 0.0->1.0?)
	node->color[0] = 1.0;
	node->color[1] = rnd() * 0.2f + 0.3f;
	node->color[2] = rnd() * 0.2f + 0.3f;
	node->color[3] = 1.0;

	node->linewidth = random(3)+2;
	node->maxvel[1] = (float)(node->linewidth*16);
	node->length = 20;

	// Add to global list of active lasers
	laser_list = g_list_prepend(laser_list, node);
}

void physics(void)
{
	// move player's ship based on most current input
	if(input_directions && player_ship.can_move) {
		VecCopy(player_ship.pos, player_ship.lastpos);
		if(input_directions & INPUT_LEFT)
		    if (player_ship.pos[0] > halfpad+ (player_ship.edge_length/2))
		    player_ship.pos[0] -= 10.0;
		if(input_directions & INPUT_RIGHT)
		   if (player_ship.pos[0] < xres-halfpad-(player_ship.edge_length/2))
		    player_ship.pos[0] += 10.0;
		if(input_directions & INPUT_UP)    player_ship.pos[1] += 10.0;
		if(input_directions & INPUT_DOWN)  player_ship.pos[1] -= 10.0;
	}

	//move rain droplets
	g_list_foreach(laser_list, (GFunc)laser_move_frame, NULL);
	
	//check rain droplets
	g_list_foreach(laser_list, (GFunc)laser_check_collision, NULL);
}

