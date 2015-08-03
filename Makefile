PROGRAM=glcontext
CC=clang
CFLAGS=-g -Wall -lm -lGL -lGLEW -lGL -lSDL -lfreetype -lGLU -lstdc++ -lpthread -std=c++11
FILES=src/stuff.cpp -I src/
ASSIMP=-I./assimp/lib/include ./assimp/lib/libassimp.so
BULLET= -I./bullet3/src ./bullet3/Extras/Serialize/BulletFileLoader/CMakeFiles/BulletFileLoader.dir/*.o /home/dave/dev/fps-build/bullet3/src/BulletDynamics/CMakeFiles/BulletDynamics.dir/ConstraintSolver/*.o ./bullet3/Extras/Serialize/BulletWorldImporter/CMakeFiles/BulletWorldImporter.dir/*.o -lBulletDynamics -lBulletCollision -lLinearMath -I./bullet3/Extras/Serialize/BulletFileLoader -I./bullet3/Extras/Serialize/BulletWorldImporter -L./bullet3/Extras/Serialize/BulletWorldImporter/
BUILD=$(CC) $(FILES) $(CFLAGS) $(ASSIMP) $(BULLET) -o $(PROGRAM)

build:
	$(BUILD)	
run:
	$(BUILD) && ./run.sh
