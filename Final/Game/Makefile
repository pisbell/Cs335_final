###########################################
# Makefile for GLFW example program on X11
###########################################
CC     = gcc
##CFLAGS = -I../../include   -pthread -O2 -Wall
CFLAGS = -I../include -pthread -O1 #`mysql_config --cflags`\ `pkg-config --cflags glib-2.0`

LIB       = ../lib/x11/libglfw.a
LFLAGS    = $(LIB) -lrt -lX11 -lGLU -lGL -pthread -lm -lXrandr #`mysql_config --libs`\ `pkg-config --libs glib-2.0`

all: rain

rain: rain.o log.o cfonts.o fonttex.o bmp.o constants.h
	gcc -o rain $(CFLAGS) rain.o log.o cfonts.o fonttex.o bmp.o $(LFLAGS)

rain.o: rain.c
	gcc $(CFLAGS) -c rain.c

log.o: log.c
	gcc $(CFLAGS) -c log.c

cfonts.o: cfonts.c
	gcc $(CFLAGS) -c cfonts.c

fonttex.o: fonttex.c
	gcc $(CFLAGS) -c fonttex.c

bmp.o: bmp.c
	gcc $(CFLAGS) -c bmp.c

clean:
	rm -f rain
	rm -f log
	rm -f *.o
	rm -f x.x

