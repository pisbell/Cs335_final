// CS 335 - Spring 2013 Final
// Program: STAR WARS Galaga
// Authors: Jacob Courtney, Patrick Isbell, Terry McIrvin
// Based on code by Gordon Griesel

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <glib.h>
#include <GL/glfw.h>

#include "constants.h"
#include "defs.h"

//These components can be turned on and off
#define USE_FONTS
#define USE_LOG

#ifdef USE_LOG
#include "log.h"
#endif //USE_LOG

#ifdef USE_FONTS
#include "fonts.h"
#endif //USE_FONTS

//macros
#define rnd() (((double)rand())/(double)RAND_MAX)
#define random(a) (rand()%a)

//constants
const float timeslice = 1.0f/60.0f;

//prototypes
int InitGL(GLvoid);
void checkkey(int k1, int k2);
void physics(void);
void render(void);
extern GLuint loadBMP(const char *imagepath);
extern GLuint tex_readgl_bmp(char *fileName, int alpha_channel);

//global variables and constants
int time_control = 2;
int input_directions = 0; //bitmask of arrow keys currently down, see INPUT_* macros

typedef struct t_laser {
	int team;
	int damage;
	int linewidth;
	Vec pos;
	Vec vel;
	float length;
	float color[4];
} Laser;

typedef struct t_ship {
	int team; 	     // TEAM_REBELS or TEAM_EMPIRE
	int shiptype;
	int health;
	int shields;
	int damage;
	int shotfreq;
	int is_vulnerable;   // Is ship vulnerable to hostile fire?
	int is_visible;      // Are we drawing the ship?
	int can_attack;      // Can the ship fire?
	int can_move; 	     // Can the ship move?
	Vec pos; 	     // Current position
	Vec dest; 	     // Where the ship "wants" to go
	float speed; 	     // Speed ship will move at (not current speed, and there is no acceleration)
	float edge_length;   // Edge size of square texture
	float hitbox_radius; // Radius of circular hitbox
} Ship;


GList *laser_list = NULL;
GList *enemies_list = NULL;
Ship *player_ship = NULL;
GLuint ship_textures[SHIP_COUNT];
GLuint background_texture;

void ship_render(Ship *ship);
Ship* ship_create(int shiptype, int team, int xpos, int ypos);

void laser_move_frame(Laser *node);
void laser_check_collision(Laser *node);
void laser_render(Laser *node);
void laser_fire(Ship *ship);

double shottimer     = 0;
int show_lasers      = 1;
int show_text        = 1;
int difficulty = DIFFICULTY_EASY;


int main(int argc, char **argv)
{
	int i, nmodes;
	int ship_select = 4;	// For player to choose ship
	GLFWvidmode glist[256];
	open_log_file();
	srand((unsigned int)time(NULL));
	
	glfwInit();
	srand(time(NULL));
	nmodes = glfwGetVideoModes(glist, 100);
	xres = glist[nmodes-1].Width;
	yres = glist[nmodes-1].Height;
	player_ship = ship_create(ship_select, TEAM_REBELS, xres/2, 100.0); 
	
	//TODO: On menu, when player selects play, present ship selection 
	//screen.  Ship choice sets ship_select variable.

	Ship *enemytmp = NULL;
	enemytmp = ship_create(SHIP_FIGHTER, TEAM_EMPIRE, xres/4, 5*yres/6);
	enemies_list = g_list_prepend(enemies_list, enemytmp);
	//enemytmp = ship_create(SHIP_BOMBER, TEAM_EMPIRE, 2*xres/4, 3*yres/6);
	//enemies_list = g_list_prepend(enemies_list, enemytmp);
	enemytmp = ship_create(SHIP_OPRESSOR, TEAM_EMPIRE, 3*xres/4, 5*yres/6);
	enemies_list = g_list_prepend(enemies_list, enemytmp);


	Log("setting window to: %i x %i\n",xres,yres);
	//if (!glfwOpenWindow(xres, yres, 0, 0, 0, 0, 0, 0, GLFW_WINDOW)) {
	if (!glfwOpenWindow(xres,yres,8,8,8,0,32,0,GLFW_FULLSCREEN)) {
		close_log_file();
		glfwTerminate();
		return 0;
	}

	glfwSetWindowTitle("STAR WARS Galaga");
	glfwSetWindowPos(0, 0);
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

	free(player_ship);
	g_list_foreach(enemies_list, (GFunc)free, NULL);
	g_list_free(enemies_list);
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

	if (k1 == ' ' && player_ship->can_attack) {
		laser_fire(player_ship);
		return;
	}

	if (k1 == '`') {
		if (--time_control < 1)
			time_control = 1;
		return;
	}
	if (k1 == '1') {
		if (++time_control > 3)
			time_control = 3;
		return;
	}
	if (k1 == '2') {
		//shrink the player_ship
		player_ship->edge_length *= (1.0 / 1.05);
		player_ship->hitbox_radius = player_ship->edge_length * 0.5;
		return;
	}
	if (k1 == '3') {
		//enlarge the player_ship
		player_ship->edge_length *= 1.05;
		player_ship->hitbox_radius = player_ship->edge_length * 0.5;
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
		player_ship->is_visible ^= 1;
		return;
	}
	if (k1 == '7') {
		player_ship->is_vulnerable ^= 1;
		return;
	}
	if (k1 == '8') {
		player_ship->can_attack ^= 1;
		return;
	}
	if (k1 == '9') {
		player_ship->can_move ^= 1;
		return;
	}
	if (k1 == '0') {
		int newtype = player_ship->shiptype += 1;
		if(newtype == SHIP_COUNT)
			newtype = 0;
		int newteam = player_ship->team;
		int xpos    = player_ship->pos[0];
		int ypos    = player_ship->pos[1];

		free(player_ship);
		player_ship = ship_create(newtype, newteam, xpos, ypos);
		return;
	}
	if (k1 == '-') {
		if(player_ship->team == TEAM_REBELS)
			player_ship->team = TEAM_EMPIRE;
		else
			player_ship->team = TEAM_REBELS;
		return;
	}
}

int InitGL(GLvoid)
{
	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_FOG);
	glDisable(GL_CULL_FACE);
	glClearColor(0.1f, 0.2f, 0.3f, 0.0f);
	
	background_texture = loadBMP("bg.bmp");
    ship_textures[SHIP_FIGHTER] = tex_readgl_bmp("Fighter.bmp", 1.0);
    ship_textures[SHIP_BOMBER] = tex_readgl_bmp("Bomber.bmp", 1.0);
    ship_textures[SHIP_INTERCEPTER] = tex_readgl_bmp("Interceptor.bmp", 1.0);
    ship_textures[SHIP_OPRESSOR] = tex_readgl_bmp("Oppressor.bmp", 1.0);
    ship_textures[SHIP_AWING] = tex_readgl_bmp("AWing.bmp", 1.0);
    ship_textures[SHIP_XWING] = tex_readgl_bmp("XWing.bmp", 1.0);
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

	ship_render(player_ship);
	g_list_foreach(enemies_list, (GFunc)ship_render, NULL);

	if (show_text) {
		//draw some text
		Rect r;
		r.left   = 10;
		r.bot    = 300;
		r.center = 0;
		ggprint16(&r, 16, 0x00cc6622, "< > Fire laser", NULL);
		ggprint16(&r, 16, 0x00aa00aa, "<`> Slow game", NULL);
		ggprint16(&r, 16, 0x00aa00aa, "<1> Speedup game", NULL);
		ggprint16(&r, 16, 0x0000aaaa, "<2> Shrink ship", NULL);
		ggprint16(&r, 16, 0x0000aaaa, "<3> Grow ship", NULL);
		ggprint16(&r, 16, 0x00aaaa00, "<4> Render lasers: %s",show_lasers==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<5> Render text: %s",show_text==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<6> Render ship: %s",player_ship->is_visible==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<7> Enable vulnerablility: %s",player_ship->is_vulnerable==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<8> Enable attack: %s",player_ship->can_attack==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<9> Enable movement: %s",player_ship->can_move==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<0> Cycle ships: %d",player_ship->shiptype);
		ggprint16(&r, 16, 0x00aaaa00, "<-> Cycle teams: %s",player_ship->team==TEAM_REBELS?"Rebels":"Empire");
		ggprint16(&r, 16, 0x00aaaa00, "    Player health: %d",player_ship->health);
		ggprint16(&r, 16, 0x00aaaa00, "    Player Shields: %d",player_ship->shields);
	}
}

void ship_render(Ship *ship)
{
	if(!ship->is_visible)
		return;

	glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
	glPushMatrix();
	glTranslatef(ship->pos[0], ship->pos[1], ship->pos[2]);
	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.0f);
	glBindTexture(GL_TEXTURE_2D, ship_textures[ship->shiptype]);
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
	node->pos[0] += node->vel[0] * timeslice;
	node->pos[1] += node->vel[1] * timeslice;
}

void ship_move_frame(Ship *ship) {
	if(!ship->can_move || ship->speed < 1)
		return;

	float xdist = ship->dest[0] - ship->pos[0];
	float ydist = ship->dest[1] - ship->pos[1];
	float xdist_squared = xdist*xdist;
	float ydist_squared = ydist*ydist;
	if(xdist_squared < 1 && ydist_squared < 1)
		return; // So close it's not worth doing

	float scale = sqrtf((ship->speed * ship->speed)/(xdist_squared + ydist_squared));
	ship->pos[0] += xdist*scale;
	ship->pos[1] += ydist*scale;
}

void ship_laser_check_collision(Ship *ship, Laser *laser) {
	// Checks laser and ship pair for collision.
	if(ship->is_vulnerable && laser->team != ship->team) {
		//collision detection for laser on ship
		float d0 = laser->pos[0] - ship->pos[0];
		float d1 = laser->pos[1] - ship->pos[1];
		float distance = sqrt((d0*d0)+(d1*d1));
		if (distance <= ship->hitbox_radius) {
			if (ship->shields > 0)
				ship->shields -= laser->damage;
			else
				ship->health -= laser->damage;

			if (ship->health <= 0) {
				//TODO: explode here, we have to handle results and deallocation differently for player vs enemy ships.
				//g_list_remove(ship)
				//free(ship);
			}
			
			laser_list = g_list_remove(laser_list, laser);
			free(laser);
			return;
		}
	}
}

void ship_enemy_attack_logic(Ship *ship) {
	if(!ship->can_attack)
		return;
	if(random(10000) < ship->shotfreq)
		laser_fire(ship);
}

void laser_check_collision(Laser *laser) {
	// Checks given laser for collisions with all applicable ships.

	if (laser->pos[1] <= -1.0f * laser->length || laser->pos[1] >= yres + laser->length) {
		// Laser is above or below screen, remove it.
		laser_list = g_list_remove(laser_list, laser);
		free(laser);
	}

	if(laser->team == TEAM_REBELS) {
		// Check every enemy ship against our laser.
		g_list_foreach(enemies_list, (GFunc)ship_laser_check_collision, laser);
	} else if(laser->team == TEAM_EMPIRE) {
		// Check our single ship against enemy laser.
		// (If we made another list for rebel ships, we could possibly do multiplayer.)
		ship_laser_check_collision(player_ship, laser);
	}
}

Ship* ship_create(int shiptype, int team, int xpos, int ypos) {
	Ship *ship = (Ship *)malloc(sizeof(Ship));
	if (ship == NULL) {
		Log("error allocating node.\n");
		exit(EXIT_FAILURE);
	}

	ship->team = team;
	ship->shiptype = shiptype;
	ship->health = statsHealth[shiptype][difficulty];
	ship->shields = statsShields[shiptype][difficulty];
	ship->damage = statsDamage[shiptype][difficulty];
	ship->shotfreq = statsShotfreq[shiptype][difficulty];
	ship->speed = statsSpeed[shiptype][difficulty];

	ship->is_vulnerable = 1;
	ship->is_visible = 1;
	ship->can_attack = 1;
	ship->can_move = 1;

	ship->pos[0] = ship->dest[0] = xpos;
	ship->pos[1] = ship->dest[1] = ypos;

	ship->edge_length = statsEdgeLength[shiptype];
	ship->hitbox_radius = statsHitboxRadius[shiptype];

	return ship;
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
	node->damage = ship->damage;
	// Set laser velocity (assuming constant for now)
	node->vel[0] = 0.0f;
	node->vel[1] = node->team * 200.0f;

	// Set color info (rgba 0.0->1.0)
	node->color[0] = node->team == TEAM_EMPIRE ? 1.0 : 0.0;
	node->color[1] = node->team == TEAM_EMPIRE ? 0.0 : 1.0;
	node->color[2] = 0.0;
	node->color[3] = 1.0;

	node->linewidth = 3;
	node->length = 20;

	// Add to global list of active lasers
	laser_list = g_list_prepend(laser_list, node);
}

void physics(void)
{
	// Move player's ship destination based on most current input
	// Target destination is arbitrary in magnitude but should never be
	// attainable in one frame. By having the concept of a "destination" for
	// each ship, AI will be much simpler and all ships can have the same
	// movement routine.

	VecCopy(player_ship->pos, player_ship->dest);
	if(input_directions & INPUT_LEFT) {
		player_ship->dest[0] -= 50.0;
		if(player_ship->dest[0] < halfpad+ (player_ship->edge_length/2))
			player_ship->dest[0] = halfpad+ (player_ship->edge_length/2);
	}
	if(input_directions & INPUT_RIGHT) {
		player_ship->dest[0] += 50.0;
	   if(player_ship->dest[0] > xres-halfpad-(player_ship->edge_length/2))
			player_ship->dest[0] =xres-halfpad-(player_ship->edge_length/2);
	}
	if(input_directions & INPUT_UP)    player_ship->dest[1] += 50.0;
	if(input_directions & INPUT_DOWN)  player_ship->dest[1] -= 50.0;

	ship_move_frame(player_ship); // Player movement
	g_list_foreach(enemies_list, (GFunc)ship_move_frame, NULL); // Enemy movement
	g_list_foreach(enemies_list, (GFunc)ship_enemy_attack_logic, NULL); // Enemy lasers/etc
	g_list_foreach(laser_list, (GFunc)laser_move_frame, NULL); // Update laser positions
	g_list_foreach(laser_list, (GFunc)laser_check_collision, NULL); // Run collision detection
}

