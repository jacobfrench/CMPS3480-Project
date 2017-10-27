//modified by: 
//date:
//
//3480 Computer Graphics
//lab8.cpp
//Author: Gordon Griesel
//Date:   2017
//
//a simple implementation of OpenGL
//circle drawing
//rotation matrix
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include "fonts.h"
#include <math.h>
typedef double Flt;

#define rnd() (float)rand() / (float)RAND_MAX
const int MAX_POINTS = 10000;
const float PI = 3.14159265;
#define MAX_CONTROL_POINTS 2000
#define SWAP(x,y) (x)^=(y);(y)^=(x);(x)^=(y)


void initXWindows(void);
void init_opengl(void);
void cleanupXWindows(void);
void check_resize(XEvent *e);
//void check_mouse(XEvent *e);
int check_keys(XEvent *e);
void physics(void);
void render(void);


struct Point {
	float x, y;
};

class Global {
public:
	int xres, yres;
	int mode;
	Point point[MAX_POINTS];
	int npoints;
	Point center;
	float radius;
	float matrix[2][2];

	//flag-vars
	int gravity;
	int jello;
	int wind;
	int rows;
	int cols;
	Flt ylen;
	Flt xlen;
	Flt xstart;
	Flt ystart;
	

	Global() {
		srand((unsigned)time(NULL));
		xres = 800;
		yres = 600;
		mode = 0;
		npoints = 5;
		center.x = xres/2;
		center.y = yres/2;
		radius = 100.0;
		gravity = 1;
		jello = 0;
		wind = 1;
		rows = 25;
		cols = 50;
		ylen = 15.0;
		xstart = (xres/2)-250;
		ystart = (yres/2)+270;
	}
} g;

class Image{
public:
	int width, height;
	unsigned char *data;
	GLuint texId;
	

	~Image(){ delete [] data; }
	Image(){
		system("convert flag.jpg flag.ppm");
		FILE *fpi = fopen("flag.ppm","r");
		if(fpi){
			char line[200];
			fgets(line, 200, fpi);
			fgets(line, 200, fpi);
			while(line[0] == '#')
				fgets(line, 200, fpi);
			
			sscanf(line, "%i %i", &width, &height);
			fgets(line, 200, fpi);

			//get pixel data
			int n = width * height * 3;
			data = new unsigned char[n];
			for(int i=0; i<n; i++)
				data[i] = fgetc(fpi);

			fclose(fpi);
		}else{
			printf("ERROR opening input flag.ppm\n");
			exit(0);
		}

		unlink("flag.ppm");
	}
}img;

//mass-struct
struct Mass {
	Flt mass, oomass;
	Flt pos[2];
	Flt vel[2];
	Flt force[2];
	int color[3];
} mass[MAX_CONTROL_POINTS];
int nmasses=0;

//spring-struct
struct Spring {
	int mass[2];
	Flt length;
	Flt stiffness;
	Flt force[2];
	Flt vel[2];
} spring[MAX_CONTROL_POINTS*4];
int nsprings=0;

class X11_wrapper {
private:
	Display *dpy;
	Window win;
	GLXContext glc;
public:
	X11_wrapper() {
		GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
		//GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, None };
		XSetWindowAttributes swa;
		//setup_screen_res(g.xres, g.yres);
		dpy = XOpenDisplay(NULL);
		if(dpy == NULL) {
			printf("\n\tcannot connect to X server\n\n");
			exit(EXIT_FAILURE);
		}
		Window root = DefaultRootWindow(dpy);
		XVisualInfo *vi = glXChooseVisual(dpy, 0, att);
		if(vi == NULL) {
			printf("\n\tno appropriate visual found\n\n");
			exit(EXIT_FAILURE);
		} 
		Colormap cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);
		swa.colormap = cmap;
		swa.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
							StructureNotifyMask | SubstructureNotifyMask;
		win = XCreateWindow(dpy, root, 0, 0, g.xres, g.yres, 0,
								vi->depth, InputOutput, vi->visual,
								CWColormap | CWEventMask, &swa);
		set_title();
		glc = glXCreateContext(dpy, vi, NULL, GL_TRUE);
		glXMakeCurrent(dpy, win, glc);
	}
	~X11_wrapper() {
		XDestroyWindow(dpy, win);
		XCloseDisplay(dpy);
	}
	void set_title() {
		//Set the window title bar.
		XMapWindow(dpy, win);
		XStoreName(dpy, win, "Changing Flag Project - CMPS 3600");
	}
	void check_resize(XEvent *e) {
		//The ConfigureNotify is sent by the
		//server if the window is resized.
		if (e->type != ConfigureNotify)
			return;
		XConfigureEvent xce = e->xconfigure;
		if (xce.width != g.xres || xce.height != g.yres) {
			//Window size did change.
			void reshape_window(int width, int height);
			reshape_window(xce.width, xce.height);
		}
	}
	bool getPending() { return XPending(dpy); }
	void getNextEvent(XEvent *e) { XNextEvent(dpy, e); }
	void swapBuffers() { glXSwapBuffers(dpy, win); }
} x11;

int main(){
	init_opengl();
	int done = 0;
	while (!done) {
		while (x11.getPending()) {
			XEvent e;
			x11.getNextEvent(&e);
			x11.check_resize(&e);
			//check_mouse(&e);
			done = check_keys(&e);
		}
		physics();
		render();
		x11.swapBuffers();
	}
	return 0;
}

void setup_screen_res(const int w, const int h) {
	g.xres = w;
	g.yres = h;

}

void reshape_window(int width, int height) {
	//window has been resized.
	setup_screen_res(width, height);
	glViewport(0, 0, (GLint)width, (GLint)height);
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
	x11.set_title();
}

void init_opengl()
{
	//OpenGL initialization
	glViewport(0, 0, g.xres, g.yres);
	//Initialize matrices
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();
	//This sets 2D mode (no perspective)
	glOrtho(0, g.xres, 0, g.yres, -1, 1);
	//Clear the screen
	glClearColor(0.0, 0.0, 0.0, 1.0);
	//glClear(GL_COLOR_BUFFER_BIT);
	//Do this to allow fonts
	glEnable(GL_TEXTURE_2D);
	initialize_fonts();


   //texture mapping functionality
   glGenTextures(1, &img.texId);
   int w = img.width;
   int h = img.height;
   glBindTexture(GL_TEXTURE_2D, img.texId);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D,0,3,w,h,0,GL_RGB,GL_UNSIGNED_BYTE,img.data);

	glClear(GL_COLOR_BUFFER_BIT);
}


void construct_spring(int curr, int next, int mode, Flt stiffness){
	double length = 0.0;
	if(mode == 0)
		length = mass[next].pos[0] - mass[curr].pos[0];
	else if (mode == 1)
		length = mass[next].pos[1] - mass[curr].pos[1];
	else
		length = (mass[next].pos[1] - mass[curr].pos[1])*sqrt(2.0);

	spring[nsprings].mass[0] = curr;
	spring[nsprings].mass[1] = next;
	spring[nsprings].length = length;
	spring[nsprings].stiffness = stiffness;
	nsprings++;
}


void setup_springs(){
	Flt x;
	Flt y;
	Flt xlen, ylen;
	Flt vel[2];

	//position the masses
	x = (int)g.xstart; //init first x pos
	y = (int)g.ystart; //init first y pos
	nmasses = 0;


	xlen = 12.0;
	
	ylen = g.ylen;
	//initialize masses in rows & columns
	int rowSize = g.rows;
	int colSize = g.cols;
	for (int i=0; i<rowSize; i++) {
		for(int j = 0; j < colSize; j++){
			mass[nmasses].mass = 1.0;
			mass[nmasses].oomass = 1.0 / mass[nmasses].mass;
			mass[nmasses].pos[0] = x;
			mass[nmasses].pos[1] = y;
			if (mass[nmasses].pos[1] < 1.0)
				mass[nmasses].pos[1] = 1.0;
			mass[nmasses].vel[0] = vel[0] + rnd() * 0.02-0.01;
			mass[nmasses].vel[1] = vel[1] + rnd() * 0.02-0.01;
			mass[nmasses].force[0] = 0.0;
			mass[nmasses].force[1] = 0.0;
			mass[nmasses].color[0] = 200 + rand() % 40;
			mass[nmasses].color[1] = 40 + rand() % 20;
			mass[nmasses].color[2] = 10 + rand() % 10;
			nmasses++;
			x += xlen;
		}
		x = g.xstart;
		y -= ylen;
		
	}


	//construct all flag springs===============================================
	//outer loop starts at first mass in each row
	//inner loop determines the current mass position in the current row
	nsprings = 0;	
	Flt stiffness = 0.0;
	for(int i = 0; i < (rowSize*colSize); i+=colSize){
		for(int j = i; j < (i+colSize); j++){

			//horizontal springs
			stiffness = rnd() * 0.2 + 0.1;		
			if(j < (i+colSize-1))
				construct_spring(j, j+1, 0, stiffness);
			
			if(j < (i+colSize-2))
				construct_spring(j, j+2, 0, stiffness);				
		

			//vertical springs
			if(i < (rowSize*colSize)-colSize)
				construct_spring(j+colSize, j, 1, stiffness);
			
			if(i < (rowSize*colSize)-2*colSize)
				construct_spring(j+2*colSize, j, 1, stiffness);
			
			//diagonal springs (right to left)
			stiffness = rnd() * 0.2 + 0.01;
			if(i < (rowSize*colSize)-colSize && j < i+colSize-1)
				construct_spring(j+colSize+1, j, 3, stiffness);		
					
		}
	}
	//end flag springs==========================================================
}

//flag-start
Flt get_wind(Mass &mass1, Mass &mass2){
	//use mass as a vector
	Mass v;
	v.pos[0] = mass2.pos[0] - mass1.pos[0];
	v.pos[1] = mass2.pos[1] - mass1.pos[1];

	Flt len = sqrt(v.pos[0]*v.pos[0] + v.pos[1]*v.pos[1]);

	//normalize v
	v.pos[0] /= len;
	v.pos[1] /= len;

	Flt dot = v.pos[0]*mass2.pos[0];

	Flt wind = 0.0000015;
	bool range30 = dot < 360 && dot > 330;
	bool range60 = dot < 330 && dot > 270;
	bool range90 = dot < 330;

	
	if(range30)
		wind = 0.00014;
	else if(range60)
		wind = 0.00018;
	else if(range90)
		wind = 0.00021;
	else
		wind = 0.000012;	

	// return wind force in x direction
	return wind*dot;
	
}

void maintain_springs(){
	int i,m0,m1;
	Flt dist,oodist,factor;
	Flt springVec[2];
	Flt springforce[2];
	int rowEnd = 0;

	//Move the masses...
	for (i=0; i<nmasses; i++) {
		//anchor first mass in every row======================
		int yPos = g.ystart;
		int xPos = g.xstart;
		int node = 0;
		for(int j=0; j<g.rows; j++){
				mass[node].pos[0] = xPos; //anchor mass xpos
				mass[node].pos[1] = yPos; //anchor mass ypos
				node += g.cols;
				yPos -=g.ylen;
			
		}
		//====================================================

		mass[i].vel[0] += mass[i].force[0] * mass[i].oomass;
		mass[i].vel[1] += mass[i].force[1] * mass[i].oomass;
		mass[i].pos[0] += mass[i].vel[0];
		mass[i].pos[1] += mass[i].vel[1];


		//apply wind force==========================================================
		if(g.wind){
			for(int j = rowEnd; j < g.cols+rowEnd; j++){
				mass[i].force[0] = get_wind(mass[i], mass[i+1]);
				mass[i].force[1] = 0.0;
			}
		}
		else{
			mass[i].force[0] = 0.0;
			mass[i].force[1] = -0.1;
		}
		

		rowEnd = (rowEnd >= g.rows*g.cols-g.cols) ? 0 : rowEnd+g.cols;
		//===========================================================================
	
		//Max velocity
		if (mass[i].vel[0] > 10.0)
			mass[i].vel[0] = 10.0;
		if (mass[i].vel[1] > 10.0)
			mass[i].vel[1] = 10.0;
		//Air resistance, or some type of damping
		mass[i].vel[0] *= 0.999;
		mass[i].vel[1] *= 0.999;
	}

	
	//Resolve all springs...
	for (i=0; i<nsprings; i++) {
		m0 = spring[i].mass[0];
		m1 = spring[i].mass[1];
		//forces are applied here
		//vector between the two masses
		springVec[0] = mass[m0].pos[0] - mass[m1].pos[0];
		springVec[1] = mass[m0].pos[1] - mass[m1].pos[1];
		// //distance between the two masses
		dist = sqrt(springVec[0]*springVec[0] + springVec[1]*springVec[1]);
		if (dist == 0.0) dist = 0.1;
		oodist = 1.0 / dist; 
		springVec[0] *= oodist;
		springVec[1] *= oodist;
		//the spring force is added to the mass force
		factor = -(dist - spring[i].length) * spring[i].stiffness;
		springforce[0] = springVec[0] * factor;
		springforce[1] = springVec[1] * factor;
		//apply force and negative force to each end of the spring...
		mass[m0].force[0] += springforce[0];
		mass[m0].force[1] += springforce[1];
		mass[m1].force[0] -= springforce[0];
		mass[m1].force[1] -= springforce[1];
		//damping
		springforce[0] = (mass[m1].vel[0] - mass[m0].vel[0]) * 0.002;
		springforce[1] = (mass[m1].vel[1] - mass[m0].vel[1]) * 0.002;
		mass[m0].force[0] += springforce[0];
		mass[m0].force[1] += springforce[1];
		mass[m1].force[0] -= springforce[0];
		mass[m1].force[1] -= springforce[1];
	}
}

void myBresenhamLine(int x0, int y0, int x1, int y1)
{
	//Bresenham's line algorithm. integers only!
	int steep = (abs(y1-y0) > abs(x1-x0));
	if (steep) {
		SWAP(x0, y0);
		SWAP(x1, y1);
	}
	if (x0 > x1) {
		SWAP(x0, x1);
		SWAP(y0, y1);
	}
	int ystep = (y1>y0) ? 1 : -1;
	int yDiff = abs(y1-y0);
	int xDiff = x1 - x0;
	int err=xDiff>>1;
	int x, y=y0;
	glBegin(GL_POINTS);
    glColor3ub(80, 255, 255);
	for	(x=x0; x<=x1; x++) {
		if (steep)
			glVertex2f((float)y, (float)x);
		else
			glVertex2f((float)x, (float)y);

		err -= yDiff;
		if (err <= 0) {
			y += ystep;
			err += xDiff;
		}
	}

	glEnd();
}

int check_keys(XEvent *e){
	//Was there input from the keyboard?
	if (e->type == KeyPress) {
		int key = XLookupKeysym(&e->xkey, 0);
		switch (key) {
			case XK_r:
				setup_springs();
				break;
			case XK_Escape:
				return 1;
				break;
		}
	}
	return 0;
}

void physics()
{
	int i;
	//gravity...
	if (g.gravity) {
		for (i=0; i<nmasses; i++) {
			mass[i].force[1] -= 0.01;
		}
	}

	maintain_springs();

}

void showMenu()
{
	Rect r;
	r.bot = g.yres - 20;
	r.left = 15;
	r.center = 0;
	unsigned int color = 0x00ffff00;
	ggprint8b(&r, 16, color, "R - Show Flag");
}


void show_anchor(int x, int y, int size)
{
	//draw a small rectangle...
	int i,j;
	glBegin(GL_POINTS);
    glColor3ub(80, 255, 255);
	for (i=-size; i<=size; i++) {
		for (j=-size; j<=size; j++) {
			glVertex2f((float)x+j, (float)y+i);
		}
	}
	glEnd();
}

void show_lines()
{
	//draw the lines using Bresenham's algorithm
	glBegin(GL_POINTS);
    glColor3ub(80, 255, 255);
	for (int i=0; i<nsprings; i++) {
		myBresenhamLine(
			(int)mass[spring[i].mass[0]].pos[0],
			(int)mass[spring[i].mass[0]].pos[1],
			(int)mass[spring[i].mass[1]].pos[0],
			(int)mass[spring[i].mass[1]].pos[1]);			
	}
	for (int i=0; i<nmasses; i++) {
		show_anchor((int)mass[i].pos[0], (int)mass[i].pos[1],
			(int)mass[i].mass*2);
	}
	glEnd();

}


void show_flag(){

	glColor3ub(255, 255, 255);
	glBindTexture(GL_TEXTURE_2D, img.texId);	

	float stepx = 0.0f;
	float stepy = 0.0f;
	for(int i = 0; i < g.rows*g.cols-g.cols; i+=g.cols){
		
		glBegin(GL_TRIANGLE_STRIP);
		for(int j = 0; j < (g.cols-1); j++){
			glTexCoord2f(stepx, stepy);		
			glVertex2f(mass[j+i].pos[0], mass[j+i].pos[1]);

			glTexCoord2f(stepx, stepy+0.04f);	
			glVertex2f(mass[j+i+g.cols].pos[0], mass[j+i+g.cols].pos[1]);

			glTexCoord2f(stepx+0.02f, stepy);	
			glVertex2f(mass[j+i+1].pos[0], mass[j+i+1].pos[1]);

			glTexCoord2f(stepx+0.02f, stepy+0.04f);			
			glVertex2f(mass[j+i+g.cols+1].pos[0], mass[j+i+g.cols+1].pos[1]);
			stepx += 0.02f;
			
		}
		stepx = 0.0f;
		stepy += 0.04f;
		
		
		glEnd();
		
	}

}


void render()
{
	glClear(GL_COLOR_BUFFER_BIT);
	showMenu();
	// show_lines();
	show_flag();


	switch (g.mode) {

	}
}


