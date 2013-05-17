#include "defs.h"
#include "log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "GL/glfw.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#define _O_BINARY 0
#define _O_RDWR   O_RDWR
#define _O_CREAT  O_CREAT
#define _O_TRUNC  O_TRUNC
#define _S_IWRITE S_IWRITE
#define _MAX_PATH 256

unsigned int loadBMP(const char *imagepath, int alpha)
{
	unsigned int retval;
	unsigned char header[54];
	//Each BMP file begins by a 54-bytes header
	//Position in the file where the actual data begins
	unsigned int dataPos;
	unsigned int width, height;
	unsigned int imageSize;
	//RGB data will go in this
	unsigned char *data; 
	//
	Log("opening file **%s**\n",imagepath);
	FILE * file = fopen(imagepath,"r");
	if (!file) { Log("Image could not be opened\n"); return 0; } 
	if (fread(header, 1, 54, file)!=54){
		// If not 54 bytes read : problem
		Log("Not a correct BMP file\n");
		return 0;
	} 
	if (header[0]!='B' || header[1]!='M') {
		Log("Not a correct BMP file\n");
		return 0;
	}
	dataPos   = *(int*)&(header[0x0A]);
	imageSize = *(int*)&(header[0x22]);
	width     = *(int*)&(header[0x12]);
	height    = *(int*)&(header[0x16]); 
	//Some BMP files are misformatted, guess missing information
	if (imageSize==0) imageSize=width*height*3;
	//imageSize=width*height*3;
	//add extra bit-per-pixel
	//if (alpha) imageSize += width*height;
	//
	if (dataPos==0) dataPos=54; 
	data = (unsigned char *)malloc(imageSize+1);
	//Read the actual data from the file into the buffer
	retval = fread(data,1,imageSize,file);
	fclose(file); 
	/* In glTexImage2D, the GL_RGB indicates that we are talking
		about a 3-component color, and GL_BGR says how exactly
		it is represented in RAM. As a matter of fact, BMP does
		not store Red->Green->Blue but Blue->Green->Red, so we
		have to tell it to OpenGL. */
	#define GL_BGR 0x80E0
	//Create one OpenGL texture
	GLuint textureID;
	glGenTextures(1, &textureID);
	/* "Bind" the newly created texture
		all future texture functions will modify this texture */
	// Give the image to OpenGL
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, width, height, 0, GL_BGR, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	free(data);
	return textureID;
}

typedef struct t_Bmphead {
	char id1;				// 1     1
	char id2;				// 1     2
	long filesize;			// 4     6
	short reserved1;		// 2     8
	short reserved2;		// 2    10
	long headersize;		// 4    14
	long infoSize;			// 4    18
	long width;				// 4    22
	long depth;				// 4    26
	short biPlanes;			// 2    28
	short bits;				// 2    30
	long biCompression;		// 4    34
	long biSizeImage;		// 4    38
	long biXPelsPerMeter;	// 4    42
	long biYPelsPerMeter;	// 4    46
	long biClsUsed;			// 4    50
	long biClrImportant;	// 4    54
} Bmphead;

void GetBmphead2(int fh, Bmphead *bmphead)
{
	int ret;
	ret = read(fh, &bmphead->id1,            1);
	ret = read(fh, &bmphead->id2,            1);
	ret = read(fh, &bmphead->filesize,       4);
	ret = read(fh, &bmphead->reserved1,      2);
	ret = read(fh, &bmphead->reserved2,      2);
	ret = read(fh, &bmphead->headersize,     4);
	ret = read(fh, &bmphead->infoSize,       4);
	ret = read(fh, &bmphead->width,          4);
	ret = read(fh, &bmphead->depth,          4);
	ret = read(fh, &bmphead->biPlanes,       2);
	ret = read(fh, &bmphead->bits,           2);
	ret = read(fh, &bmphead->biCompression,  4);
	ret = read(fh, &bmphead->biSizeImage,    4);
	ret = read(fh, &bmphead->biXPelsPerMeter,4);
	ret = read(fh, &bmphead->biYPelsPerMeter,4);
	ret = read(fh, &bmphead->biClsUsed,      4);
	ret = read(fh, &bmphead->biClrImportant, 4);
}

GLuint tex_readgl_bmp(char *fileName, int alpha_channel)
{
	int i,j,w,h,ret;
	unsigned char *rbuf;
	int fh,k,bytesPerLine;
	int BGJ=0;
	Bmphead bmphead;
	Texmap tm;

	printf("tex_readgl_bmp(%s)...\n",fileName);
	fh = open(fileName, _O_RDWR);
	if (fh < 0) {
		printf("ERROR: file not found **%s**\n",fileName);
		return 0;
	}
	//Image is a BMP bitmap.
	GetBmphead2(fh, &bmphead);
	w = (int)bmphead.width;
	h = (int)bmphead.depth;
	bytesPerLine = w * 3;
	while(bytesPerLine % 4 != 0) { bytesPerLine++; }
	tm.c = (unsigned char *)malloc( sizeof(char *) * h * w * 4 );
	rbuf = (unsigned char *)malloc(bytesPerLine+100);
	if (bmphead.bits >= 24) {
		int p = 0;
		int c1,c2,c3,c4;
		int lcount=0;
		unsigned char *ptr = tm.c;
		for(j=h-1; j>=0; j--) {
			ret = read(fh, rbuf, bytesPerLine);
			k = 0;
			for (i=0; i<w; i++) {
				if (alpha_channel) {
					c3 = rbuf[k++];
					c2 = rbuf[k++];
					c1 = rbuf[k++];
					*ptr     = c1;
					*(ptr+1) = c2;
					*(ptr+2) = c3;
					c4 = (c1+c2+c3) / 3;
					*(ptr+3) = (unsigned char)c4;
					ptr += 4;
				}
				else {
					*(ptr+2) = rbuf[k++];
					*(ptr+1) = rbuf[k++];
					*(ptr+0) = rbuf[k++];
					ptr+=3;
				}
			}
		}
	}
	close(fh);
	tm.xres = w;
	tm.yres = h;
	free(rbuf);
	//
	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	//
	if (alpha_channel) {
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
						tm.xres, tm.yres, 0, GL_RGBA, GL_UNSIGNED_BYTE, tm.c );
		Log("setting up alpha channel for **%s**\n", fileName);
	} else {
	glTexImage2D(GL_TEXTURE_2D, 0, 3, tm.xres, tm.yres, 0, GL_RGB, GL_UNSIGNED_BYTE, tm.c);
	}
	free(tm.c);
	return textureID;
}

