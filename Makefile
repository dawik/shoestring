PROGRAM=ss-engine
CC=clang
FLAGS=-g -Wall -lstdc++ -std=c++11 -lm -lGL -lGLEW -lGL -lSDL -lfreetype -lGLU -lBulletDynamics -lBulletCollision -lLinearMath 
SOURCE=src/stuff.cpp -I src/
ASSIMP=-I./assimp/include ./assimp/lib/libassimp.so
BULLET_OBJ_DIR=./bullet3/build3/gmake/obj/x64/Release
BULLET_OBJ_FILES=$(BULLET_OBJ_DIR)/BulletFileLoader/*.o $(BULLET_OBJ_DIR)/BulletDynamics/*.o $(BULLET_OBJ_DIR)/BulletWorldImporter/*.o $(BULLET_OBJ_DIR)/BulletCollision/*.o
BULLET= -I./bullet3/src -L./bullet3/bin $(BULLET_OBJ_FILES) -I./bullet3/Extras/Serialize/BulletFileLoader -I./bullet3/Extras/Serialize/BulletWorldImporter -L./bullet3/Extras/Serialize/BulletWorldImporter/
BUILD=$(CC) $(SOURCE) $(ASSIMP) $(BULLET) $(FLAGS) -o $(PROGRAM)

build:
	$(BUILD)	
run:
	$(BUILD) && ./run.sh
