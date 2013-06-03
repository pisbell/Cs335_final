// CS 335 - Spring 2013 Final
// Program: STAR WARS Galaga
// Authors: Jacob Courtneay, Patrick Isbell, Terry McIrvin
// Based on code by Gordon Griesel

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <limits.h>
#include <glib.h>
#include <GL/glfw.h>

#include "constants.h"
#include "defs.h"
#include "fonts.h"

//macros
#define rnd() (((double)rand())/(double)RAND_MAX)
#define random(a) (rand()%a)

int InitGL(GLvoid);
void checkkey(int k1, int k2);
void physics(void);
void render(void);
void enemyFormation(int, int, int, int);// takes # of each enemies we want,
void player_ship_selection(void);
extern GLuint loadBMP(const char *imagepath);
extern GLuint tex_readgl_bmp(char *fileName, int alpha_channel);

//global variables and constants
const float timeslice = 1.0f/60.0f;
int time_control = 5;
int input_directions = 0; //bitmask of arrow keys currently down, see INPUT_* macros
int game_over = 0;	  //has player beaten game or run out of lives?
int victory = 0;	  //did player beat every level?	
typedef struct t_ship {
	int team; 	     // TEAM_REBELS or TEAM_EMPIRE
	int shiptype;
	int health;
	int shields;
	int damage;
	int shotfreq;
	int laser_width;
	int score;
	int is_vulnerable;   // Is ship vulnerable to hostile fire?
	int is_visible;      // Are we drawing the ship?
	int can_attack;      // Can the ship fire?
	int can_move; 	     // Can the ship move?
	Vec pos; 	     // Current position
	Vec dest; 	     // Where the ship "wants" to go
	float speed; 	     // Speed ship will move at (not current speed, and there is no acceleration)
	float edge_length;   // Edge size of square texture
	float hitbox_radius; // Radius of circular hitbox
	float dodge_radius;  // Extra radius from which ship will attempt to dodge lasers
	int dodge_info; // Sign is direction to dodge (-+ = left/right), magnitude is distance from laser, smaller is closer so more dangerous
	int death_animation; // Progress of explosion, -1 disables
} Ship;

typedef struct t_laser {
	int team;
	int damage;
	int linewidth;
	Vec pos;
	Vec vel;
	float length;
	float color[4];
	Ship* homing_target;
} Laser;

GList *laser_list   = NULL;
GList *enemies_list = NULL;
GList *turret_list  = NULL;
Ship *player_ship   = NULL;
Ship *enemytmp      = NULL;
Ship *targets[8]    = {NULL, NULL, NULL,NULL,NULL,NULL,NULL, NULL};
Ship *turrets[7]    = {NULL, NULL, NULL,NULL,NULL,NULL,NULL};
GLuint ship_textures[SHIP_COUNT];
GLuint background_texture;
GLuint explosion_textures[EXPLOSION_IMAGES];

void ship_render(Ship *ship);
Ship* ship_create(int shiptype, int team, int xpos, int ypos);

void level_spawn();
void laser_move_frame(Laser *node);
void laser_check_collision(Laser *node);
void laser_render(Laser *node);
void laser_fire(Ship *ship, Ship *homing_target);
void deathStar_physics();

double shottimer       = 0;
int show_lasers        = 1;
int show_text          = 1;
int difficulty         = DIFFICULTY_EASY;
int level              = 1;
int player_score       = 0;
int deathStar_charging = -10000;
int deathStar_cannon   = 0;
int ship_select        = SHIP_XWING; // For player to choose ship
int ship_selected      = 0; // 0 while ship is being selected
int demo_mode          = 0; // Game plays itself

int main(int argc, char **argv)
{
	int i, nmodes;
	GLFWvidmode glist[256];
	open_log_file();
	srand((unsigned int)time(NULL));
	
	glfwInit();
	srand(time(NULL));
	nmodes = glfwGetVideoModes(glist, 100);
	xres = glist[nmodes-1].Width;
	yres = glist[nmodes-1].Height;

	Log("setting window to: %i x %i\n",xres,yres);
	if (!glfwOpenWindow(xres,yres,8,8,8,0,32,0,GLFW_FULLSCREEN)) {
		close_log_file();
		glfwTerminate();
		return 0;
	}

	glfwSetWindowTitle("STAR WARS Galaga");
	glfwSetWindowPos(0, 0);
	glfwDisable( GLFW_MOUSE_CURSOR );
	InitGL();

	glShadeModel(GL_SMOOTH);
	//texture maps must be enabled to draw fonts
	glEnable(GL_TEXTURE_2D);
	initialize_fonts();

	player_ship_selection();
	player_ship = ship_create(ship_select, TEAM_REBELS, xres/2, 100.0); 
	difficulty--;
	level_spawn();
	glfwSetKeyCallback((GLFWkeyfun)(checkkey));
	while(1) {
		for (i=0; i<time_control; i++)
			physics();

		render();
		glfwSwapBuffers();
		if (glfwGetKey(GLFW_KEY_ESC)) break;
		if (!glfwGetWindowParam(GLFW_OPENED)) break;
		if (game_over) break;
	}

	glfwSetKeyCallback((GLFWkeyfun)NULL);
	glfwSetMousePosCallback((GLFWmouseposfun)NULL);
	close_log_file();
	glfwTerminate();

	cleanup_fonts();

	free(player_ship);
	g_list_foreach(enemies_list, (GFunc)free, NULL);
	g_list_free(enemies_list);
	g_list_foreach(laser_list, (GFunc)free, NULL);
	g_list_free(laser_list);

	char temp[512];
	sprintf(temp, "xdg-open 'http://www.terrymcirvin.com/SWG.php?score=%d\&vict=%d'", player_score, victory);
	system((char*)temp);

	return 0;
}

void checkkey(int k1, int k2)
{
	if (k2 == GLFW_PRESS) {
		// Set the flag for the given arrow key if pressed
		if(k1 == GLFW_KEY_LEFT) input_directions |= INPUT_LEFT;
		if(k1 == GLFW_KEY_RIGHT) input_directions |= INPUT_RIGHT;
		if(k1 == ' ') input_directions |= INPUT_FIRE;
	} else if (k2 == GLFW_RELEASE) {
		// Unset the flag for the given arrow key if released
		if(k1 == GLFW_KEY_LEFT) input_directions &= ~INPUT_LEFT;
		if(k1 == GLFW_KEY_RIGHT) input_directions &= ~INPUT_RIGHT;
		if(k1 == ' ') input_directions &= ~INPUT_FIRE;
		//don't process any other keys on a release
		return;
	}

	if (k1 == '`') {
		if (--time_control < 1)
			time_control = 1;
		return;
	}
	if (k1 == '1') {
		if (++time_control > 10)
			time_control = 10;
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
	if (k1 == '='){
	    if (enemies_list == NULL)
	    {
		enemyFormation(5, 10, 15, 20);
	    }
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

	int i;
	char filename[20];
	for(i = 1; i <= EXPLOSION_IMAGES; i++) {
		sprintf(filename, "explosions/%d.bmp", i);
		explosion_textures[i-1] = tex_readgl_bmp(filename, 1.0);
	}

	for(i=0 ; i < 7 ; i++) {
		turrets[i] = ship_create(SHIP_TURRET, TEAM_EMPIRE, (xres/4)+ (i*10)+45, yres - (abs(4-i)*5)-10);
		targets[i] = ship_create(SHIP_TURRET, TEAM_REBELS, 0, 100);
	}
    targets[7] = ship_create(SHIP_TURRET, TEAM_REBELS, (xres/4)+70, yres-150);

	return 1;
}

void render_bg(void) {
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
}

void render(GLvoid)
{
	glfwGetWindowSize(&xres, &yres);
	glViewport(halfpad, 0, xres-pad, yres);
	//clear color buffer
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode (GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glOrtho(0, xres, 0, yres, -1, 1);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	render_bg();

	if (show_lasers) {
		g_list_foreach(laser_list, (GFunc)laser_render, NULL);
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
		ggprint16(&r, 16, 0x00aaaa00, "<4> Render lasers; %s",show_lasers==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<5> Render text; %s",show_text==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<6> Render ship; %s",player_ship->is_visible==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<7> Enable vulnerablility; %s",player_ship->is_vulnerable==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<8> Enable attack; %s",player_ship->can_attack==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<9> Enable movement; %s",player_ship->can_move==1?"On":"Off");
		ggprint16(&r, 16, 0x00aaaa00, "<0> Cycle ships; %d",player_ship->shiptype);
		ggprint16(&r, 16, 0x00aaaa00, "<-> Cycle teams; %s",player_ship->team==TEAM_REBELS?"Rebels":"Empire");
		ggprint16(&r, 16, 0x00aaaa00, "    Player health; %d",player_ship->health);
		ggprint16(&r, 16, 0x00aaaa00, "    Player Shields; %d",player_ship->shields);
		ggprint16(&r, 16, 0x00aaaa00, "    Player Score;  %d", player_score);
	}
}

float* ship_calculate_shield_color(Ship *ship) {
	float* out_color = malloc(4*sizeof(float));
	out_color[3] = 0.3f;

	float full_color[3] =  {0.0f, 1.0f, 0.0f};
	float mid_color[3] =   {1.0f, 1.0f, 0.0f};
	float empty_color[3] = {1.0f, 0.0f, 0.0f};

	float halfshield = statsShields[ship->shiptype][difficulty]/2;
	if(ship->shields > halfshield) {
		float remaining = (ship->shields - halfshield) / halfshield;
		float gone = 1 - remaining;
		out_color[0] = full_color[0] * remaining + mid_color[0] * gone;
		out_color[1] = full_color[1] * remaining + mid_color[1] * gone;
		out_color[2] = full_color[2] * remaining + mid_color[2] * gone;
	} else {
		float remaining = ship->shields / halfshield;
		float gone = 1 - remaining;
		out_color[0] = mid_color[0] * remaining + empty_color[0] * gone;
		out_color[1] = mid_color[1] * remaining + empty_color[1] * gone;
		out_color[2] = mid_color[2] * remaining + empty_color[2] * gone;
	}

	return out_color;
}

void ship_render(Ship *ship)
{
	int circle_i;
	if(!ship->is_visible)
		return;

	if(ship->death_animation == -1) {
		// Normal render
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

		if(ship->shields > 0) {
			glEnable(GL_BLEND);
			glBegin(GL_TRIANGLE_FAN);
				float *colors = ship_calculate_shield_color(ship);
				glColor4f(colors[0], colors[1], colors[2], colors[3]);
				free(colors);
				glVertex2f(0, 0);

				int slices = 16;
				float slice_size = 2 * 3.1415926535 / slices;
				for(circle_i=0; circle_i<=slices; circle_i++) {
					glVertex2f(w*cosf(circle_i*slice_size), w*sinf(circle_i*slice_size));
				}
			glEnd();
			glDisable(GL_BLEND);
		}

	} else if(ship->death_animation < EXPLOSION_IMAGES*2) {
		// Render ship explosion (2 frames per image)
		glColor4f(1.0f, 1.0f, 1.0f, 0.8f);
		glPushMatrix();
		glTranslatef(ship->pos[0], ship->pos[1], ship->pos[2]);
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0.0f);
		glBindTexture(GL_TEXTURE_2D, explosion_textures[ship->death_animation/2]);
		glBegin(GL_QUADS);
			float w = ship->edge_length * 0.5;
			glTexCoord2f(0.0f, 0.0f); glVertex2f(-w, -w);
			glTexCoord2f(1.0f, 0.0f); glVertex2f( w, -w);
			glTexCoord2f(1.0f, 1.0f); glVertex2f( w,  w);
			glTexCoord2f(0.0f, 1.0f); glVertex2f(-w,  w);
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);
		glDisable(GL_ALPHA_TEST);
		ship->death_animation++;
	} else {
		// Explosion complete, destroy ship
		if(ship == player_ship) {
			// Actually free()ing ship segfaults the app, just leave it dormant
			//TODO; respawn (we need lives)
			// Game over condition (win or loss) will redirect to high scores
			// web page and be handled there.
			game_over = 1;


		} else {
			enemies_list = g_list_remove(enemies_list, ship);
			free(ship);
			if(enemies_list == NULL) {
				if(++level < LEVEL_COUNT) {
				    level_spawn();
					//enemyFormation( 5, 6, 14, 15); // takes # of each enemies we want,
				} else {
					victory = 1;
					game_over = 1;
				}
			}
		}
	}

	glPopMatrix();
}

void level_spawn()
{
    switch(level % 5)
    {
	case 0:
	    enemyFormation(10,15,15,20);
	    break;
	case 1:
	    difficulty++;
	    enemyFormation(0,0,10,15);
	    break;
	case 2:
	    enemyFormation(0,5,10,15);
	    break;
	case 3:
	    enemyFormation(2,10,10,15);
	    break;
	case 4:
	    enemyFormation(5,10,10,20);
	    break;
    }
}

void laser_render(Laser *node) {
	if(!node->linewidth)
		return;
	glPushMatrix();
	glTranslated(node->pos[0],node->pos[1],node->pos[2]);
	glColor4fv(node->color);
	glRotatef(180*atanf(node->vel[1]/node->vel[0])/3.141592, 0.0, 0.0, 1.0);
	if(node->linewidth <= 10) { // Small enough to use GL_LINES
		glLineWidth(node->linewidth);
		glBegin(GL_LINES);
			glVertex2f(0.0f, 0.0f);
			glVertex2f(node->length, 0.0f);
		glEnd();
	} else {
		glBegin(GL_QUADS);
			glVertex2f(0, node->linewidth/2);
			glVertex2f(0, -1 * node->linewidth/2);
			glVertex2f(node->length, -1 * node->linewidth/2);
			glVertex2f(node->length, node->linewidth/2);
		glEnd();
	}
	glPopMatrix();
}

void enemyFormation( int OppressorNum, int InterceptorNum,int BomberNum, int FighterNum)
{
    int playspace = xres-pad;
    int MaxEnemyScreen = (playspace/80 )-2;
    int epPoint = yres - 100;
    int EnemyRow = 0;
    int topEnemies = 0;
    int spread = 0;
    int k = 0;
    int j = 0;
    int i = 0;

    if ( OppressorNum > 0)
    {
	EnemyRow = OppressorNum/MaxEnemyScreen;
	topEnemies = OppressorNum % MaxEnemyScreen;
	if (topEnemies != 0)
	{
	    spread = (playspace-150)/topEnemies;
	    for( k = -topEnemies/2; k < topEnemies/2; k++)
	    {
		enemytmp = ship_create(SHIP_OPRESSOR, TEAM_EMPIRE, (xres/2)+ (k*spread), epPoint);
		enemies_list = g_list_prepend(enemies_list, enemytmp); 

	    }
	    epPoint = epPoint - 76;

	}
	for(j = 0; j < EnemyRow; j++)
	{
	    for (i = -MaxEnemyScreen/2; i < MaxEnemyScreen/2; i++)
	    {
		enemytmp = ship_create(SHIP_OPRESSOR, TEAM_EMPIRE, (xres/2)+(i*80), epPoint);
		enemies_list = g_list_prepend(enemies_list, enemytmp); 
	    }
	    epPoint = epPoint - (76);


	}
    }

    if ( InterceptorNum > 0)
    {
	EnemyRow = InterceptorNum/MaxEnemyScreen;
	topEnemies = InterceptorNum % MaxEnemyScreen;
	if (topEnemies != 0)
	{
	    spread = (playspace-150)/topEnemies;
	    for(k = -topEnemies/2; k < topEnemies/2; k++)
	    {
		enemytmp = ship_create(SHIP_INTERCEPTER, TEAM_EMPIRE, (xres/2)+ (k*spread), epPoint);
		enemies_list = g_list_prepend(enemies_list, enemytmp); 

	    }
	    epPoint = epPoint - 76;
	}
	for(j=0; j < EnemyRow; j++)
	{
	    for (i = -MaxEnemyScreen/2; i < MaxEnemyScreen/2; i++)
	    {
		enemytmp = ship_create(SHIP_INTERCEPTER, TEAM_EMPIRE, (xres/2)+(i*80), epPoint);
		enemies_list = g_list_prepend(enemies_list, enemytmp); 
	    }
	    epPoint = epPoint - (76);


	}
    }
    if (BomberNum > 0)
    {
	EnemyRow = BomberNum/MaxEnemyScreen;
	topEnemies = BomberNum % MaxEnemyScreen;
	if (topEnemies != 0)
	{
	    spread = (playspace-150)/topEnemies;
	    for(k = -topEnemies/2; k < topEnemies/2; k++)
	    {
		enemytmp = ship_create(SHIP_BOMBER, TEAM_EMPIRE, (xres/2)+ (k*spread), epPoint);
		enemies_list = g_list_prepend(enemies_list, enemytmp); 

	    }
	    epPoint = epPoint - 76;
	}
	for(j=0; j < EnemyRow; j++)
	{
	    for ( i = -MaxEnemyScreen/2; i < MaxEnemyScreen/2; i++)
	    {
		enemytmp = ship_create(SHIP_BOMBER, TEAM_EMPIRE, (xres/2)+(i*80), epPoint);
		enemies_list = g_list_prepend(enemies_list, enemytmp); 
	    }
	    epPoint = epPoint - (76);


	}
    }
    if ( FighterNum > 0)
    {
	EnemyRow = FighterNum/MaxEnemyScreen;
	topEnemies = FighterNum % MaxEnemyScreen;
	if (topEnemies != 0)
	{
	    spread = (playspace-150)/topEnemies;
	    for(k = -topEnemies/2; k < topEnemies/2; k++)
	    {
		enemytmp = ship_create(SHIP_FIGHTER, TEAM_EMPIRE, (xres/2)+ (k*spread)+(150), epPoint);
		enemies_list = g_list_prepend(enemies_list, enemytmp); 

	    }
	    epPoint = epPoint - 76;
	}
	for(j = 0; j < EnemyRow; j++)
	{
	    for (i = -MaxEnemyScreen/2; i < MaxEnemyScreen/2; i++)
	    {
		enemytmp = ship_create(SHIP_FIGHTER, TEAM_EMPIRE, (xres/2)+(i*80), epPoint);
		enemies_list = g_list_prepend(enemies_list, enemytmp); 
	    }
	    epPoint = epPoint - (76);


	}

    }
}

void laser_move_frame(Laser *node) {
	if(node->homing_target != NULL) {
		float total_vel = sqrtf(node->vel[0]*node->vel[0] + node->vel[1]*node->vel[1]);
		float dx = node->homing_target->pos[0] - node->pos[0];
		float dy = node->homing_target->pos[1] - node->pos[1];
		float scale = total_vel/sqrtf(dx*dx + dy*dy);

		if((dy > 0 && node->team == TEAM_REBELS) || (dy < 0 && node->team == TEAM_EMPIRE)) {
			node->vel[0] = scale * dx;
			node->vel[1] = scale * dy;

			if(fabs(node->vel[1]) < fabs(node->vel[0])) {
				node->vel[0] = sqrtf(total_vel*total_vel/2) * (dx > 0 ? 1 : -1);
				node->vel[1] = sqrtf(total_vel*total_vel/2) * node->team;
			}
		}
	}

	node->pos[0] += node->vel[0] * timeslice;
	node->pos[1] += node->vel[1] * timeslice;
}

void ship_move_frame(Ship *ship) {
	if(!ship->can_move || ship->speed == 0)
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

void ship_explode(Ship *ship) {
	ship->is_vulnerable = 0;
	ship->is_visible = 1;
	ship->can_attack = 0;
	ship->can_move = 0;
	ship->death_animation = 0; // Begins death animation

}

void ship_laser_check_collision(Ship *ship, Laser **dplaser) {
	// Checks laser and ship pair for collision.

	// g_list_foreach just passes the same laser pointer for every ship. If a
	// collision occurs, the laser must be freed. When a laser hits two ships
	// at once, a double free error occurs if we simply pass a laser pointer,
	// as there's no way to tell the struct has been freed within ship_laser_check_collision.
	// By passing a double pointer, we can mark the laser pointer as NULL when
	// it has been freed, allowing later iterations to see that the laser has
	// been destroyed.
	if(*dplaser == NULL)
		return;

	if((*dplaser)->linewidth == beamwidth) // Death star beam doesn't collide (would look bad)
		return;

	Laser *laser = *dplaser;
	if(laser->team != ship->team) {
		//collision detection for laser on ship
		float d0 = laser->pos[0] - ship->pos[0];
		float d1 = laser->pos[1] - ship->pos[1];
		float distance = sqrt((d0*d0)+(d1*d1));
		if (ship->is_vulnerable && distance <= ship->hitbox_radius) {
			// Ship is hit
			if (ship->shields > 0)
				ship->shields -= laser->damage;
			else
				ship->health -= laser->damage;

			if (ship->health <= 0) {
				ship_explode(ship);
				//enemies_list = g_list_remove(enemies_list, ship);
				//free(ship);
				player_score += ship->score;
			}
			
			laser_list = g_list_remove(laser_list, laser);
			free(laser);
			*dplaser = NULL;
		} else if(distance <= ship->hitbox_radius + ship->dodge_radius && ship->pos[1] > laser->pos[1]) {
			// Ship not hit, but close enough to trigger evasive maneuvers
			// Ship will evade closest laser within its dodge radius.
			int dodgedir = d0 > 0 ? -1 : 1; // Dodge in opposite direction of laser
			if(ship->dodge_info == 0 || distance < ship->dodge_info) // Only go this direction if it's the closest threat
				ship->dodge_info = dodgedir * distance;
		}
	}
}

void ship_enemy_attack_logic(Ship *ship) {
    if(!ship->can_attack)
	return;

	if(ship->shiptype == SHIP_BOMBER && difficulty >= DIFFICULTY_MEDIUM) {
		if(abs(ship->pos[0] - player_ship->pos[0]) <= 100 && random(1000) < ship->shotfreq)
			laser_fire(ship, NULL);
		return;
	}

	if(random(100000) < ship->shotfreq) {
		if(ship->shiptype == SHIP_INTERCEPTER && difficulty >= DIFFICULTY_HARD)
			laser_fire(ship, player_ship);
		else
			laser_fire(ship, NULL);
	}
}

void ship_enemy_move_logic(Ship *ship) {
	ship->dest[1] = ship->pos[1];
	int direction = (ship->dest[0] > ship->pos[0]) ? 1 : -1;
	int dist = fabs(ship->dest[0] - ship->pos[0]);

	if(ship->shiptype == SHIP_OPRESSOR) {
		if(dist < 5)
			ship->dest[0] = random(500) + player_ship->pos[0]-250;
		if (ship->dest[0] < halfpad)
			ship->dest[0] = halfpad + ship->edge_length/2;
		if (ship->dest[0] > xres - halfpad)
			ship->dest[0] = xres - halfpad - ship->edge_length/2;
		return;
	}


	if(ship->dodge_info != 0) {
		if(dist < ship->speed) {
			// dodge complete, flip direction back
			ship->dest[0] = ship->dodge_info > 0 ? 0 : xres+100;
			ship->dodge_info = 0;
			dist = ship->speed; // To throw off default direction at bottom of function
		} else if(direction == 1 && ship->dodge_info < 0) {
			// dodge left
			ship->dest[0] = ship->pos[0] - ship->edge_length;
		} else if(direction == -1 && ship->dodge_info > 0) {
			// dodge right
			ship->dest[0] = ship->pos[0] + ship->edge_length;
		}
	}

	if(ship->pos[0] <= halfpad+ (ship->edge_length/2)) {
		ship->dest[0] = xres+100;
		ship->dodge_info = 0;
	} else if(ship->pos[0] >= xres-halfpad-(ship->edge_length/2)) {
		ship->dest[0] = 0;
		ship->dodge_info = 0;
	} else if(dist < ship->speed) {
		ship->dest[0] = 0;
		ship->dodge_info = 0;
	}
}

void deathStar_physics() {
	// deathStar_charging begins at a negative value (chargemin) and is incremented continually, meanwhile the deathStar is dormant.
	// deathStar_charging triggers the "charging" stage when it hits 0. it is bumped to a large positive value (chargemax).
	// deathStar_charging causes lasers to fire into the center of the deathStar when positive, a "charging" animation. 
	// it is decremented during this time.
	// after it hits 0 again, the charging cycle is reset and the deathStar begins firing, using deathStar_cannon as a timer.
	int i;

	if (deathStar_charging < 0) {
		// Remain dormant another frame
		deathStar_charging++;
	}

	if (deathStar_charging == 0) {
		// Prepare to charge, always continues on to next block
	    for (i=0; i < 7; i++)
			turrets[i]->laser_width = 4;
		targets[7]->health = INT_MAX;
		targets[7]->is_vulnerable = 1;
		deathStar_charging = chargemax;
	}

	if (deathStar_charging > 0) {
		deathStar_charging--;
		// Currently charging laser
		if (deathStar_charging > 200){
		    for (i=0; i < 7; i++)
			if(random(1000) < turrets[i]->shotfreq)
			    laser_fire(turrets[i], targets[7]);
		}
		if (deathStar_charging == 1) {
			// Done charging
			int playspace = random((xres-pad-70));
			for(i=0; i< 7; i++) {
				turrets[i]->laser_width = 0;
				targets[i]->pos[0] = playspace+(i*10)+halfpad;
			}
			turrets[3]->laser_width = beamwidth;
			targets[7]->is_vulnerable = 0;
			deathStar_charging = chargemin; // Wait before beginning to charge again
			deathStar_cannon = cannonmax; // Begin firing
	    }
	}

	if(deathStar_cannon > 0) {
		deathStar_cannon--;
	    for(i=0; i < 7; i++)
			if(random(10) < 4)
				laser_fire(turrets[i], targets[i]);
	}

}

void laser_check_collision(Laser *laser) {
	// Checks given laser for collisions with all applicable ships.

	if (laser->pos[1] <= -1.0f * laser->length || laser->pos[1] >= yres + laser->length) {
		// Laser is above or below screen, remove it.
		laser_list = g_list_remove(laser_list, laser);
		free(laser);
	}

	Laser **dplaser = &laser; // See ship_laser_check_collision for why we're doing this.

	if(laser->team == TEAM_REBELS) {
		// Check every enemy ship against our laser.
		g_list_foreach(enemies_list, (GFunc)ship_laser_check_collision, dplaser);
	} else if(laser->team == TEAM_EMPIRE) {
		// Check our single ship against enemy laser.
		// (If we made another list for rebel ships, we could possibly do multiplayer.)
		ship_laser_check_collision(player_ship, dplaser);
		ship_laser_check_collision(targets[7], dplaser);
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
	ship->laser_width = statsLaserWidth[shiptype][difficulty];
	ship->speed = statsSpeed[shiptype][difficulty];
	ship->score = statsScore[shiptype][difficulty];

	if (shiptype == SHIP_TURRET)
	{
	    ship->is_vulnerable  = 0;
	    ship->is_visible     = 0;
	    ship->can_move       = 0;
	    ship->can_attack     = 1;
	    ship->death_animation = -1;
	}else{
	    ship->is_vulnerable   = 1;
	    ship->is_visible      = 1;
	    ship->can_attack      = 1;
	    ship->can_move        = 1;
	    ship->death_animation = -1;
	}

	ship->pos[0] = ship->dest[0] = xpos;
	ship->pos[1] = ship->dest[1] = ypos;

	ship->edge_length = statsEdgeLength[shiptype];
	ship->hitbox_radius = statsHitboxRadius[shiptype];
	ship->dodge_radius = statsDodgeRadius[shiptype][difficulty];
	ship->dodge_info = 0;

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

void laser_fire(Ship *ship, Ship *homing_target) {
	// Creates and fires laser from the given ship
	if(!ship->can_attack)
		return;

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
	node->vel[1] = node->team * 100.0f;

	// Set color info (rgba 0.0->1.0)
	node->color[0] = node->team == TEAM_EMPIRE ? 1.0 : 0.0;
	node->color[1] = node->team == TEAM_EMPIRE ? 0.0 : 1.0;
	node->color[2] = 0.0;
	node->color[3] = 1.0;

	node->linewidth = ship->laser_width;
	node->length = 20;

	if(homing_target == player_ship) {
		// Set color info (rgba 0.0->1.0)
		node->color[0] = 1.0;
		node->color[1] = 0.75;
		node->color[2] = 0.0;
		node->color[3] = 1.0;
	}
	if (homing_target != player_ship && homing_target != NULL) {
	    node->color[0] = 0.0;
	    node->color[1] = 1.0;
	    node->color[2] = 0.0;
	    node->color[3] = 1.0;
	}

	node->homing_target = homing_target;

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

	if(demo_mode) {
		player_ship->is_vulnerable = 0;
		player_ship->shields = 0;
		ship_enemy_move_logic(player_ship);
		if(random(200) < player_ship->shotfreq)
			laser_fire(player_ship, NULL);
	} else {
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
		if(input_directions & INPUT_FIRE && random(200) < player_ship->shotfreq)
			laser_fire(player_ship, NULL);
	}

	deathStar_physics();
	ship_move_frame(player_ship); // Player movement
	g_list_foreach(laser_list, (GFunc)laser_move_frame, NULL); // Update laser positions
	g_list_foreach(laser_list, (GFunc)laser_check_collision, NULL); // Run collision detection
	g_list_foreach(enemies_list, (GFunc)ship_enemy_move_logic, NULL); // Determine enemy destinations
	g_list_foreach(enemies_list, (GFunc)ship_move_frame, NULL); // Enemy movement
	g_list_foreach(enemies_list, (GFunc)ship_enemy_attack_logic, NULL); // Enemy lasers/etc
}

void player_ship_selection_key(int k1, int k2) {
	if(k2 == GLFW_RELEASE)
		return;
	if(k1 == GLFW_KEY_LEFT) {
		ship_select = SHIP_XWING;
	}
	if(k1 == GLFW_KEY_RIGHT) {
		ship_select = SHIP_AWING;
	}
	if(k1 == GLFW_KEY_ENTER || k1 == ' ') {
		ship_selected = 1;
	}
	if(k1 == 'D') {
		demo_mode = 1;
		ship_selected = 1;
	}
}

void player_ship_selection(void) {
	glfwSetKeyCallback((GLFWkeyfun)(player_ship_selection_key));
	Ship* xwing = ship_create(SHIP_XWING, TEAM_REBELS, 200, 200);
	Ship* awing = ship_create(SHIP_AWING, TEAM_REBELS, 500, 200);
	Rect r;
	r.center = 1;

	while(!ship_selected) {
		glfwGetWindowSize(&xres, &yres);
		glViewport(halfpad, 0, xres-pad, yres);
		//clear color buffer
		glClear(GL_COLOR_BUFFER_BIT);
		glMatrixMode (GL_PROJECTION); glLoadIdentity();
		glMatrixMode(GL_MODELVIEW); glLoadIdentity();
		glOrtho(0, xres, 0, yres, -1, 1);
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_BLEND);

		xwing->pos[1] = awing->pos[1] = yres/2;
		xwing->pos[0] = 2*xres/5;
		awing->pos[0] = 3*xres/5;
		xwing->shields = 0;
		awing->shields = 0;
		if(ship_select == SHIP_XWING)
			xwing->shields = 1000;
		if(ship_select == SHIP_AWING)
			awing->shields = 1000;

		render_bg();
		ship_render(xwing);
		ship_render(awing);

		r.left   = (xwing->pos[0]+awing->pos[0])/2;
		r.bot    = 2*yres/3;
		r.center = 1;
		ggprint16(&r, 16, 0x000000ff, "Choose your ship", NULL);
		r.bot    = 2*yres/3-24;
		ggprint16(&r, 16, 0x000000ff, "Press d for demo", NULL);
		r.left   = xwing->pos[0];
		r.bot    = xwing->pos[1] - 100;
		ggprint16(&r, 16, 0x000000ff, "X-Wing", NULL);
		r.left   = awing->pos[0];
		r.bot    = awing->pos[1] - 100;
		ggprint16(&r, 16, 0x000000ff, "A-Wing", NULL);

		glfwSwapBuffers();
		if (glfwGetKey(GLFW_KEY_ESC)) break;
		if (!glfwGetWindowParam(GLFW_OPENED)) break;
	}
}
