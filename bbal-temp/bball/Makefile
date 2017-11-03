all: bball

bball: bball.cpp ppm.cpp
	g++ bball.cpp ppm.cpp libggfonts.a -Wall -Wextra -o bball -lX11 -lGL -lGLU -lm

clean:
	rm -f bball
	rm -f *.o

