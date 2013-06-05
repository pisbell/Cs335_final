###########################################
# Makefile for GLFW example program on X11
###########################################
CC     = gcc
##CFLAGS = -I../../include   -pthread -O2 -Wall
CFLAGS = -I../include -pthread -O1 `pkg-config --cflags glib-2.0` 

LIB       = ../lib/x11/libglfw.a
LFLAGS    = $(LIB) -lrt -lX11 -lGLU -lGL -pthread -lm -lXrandr `pkg-config --libs glib-2.0` 

all: swg

swg: swg.o log.o cfonts.o fonttex.o bmp.o constants.h
	gcc -o swg $(CFLAGS) swg.o log.o cfonts.o fonttex.o bmp.o $(LFLAGS)

swg.o: swg.c
	gcc $(CFLAGS) -c swg.c

log.o: log.c
	gcc $(CFLAGS) -c log.c

cfonts.o: cfonts.c
	gcc $(CFLAGS) -c cfonts.c

fonttex.o: fonttex.c
	gcc $(CFLAGS) -c fonttex.c

bmp.o: bmp.c
	gcc $(CFLAGS) -c bmp.c

clean:
	rm -f swg
	rm -f log
	rm -f *.o
	rm -f x.x

