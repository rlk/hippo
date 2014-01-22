OPTS= -g -Wall

ifeq ($(shell uname), Darwin)
	CC= cc
	CXX= c++
	GL= -framework GLUT -framework OpenGL
endif

ifeq ($(shell uname), Linux)
	CC=  g++
	CXX= g++
	GL= -lGLEW -lGL -lglut
endif

all : hipgen hipviz

hipviz : hipviz-glut.o hipviz.o hippo.o
	$(CXX) $(OPTS) -o $@ $^ -lm $(GL)

hipgen : hipgen.o hippo.o
	$(CC) $(OPTS) -o $@ $^ -lm

hipparcos.riff : hipgen hip_main.dat
	./hipgen -H hip_main.dat hipparcos.riff

tycho.riff : hipgen tyc2.dat
	./hipgen -T tyc2.dat tycho.riff

.c.o :
	$(CC) $(OPTS) -c $<

.cpp.o :
	$(CXX) $(OPTS) -c $<

clean :
	$(RM) *.o hipviz hipgen
