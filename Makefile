all: mandelbrot

CC = mpic++

LDLIBS += -lGL -lX11 -lglut -lGLU
#CPPFLAGS += 

#DEBUG = -g -Wall
OBJ = timer.o log.o main.o

mandelbrot: $(OBJ)
	$(CC) $(LDLIBS) $(OBJ) -o mandelbrot

main.o: main.c
	$(CC) $(DEBUG) $(CPPFLAGS) -c main.c

timer.o: timer.c
	$(CC) $(DEBUG) $(CPPFLAGS) -c timer.c

log.o: log.c
	$(CC) $(DEBUG) $(CPPFLAGS) -c log.c
 
clean: 
	rm -fv $(OBJ) mandelbrot

