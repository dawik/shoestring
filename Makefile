PROGRAM=glcontext
CC=clang
FLAGS=-g -Wall -lstdc++ -std=c++11 -lm -lGL -lGLEW -lGL -lSDL -lfreetype -lGLU -lBulletDynamics -lBulletCollision -lLinearMath 
SOURCE=src/stuff.cpp -I src/
ASSIMP=-I./assimp/lib/include ./assimp/lib/libassimp.so
BULLET= -I./bullet3/src ./bullet3/Extras/Serialize/BulletFileLoader/CMakeFiles/BulletFileLoader.dir/*.o ./bullet3/src/BulletDynamics/CMakeFiles/BulletDynamics.dir/ConstraintSolver/*.o ./bullet3/Extras/Serialize/BulletWorldImporter/CMakeFiles/BulletWorldImporter.dir/*.o -I./bullet3/Extras/Serialize/BulletFileLoader -I./bullet3/Extras/Serialize/BulletWorldImporter -L./bullet3/Extras/Serialize/BulletWorldImporter/
BUILD=$(CC) $(SOURCE) $(ASSIMP) $(BULLET) $(FLAGS) -o $(PROGRAM)

build:
	$(BUILD)	
run:
	$(BUILD) && ./run.sh
