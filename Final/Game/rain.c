//cs335 - Spring 2013
//program: Rain drops
//author:  Gordon Griesel

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <glib.h>

//These components can be turned on and off
#define USE_FONTS
#define USE_LOG

// Designates teams for ships/weapons, to prevent friendly fire.
#define TEAM_EMPIRE (-1)
#define TEAM_REBELS 1

#ifdef USE_LOG
#include "log.h"
#endif //USE_LOG
#include "defs.h"
#include <GL/glfw.h>

#ifdef USE_FONTS
#include "fonts.h"
#endif //USE_FONTS

//macros
#define rnd() (((double)rand())/(double)RAND_MAX)
#define random(a) (rand()%a)

//constants
const float timeslice = 1.0f/60.0f;
const float gravity = -0.2f;
#define ALPHA 1

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
int xres=800;
int yres=600;
int pad; // For empty space on sides of screen
int halfpad;


//ship global declarations 
int tFighter      = 1;
int tBomber       = 2;
int tIntercepter  = 3;
int tOpressor     = 4;
int aWing         = 5;
int xWing         = 6;
int Diffculty     = 0;


int statsHealth  [6][3] = { 
    { 100, 200, 300},
    {  50,  75, 150},
    { 200, 300, 400},
    { 300, 400, 500},
    {1000, 800, 650},
    { 800, 750, 600}
};
int statsShields [6][3] = {
    { 100, 200, 300},
    { 150, 225, 350},
    {  50,  75, 150},
    { 100, 200, 300},
    {1000, 700, 500},
    { 750, 500, 350}
};
int statsDamage  [6][3] = {
    { 100, 175, 225},
    { 150, 250, 300},
    {   2,   6,   9},
    { 400, 500, 600},
    {  75,  75,  75},
    { 100, 100, 100}
};
int statsShotfreq[6][3] = {
    {  15,  25,  40},
    {   5,  15,  25},
    {  40,  70,  95},
    {   5,  10,  15},
    {   2,   2,   2},
    {   5,   5,   5}
};

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
	int vulnerable; // Is ship vulnerable to hostile fire?
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
int show_umbrella  = 0;
void draw_umbrella(void);

void laser_move_frame(Laser *node);
void laser_check_collision(Laser *node);
void laser_render(Laser *node);

int show_rain      = 1;
int show_text      = 0;


int main(int argc, char **argv)
{
	pad = 500;
	halfpad = pad/2;
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
	glfwEnable( GLFW_KEY_REPEAT );
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
	static int shift=0;
	if (k2 == GLFW_PRESS) {
		//some key is being pressed now
		if (k1 == GLFW_KEY_LSHIFT || k1 == GLFW_KEY_RSHIFT) {
			//it is a shift key
			shift=1;
			return;
		}
	}
	if (k2 == GLFW_RELEASE) {
		if (k1 == GLFW_KEY_LSHIFT || k1 == GLFW_KEY_RSHIFT) {
			//the shift key was released
			shift=0;
		}
		//don't process any other keys on a release
		return;
	}


	if (k1 == 'R') {
		show_rain ^= 1;
		return;
	}
	if (k1 == 'T') {
		show_text ^= 1;
		return;
	}
	if (k1 == 'U') {
		show_umbrella ^= 1;
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
	if (show_umbrella) {
		if (k1 == 'W') {
			if (shift) {
				//shrink the player_ship
				player_ship.edge_length *= (1.0 / 1.05);
			} else {
				//enlarge the player_ship
				player_ship.edge_length *= 1.05;
			}
			//hit box is circle inscribed in texture edges
			player_ship.hitbox_radius = player_ship.edge_length * 0.5;
			return;
		}
		if (k1 == GLFW_KEY_LEFT)  {
			VecCopy(player_ship.pos, player_ship.lastpos);
			player_ship.pos[0] -= 10.0;
		}
		if (k1 == GLFW_KEY_RIGHT)  {
			VecCopy(player_ship.pos, player_ship.lastpos);
			player_ship.pos[0] += 10.0;
		}
		if (k1 == GLFW_KEY_UP)  {
			VecCopy(player_ship.pos, player_ship.lastpos);
			player_ship.pos[1] += 10.0;
		}
		if (k1 == GLFW_KEY_DOWN)  {
			VecCopy(player_ship.pos, player_ship.lastpos);
			player_ship.pos[1] -= 10.0;
		}
		if (k1 == ' ')  {
			laser_fire(&player_ship);
		}
	}
}

void init(void)
{
	player_ship.pos[0] = 200.0;
	player_ship.pos[1] = 400.0;
	VecCopy(player_ship.pos, player_ship.lastpos);
	player_ship.edge_length = 300.0;
	player_ship.hitbox_radius = player_ship.edge_length * 0.5;
	player_ship.team = TEAM_REBELS;
	player_ship.vulnerable = 1;
}

int InitGL(GLvoid)
{
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FOG);
	glDisable(GL_CULL_FACE);
	glClearColor(0.1f, 0.2f, 0.3f, 0.0f);
	
	background_texture = loadBMP("bg.bmp");
    umbrella_texture = tex_readgl_bmp("XWing.bmp", ALPHA);
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


	if (show_rain) {
		g_list_foreach(laser_list, (GFunc)laser_render, NULL);
		glLineWidth(1);
	}
	glDisable(GL_BLEND);

	if (show_umbrella)
		draw_umbrella();

	if (show_text) {
		//draw some text
		Rect r;
		r.left   = 10;
		r.bot    = 120;
		r.center = 0;
		ggprint12(&r, 16, 0x00cc6622, "<R> Rain: %s",show_rain==1?"On":"Off");
		ggprint12(&r, 16, 0x00cc6622, "<U> Umbrella: %s",show_umbrella==1?"On":"Off");
		//ggprint12(&r, 16, 0x00aaaa00, "total drops: %i",totrain);
	}
}

void draw_umbrella(void)
{
	glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
	glPushMatrix();
	glTranslatef(player_ship.pos[0],player_ship.pos[1],player_ship.pos[2]);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glBindTexture(GL_TEXTURE_2D, umbrella_texture);
	glBegin(GL_QUADS);
		float w = player_ship.edge_length * 0.5;
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
		if(player_ship.vulnerable) {
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
	node->vel[1] = node->team * 75.0f;

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
	//Log("physics()...\n");
	if (random(100) < 10) {
		//laser_fire(&player_ship);
	}

	//move rain droplets
	g_list_foreach(laser_list, (GFunc)laser_move_frame, NULL);
	
	//check rain droplets
	g_list_foreach(laser_list, (GFunc)laser_check_collision, NULL);

}

