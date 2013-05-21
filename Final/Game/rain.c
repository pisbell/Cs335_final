//cs335 - Spring 2013
//program: Rain drops
//author:  Gordon Griesel

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

//These components can be turned on and off
#define USE_FONTS
#define USE_UMBRELLA
#define USE_LOG


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
void draw_raindrops(void);
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
	int linewidth;
	Vec pos;
	Vec lastpos;
	Vec vel;
	Vec maxvel;
	float length;
	float color[4];
	struct t_laser *prev;
	struct t_laser *next;
} Laser;

typedef struct ships {
    int shiptype;
    int health;
    int shields;
    int damage;
    int shotfreq;
    Vec vel;
    Vec maxvel;
    Vec pos;
    Vec lastpos;
	float width;
	float width2;
	float radius;
} Ship;


Laser *ihead=NULL;
void delete_rain(Laser *node);
void cleanup_raindrops(void);

#ifdef USE_UMBRELLA
Ship player_ship;
GLuint umbrella_texture;
GLuint background_texture;
int show_umbrella  = 0;
void draw_umbrella(void);
#endif //USE_UMBRELLA

int totrain=0;
int maxrain=0;
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
	printf("totrain: %i  maxrain: %i\n",totrain,maxrain);
	glfwTerminate();
	#ifdef USE_FONTS
	cleanup_fonts();
	#endif //USE_FONTS

	cleanup_raindrops();
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
	#ifdef USE_UMBRELLA
	if (k1 == 'U') {
		show_umbrella ^= 1;
		return;
	}
	#endif //USE_UMBRELLA
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
	#ifdef USE_UMBRELLA
	if (show_umbrella) {
		if (k1 == 'W') {
			if (shift) {
				//shrink the player_ship
				player_ship.width *= (1.0 / 1.05);
			} else {
				//enlarge the player_ship
				player_ship.width *= 1.05;
			}
			//half the width
			player_ship.width2 = player_ship.width * 0.5;
			player_ship.radius = (float)player_ship.width2;
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
	}
	#endif //USE_UMBRELLA
}

void init(void)
{
	#ifdef USE_UMBRELLA
	player_ship.pos[0] = 200.0;
	player_ship.pos[1] = 400.0;
	VecCopy(player_ship.pos, player_ship.lastpos);
	player_ship.width = 300.0;
	player_ship.width2 = player_ship.width * 0.5;
	player_ship.radius = (float)player_ship.width2;
	#endif //USE_UMBRELLA
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


	if (show_rain)
		draw_raindrops();
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
		//#ifdef USE_UMBRELLA
		ggprint12(&r, 16, 0x00cc6622, "<U> Umbrella: %s",show_umbrella==1?"On":"Off");
		//#endif //USE_UMBRELLA
		ggprint12(&r, 16, 0x00aaaa00, "total drops: %i",totrain);
		ggprint12(&r, 16, 0x00aaaa00, "max drops: %i\n",maxrain);
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
		float w = player_ship.width2;
		glTexCoord2f(0.0f, 0.0f); glVertex2f(-w, -w);
		glTexCoord2f(1.0f, 0.0f); glVertex2f( w, -w);
		glTexCoord2f(1.0f, 1.0f); glVertex2f( w,  w);
		glTexCoord2f(0.0f, 1.0f); glVertex2f(-w,  w);
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	glDisable(GL_ALPHA_TEST);
	glPopMatrix();
}

void draw_raindrops(void)
{
	if (ihead) {
		Laser *node = ihead;
		while(node) {
			glPushMatrix();
			glTranslated(node->pos[0],node->pos[1],node->pos[2]);
			glColor4fv(node->color);
			glLineWidth(node->linewidth);
			glBegin(GL_LINES);
				glVertex2f(0.0f, 0.0f);
				glVertex2f(0.0f, node->length);
			glEnd();
			glPopMatrix();
			//
			if (node->next==NULL) break;
			node = node->next;
		}
	}
	glLineWidth(1);
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

void physics(void)
{
	//Log("physics()...\n");
	if (random(100) < 10) {
		//create new rain drops...
		Laser *node = (Laser *)malloc(sizeof(Laser));
		if (node == NULL) {
			Log("error allocating node.\n");
			exit(EXIT_FAILURE);
		}
		node->prev = node->next = NULL;
		node->pos[0] = (rnd() * (float)(xres-pad-600));
		node->pos[0] += halfpad+100;
		node->pos[1] = (float)yres - 100 - rnd()*50;//rnd() * 100.0f + (float)yres;
		VecCopy(node->pos, node->lastpos);
		node->vel[0] = 
		node->vel[1] = 0.0f;
		node->color[0] = 1.0;//rnd() * 0.2f + 0.8f;
		node->color[1] = rnd() * 0.2f + 0.3f;
		node->color[2] = rnd() * 0.2f + 0.3f;
		node->color[3] = 1.0;//rnd() * 0.5f + 0.3f; //alpha
		node->linewidth = random(3)+2;
		//larger linewidth = faster speed
		node->maxvel[1] = (float)(node->linewidth*16);
		node->length = 20;//node->maxvel[1] * 0.2f + rnd();
		//put raindrop into linked list
		node->next = ihead;
		if (ihead) ihead->prev = node;
		ihead = node;
		++totrain;
	}

	//move rain droplets
	if (ihead) {
		Laser *node = ihead;
		while(node) {
			node->vel[1] = -75.0;
			VecCopy(node->pos, node->lastpos);
			node->pos[0] += node->vel[0] * timeslice;
			node->pos[1] += node->vel[1] * timeslice;
			if (fabs(node->vel[1]) > node->maxvel[1])
				node->vel[1] *= 0.96;
			node->vel[0] *= 0.999;
			//
			if (node->next == NULL) break;
			node = node->next;
		}
	}
	
	//check rain droplets
	if (ihead) {
		int r,n=0;
		Laser *savenode;
		Laser *node = ihead;
		while(node) {
			n++;
			//#ifdef USE_UMBRELLA
			if (show_umbrella) {
				//collision detection for raindrop on player_ship
				float d0 = node->pos[0] - player_ship.pos[0];
				float d1 = node->pos[1] - player_ship.pos[1];
				float distance = sqrt((d0*d0)+(d1*d1));
				if (distance <= player_ship.radius && node->pos[1] > player_ship.pos[1]) {
					if (node->linewidth > 1) {
						savenode = node->next;
						delete_rain(node);
						node = savenode;
						if (node == NULL) break;
					}
				}
				//VecCopy(player_ship.pos, player_ship.lastpos);
			}
		
			if (node->pos[1] < -20.0f) {
				//rain drop is below the visible area
				savenode = node->next;
				delete_rain(node);
				node = savenode;
				if (node == NULL) break;
			}
			if (node->next == NULL) break;
			node = node->next;
		}
		if (maxrain < n)
			maxrain = n;
	}
}

void delete_rain(Laser *node)
{
	//remove a node from linked list
	if (node->next == NULL && node->prev == NULL) {
		//only one item in list
		free(node);
		ihead = NULL;
		return;
	}
	if (node->next != NULL && node->prev == NULL) {
		//at beginning of list
		node->next->prev = NULL;
		free(node);
		return;
	}
	if (node->next == NULL && node->prev != NULL) {
		//at end of list
		node->prev->next = NULL;
		free(node);
		return;
	}
	if (node->next != NULL && node->prev != NULL) {
		//in middle of list
		node->prev->next = node->next;
		node->next->prev = node->prev;
		free(node);
		return;
	}
	//to do: optimize the code above.
}

void cleanup_raindrops(void)
{
	Laser *s;
	while(ihead) {
		s = ihead->next;
		free(ihead);
		ihead = s;
	}
}

