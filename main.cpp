#include <GL/glew.h>
#include <GL/glut.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include <sys/time.h>

#include "data/tex.h"

#define clamp_abs(a, b) a=a > b ? a-b : (a < -b ? a+b : a)

#define MAXPTS 10

FILE * logfile=stdout;

#define FUNC __FUNCTION__

void loadTexture(GLuint * texture, const unsigned char * data) {
	glGenTextures(1, texture);
	glBindTexture(GL_TEXTURE_2D, *texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 
				128,
				128,
				0, GL_BGR, GL_UNSIGNED_BYTE,
				data+18);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void logmsg(const char * function, const char * format, ...) {
	va_list ap;
	va_start(ap, format);
	fprintf(logfile, "[MSG  ]: %s: ", function);
	vfprintf(logfile, format, ap);
	va_end(ap);
}

void logerr(const char * function, const char * format, ...) {
	va_list ap;
	va_start(ap, format);
	fprintf(logfile, "[ERROR]: %s: ", function);
	vfprintf(logfile, format, ap);
	va_end(ap);
}

void printGlErrors() {
	GLint err=0;
	while((err=glGetError())!=0) {
		logerr(__FUNCTION__, "GL: %s\n", gluErrorString(err));
	}
}
/* ############################################################ */
struct world_t {
	GLfloat x, y, z,
			dx, dy, dz,
			xrot, yrot, zrot,
			dxrot, dyrot, dzrot;
	unsigned long time;
	unsigned long dtime;
	GLuint n;
	
	GLuint tex1;
	GLuint tex2;
	GLuint tex3;
} world;

#define COLLS 16
#define ROWS 8

struct game_t {
} game;
struct stack_t {
	GLubyte fields[COLLS*ROWS];
} stack;
enum Forms { LONG=0, TTYPE, LTYPE, ZTYPE, /*QUAD,*/ LAST };
struct part_t {
	GLint xr;
	GLint zr;
	Forms type;
	GLint x;
	GLint y;
	GLbyte pts[MAXPTS*2];
} part;
GLbyte ttype_map[8+1]= {
	4,
	0,0, 1,0, -1,0, 0,1
};
GLbyte long_map[8+1]= {
	4,
	2,0, 1,0, 0,0, -1,0
};
GLbyte ltype_map[8+1]= {
	4,
	-1,0, 0,0, 1,0, -1,1
};
GLbyte ztype_map[8+1]= {
	4,
	-1,0, 0,0, 0,1, 1,1
};

GLbyte z_rot_mat[4][9] = {
	{	 1, 0, 0, //0
		 0, 1, 0,
		 0, 0, 1	},
	
	{	 0,-1, 0, //1
		 1, 0, 0,
		 0, 0, 1	},
	
	{	-1, 0, 0, //2
		 0,-1, 0,
		 0, 0, 1	},
	
	{	 0, 1, 0, //3
		-1, 0, 0,
		 0, 0, 1	}
};
GLbyte x_rot_mat[2][9] = {
	{
		 1, 0, 0,
		 0, 1, 0,
		 0, 0, 1	},
	{	
		 1, 0, 0,
		 0,-1, 0,
		 0, 0,-1	}
	
};


void setStackPos(GLint x, GLint y) {
	if(x<COLLS && y<ROWS) {
		stack.fields[ROWS*(x%COLLS)+y]=1;
	}
}
bool isValidPos(GLint x, GLint y) {
	if(x<COLLS && y<ROWS && y>=0)
		return stack.fields[ROWS*(x%COLLS)+y]==0;
	else return false;
}
GLbyte * partMap() {
	switch(part.type) {
		case LONG:
			return long_map; break;
		case TTYPE:
			return ttype_map; break;
		case LTYPE:
			return ltype_map; break;
		case ZTYPE:
			return ztype_map; break;
		default:
			return 0;
	}
}
void crunchPart(GLbyte * part_map) {
	if(part_map) {
		for(GLint p=0; p<(part_map[0]*2); p+=2) {
			setStackPos(part.pts[p], part.pts[p+1]);
		}
	}
}

bool fitPart(GLbyte * part_map, GLint x, GLint y, GLint zr, GLint xr, GLbyte * trx) {
	if(part_map) {
		GLbyte mat[9]= {
			1,0,0,
			0,1,0,
			0,0,1
		};
		for(GLuint i=0; i<3; i++) {
			for(GLuint j=0; j<3; j++) {
				mat[i*3+j]=
					z_rot_mat[zr%4][i*3+0]*x_rot_mat[xr%2][3*0+j]+
					z_rot_mat[zr%4][i*3+1]*x_rot_mat[xr%2][3*1+j]+
					z_rot_mat[zr%4][i*3+2]*x_rot_mat[xr%2][3*2+j];
			}
			//logmsg(FUNC, "[%d][%d][%d]\n", mat[i*3+0], mat[i*3+1], mat[i*3+2]);
		}
		GLint pos[3];
		GLint ux, uy;
		for(GLint p=0; p<part_map[0];p++) {
			ux=part_map[1+p*2];
			uy=part_map[1+p*2+1];
			pos[0]=mat[3*0+0]*ux+mat[3*0+1]*uy+mat[3*0+2]*1;
			pos[1]=mat[3*1+0]*ux+mat[3*1+1]*uy+mat[3*1+2]*1;
			//pos[2]=mat[3*2+0]*ux+mat[3*1+1]*uy+mat[3*2+2]*1;
			/*logmsg(FUNC, "[%d] (%d, %d) ~ [%d, %d] -> {%d, %d}\n", p, pos[0], pos[1], x+ux, y+uy, x+pos[0],y+pos[1]);
			if(p==part_map[0]-1)
				logmsg(FUNC, "\n");*/
			if(!isValidPos(x+pos[0],y+pos[1]))
				return false;
			if(trx) {
				trx[p*2]=x+pos[0];
				trx[p*2+1]=y+pos[1];
			}
		}
		return true;
	}
	return false;
}
void rotatePartZ(GLuint zr) {
	GLbyte * ptr=partMap();
	GLbyte tmp[MAXPTS*2];
	if(fitPart(ptr, part.x, part.y, zr, part.xr, tmp)) {
		part.zr=zr;
		memcpy(part.pts, tmp, MAXPTS*2);
	}
}
void rotatePartX(GLuint xr) {
	GLbyte * ptr=partMap();
	GLbyte tmp[MAXPTS*2];
	if(fitPart(ptr, part.x, part.y, part.zr, xr, tmp)) {
		part.xr=xr;
		memcpy(part.pts, tmp, MAXPTS*2);
	}
}
void movePart(GLuint x, GLuint y) {
	GLbyte * ptr=partMap();
	GLbyte tmp[MAXPTS*2];
	if(fitPart(ptr, x, y, part.zr, part.xr, tmp)) {
		part.x=x;
		part.y=y;
		memcpy(part.pts, tmp, MAXPTS*2);
	}
}
bool partFitH() {
	GLbyte * ptr=partMap();
	return fitPart(ptr, part.x+1, part.y, part.zr, part.xr, 0);
}
void partToStack() {
	GLbyte * ptr=partMap();
	crunchPart(ptr);
}
void gameFunc(GLint t) {
	if(partFitH()) {
		movePart(part.x+1, part.y);
	} else {
		partToStack();
		memset(&part, 0, sizeof(part));
		part.type=(Forms)(rand()%LAST);
		//part.type=LTYPE;
		part.zr=rand()%4;
		part.xr=rand()%2;
		part.x=0;
		part.y=ROWS/2;
		fitPart(partMap(), part.x, part.y, part.zr, part.xr, part.pts);
		for(GLuint c=COLLS; c!=0; c--) {
			GLuint p=0;
			for(GLuint r=0; r<ROWS; r++) {
				if(!isValidPos(c-1, r))
					p++;
			}
			if(p==ROWS) {
				GLubyte * d=stack.fields+ROWS*(c-1);
				GLubyte * s=stack.fields+ROWS*(c-2);
				//printf("%p, %ld, %ld, n: %d\n", stack.fields, (d-stack.fields)/ROWS, (s-stack.fields)/ROWS, c-2);
				for(GLuint i=0; i<c-2; i++) {
					memcpy(d, s, ROWS);
					d-=ROWS;
					s-=ROWS;
					memset(stack.fields, 0, ROWS);
				}
				c++;
				continue;
			}
		}
	}
	glutTimerFunc(500, gameFunc, 0);
} 

/* ############################################################ */
void resizeGlWindow(GLint width, GLint height) {
	if(height==0) {
		height=1;
	}
	GLfloat ratio=(GLfloat)width/(GLfloat)height;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glViewport(0,0, width, height);
	gluPerspective(45, ratio, 1, 1000);
	glMatrixMode(GL_MODELVIEW);
	
}
void handleKeys(GLubyte key, GLint x, GLint y) {
	GLint mods=glutGetModifiers();
	switch(key) {
		case 27:
			exit(0); break;
		case 32:
			rotatePartZ((part.zr+1)%4);
			break;
		case 'l':
			part.type=LTYPE;
			break;
		case 'z':
			part.type=ZTYPE;
			break;
		case 'x':
			rotatePartX((part.xr+1)%2);
			break;
		default:
			logmsg(FUNC, 
				"Unhandled key %d (mods: a:%d s:%d, c:%d)\n",
				key,
				(mods&GLUT_ACTIVE_ALT)>0,
				(mods&GLUT_ACTIVE_SHIFT)>0,
				(mods&GLUT_ACTIVE_CTRL)>0);
	}
}
void handleSpecialKeys(GLint key, GLint x, GLint y) {
	GLint mods=glutGetModifiers();
	switch(key) {
		case GLUT_KEY_LEFT:
			movePart(part.x-1,part.y); 
			break;
		case GLUT_KEY_RIGHT:
			movePart(part.x+1,part.y);
			break;
		case GLUT_KEY_UP:
			movePart(part.x, part.y+1);
			break;
		case GLUT_KEY_DOWN:
			movePart(part.x, part.y-1);
			break;
		default:
			logmsg(FUNC, 
				"Unhandled key %d (mods: a:%d s:%d, c:%d)\n",
				key,
				(mods&GLUT_ACTIVE_ALT)>0,
				(mods&GLUT_ACTIVE_SHIFT)>0,
				(mods&GLUT_ACTIVE_CTRL)>0);
	}
}
void handleSpecialKeysUp(GLint key, GLint x, GLint y) {
/*	GLint mods=glutGetModifiers();
	switch(key) {
		case GLUT_KEY_LEFT:
		case GLUT_KEY_RIGHT:
			world.dyrot=0; break;
		case GLUT_KEY_UP:
		case GLUT_KEY_DOWN:
			world.dxrot=0; break;
		default:
			logmsg(FUNC, 
				"Unhandled key %d (mods: a:%d s:%d, c:%d)\n",
				key,
				(mods&GLUT_ACTIVE_ALT)>0,
				(mods&GLUT_ACTIVE_SHIFT)>0,
				(mods&GLUT_ACTIVE_CTRL)>0);
	}*/
}
void drawQuad() {
	glBegin(GL_QUADS);
		glColor4f(1, 0, 0, 1);
		
		glNormal3f(0, 0,1);
		glTexCoord2f(0,0); glVertex3f(-0.5,-0.5, 0.5);
		glTexCoord2f(0,1); glVertex3f(-0.5, 0.5, 0.5);
		glTexCoord2f(1,1); glVertex3f( 0.5, 0.5, 0.5);
		glTexCoord2f(1,0); glVertex3f( 0.5,-0.5, 0.5);
		glNormal3f(-1, 0,0);
		glTexCoord2f(0,0); glVertex3f(-0.5,-0.5, 0.5);
		glTexCoord2f(0,1); glVertex3f(-0.5, 0.5, 0.5);
		glTexCoord2f(1,1); glVertex3f(-0.5, 0.5,-0.5);
		glTexCoord2f(1,0); glVertex3f(-0.5,-0.5,-0.5);
		glNormal3f(0,0,-1);
		glTexCoord2f(0,0); glVertex3f( 0.5,-0.5,-0.5);
		glTexCoord2f(0,1); glVertex3f( 0.5, 0.5,-0.5);
		glTexCoord2f(1,1); glVertex3f(-0.5, 0.5,-0.5);
		glTexCoord2f(1,0); glVertex3f(-0.5,-0.5,-0.5);
		glNormal3f(1,0,0);
		glTexCoord2f(0,0); glVertex3f( 0.5,-0.5, 0.5);
		glTexCoord2f(0,1); glVertex3f( 0.5, 0.5, 0.5);
		glTexCoord2f(1,1); glVertex3f( 0.5, 0.5,-0.5);
		glTexCoord2f(1,0); glVertex3f( 0.5,-0.5,-0.5);
		glNormal3f(0,1,0);
		glTexCoord2f(0,0); glVertex3f(-0.5, 0.5, 0.5);
		glTexCoord2f(0,1); glVertex3f(-0.5, 0.5,-0.5);
		glTexCoord2f(1,1); glVertex3f( 0.5, 0.5,-0.5);
		glTexCoord2f(1,0); glVertex3f( 0.5, 0.5, 0.5);
		glNormal3f(0,-1,0);
		glTexCoord2f(0,0); glVertex3f(-0.5,-0.5, 0.5);
		glTexCoord2f(0,1); glVertex3f(-0.5,-0.5,-0.5);
		glTexCoord2f(1,1); glVertex3f( 0.5,-0.5,-0.5);
		glTexCoord2f(1,0); glVertex3f( 0.5,-0.5, 0.5);
		
	glEnd();
}
void drawLine(const char * str, GLuint x, GLuint y)	{
	glDisable(GL_LIGHTING);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	GLint   viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    gluOrtho2D(viewport[0],viewport[2],viewport[1],viewport[3]);
    glMatrixMode(GL_MODELVIEW);
    
    glRasterPos2i(x, y);
	const char * ptr;
	for (ptr=str; *ptr != '\0'; ptr++) {
		  glutBitmapCharacter(GLUT_BITMAP_8_BY_13, *ptr);
	}
	
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
}

void drawScene() {
	/* update timer */
	struct timeval tv; gettimeofday(&tv, 0);
	unsigned long t1=tv.tv_sec*1000+tv.tv_usec/1000; 
	world.dtime=t1-world.time;
	world.time=t1;
	
	/* update variables */
	world.x+=world.dx*(GLfloat)world.dtime;
	world.y+=world.dy*(GLfloat)world.dtime;
	world.z+=world.dz*(GLfloat)world.dtime;
	world.xrot+=world.dxrot*(GLfloat)world.dtime;
	world.yrot+=world.dyrot*(GLfloat)world.dtime;
	world.zrot+=world.dzrot*(GLfloat)world.dtime;
	/* clamp */

	clamp_abs(world.xrot, 360.0f);
	clamp_abs(world.yrot, 360.0f); 
	clamp_abs(world.zrot, 360.0f); 
	
	/* draw */
	glClearColor(0.2, 0.2, 0.2, 1);
	
	glEnable(GL_LIGHT0);
	
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	glTranslatef(-10,-5,-20);
	glRotatef(world.xrot, 1, 0, 0);
	glRotatef(world.yrot, 0, 1, 0);
	glRotatef(world.zrot, 0, 0, 1);
	glTranslatef(world.x,world.y,world.z);
	
	
	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);
	
	glBindTexture(GL_TEXTURE_2D, world.tex1);
	/* border */
	glPushMatrix();
		for(GLuint i=0; i<=COLLS; i++) {
			drawQuad(); glTranslatef(1.1,0,0);
		}
		glTranslatef(-1.1-1.1*COLLS,1.1+ROWS*1.1,0);
		for(GLuint i=0; i<=COLLS; i++) {
			drawQuad(); glTranslatef(1.1,0,0);
		}
		glTranslatef(-1.1,-1.1,0);
		for(GLuint i=0; i<ROWS; i++) {
			drawQuad(); glTranslatef(0,-1.1,0);
		}
	glPopMatrix();
	
	/*glPushMatrix();
		glTranslatef(part.x*1.1, 1.1+part.y*1.1, 0);
		glRotatef(90*part.zr, 0, 0, 1);
		glRotatef(180*part.xr, 1, 0, 0);
		drawForm((Forms)part.type);
	glPopMatrix();*/
	
	/* ################################ */
	glBindTexture(GL_TEXTURE_2D, world.tex2);
	GLbyte * ptr=partMap();
	for(GLint p=0; p<(ptr[0]*2); p+=2) {
		glPushMatrix();
			glTranslatef(1.1*part.pts[p], 1.1+1.1*part.pts[p+1], 0);
			drawQuad();
		glPopMatrix();
	}

	/* ################################ */
	glBindTexture(GL_TEXTURE_2D, world.tex3);
	for(GLuint x=0; x<COLLS; x++) {
		for(GLuint y=0; y<ROWS; y++) {
			if(!isValidPos(x,y)) {
				glPushMatrix();
					glTranslatef(x*1.1, 1.1+y*1.1, 0);
					drawQuad();
				glPopMatrix();
			}
		}
	}
	
	char tmp[200];
	snprintf(tmp, sizeof(tmp), "part: x:%d, y:%d, xr:%d zr:%d, t:%d", part.x, part.y, part.xr, part.zr, (GLuint)part.type);
	
	drawLine(tmp, 0,0);
	
	/* finish drawing */
	printGlErrors();
	glutSwapBuffers();
	//usleep(10000);
}
/* ############################################################ */
int main(int argc, char ** argv) {
	glutInit(&argc, argv);
	glutInitWindowPosition(0,0);
	glutInitWindowSize(800,400);
	glutInitDisplayMode(GLUT_RGBA|GLUT_DOUBLE|GLUT_ACCUM|GLUT_DEPTH|GLUT_STENCIL);
	glutCreateWindow("GLF/G");
	glutReshapeFunc(resizeGlWindow);
	
	glutKeyboardFunc(handleKeys);
	glutSpecialFunc(handleSpecialKeys);
	glutSpecialUpFunc(handleSpecialKeysUp);
	
	glutTimerFunc(500, gameFunc, 0);
	
	memset(&world, 0, sizeof(world));
	memset(&stack, 0, sizeof(stack));
	memset(&part, 0, sizeof(part));
	
	
	//part.type=(Forms)(rand()%LAST);
	part.type=LTYPE;
	//part.zr=rand()%4;
	part.zr=rand()%4;
	part.xr=rand()%2;
	part.y=ROWS/2;
	fitPart(partMap(), part.x, part.y, part.zr, part.xr, part.pts);
	
	loadTexture(&world.tex1, tex1_tga);
	loadTexture(&world.tex2, tex2_tga);
	loadTexture(&world.tex3, tex3_tga);
	
	glutDisplayFunc(drawScene);
	glutIdleFunc(drawScene);
	
	glewInit();
	
	if(glewIsSupported("GL_VERSION_2_1")) {
		glutMainLoop();
	}
	
	
	
	return 0;
}
