all: project

project: project.cpp
	g++ project.cpp libggfonts.a -Wall -oproject -lX11 -lGL -lGLU -lm

clean:
	rm -f project

