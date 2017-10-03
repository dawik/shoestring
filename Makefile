PROGRAM=ss-engine
CC=clang
FLAGS=-g -Wall -Wno-unused-function -std=c++11 -lstdc++
FREETYPE=-I/usr/include/freetype2 -lfreetype
FREETYPE_OSX=-I/usr/local/opt/freetype/include/freetype2 -L/usr/local/opt/freetype/lib
INCLUDE_OSX=-I/opt/X11/include/
LINKED_LIBRARIES=-lstdc++ -lm -lGLEW -framework OpenGL -lglfw -lfreetype -lBulletDynamics -lBulletCollision -lLinearMath
SOURCE=src/code.cpp src/glstuff.cpp src/text.cpp -I./src
ASSIMP=-I./assimp/include ./assimp/lib/libassimp.dylib
BULLET_OBJ_DIR=./bullet3/build3/gmake/obj/x64/Release
BULLET_OBJ_FILES=$(BULLET_OBJ_DIR)/BulletFileLoader/*.o $(BULLET_OBJ_DIR)/BulletDynamics/*.o $(BULLET_OBJ_DIR)/BulletWorldImporter/*.o $(BULLET_OBJ_DIR)/BulletCollision/*.o
BULLET= -I./bullet3/src -L./bullet3/bin $(BULLET_OBJ_FILES) -I./bullet3/Extras/Serialize/BulletFileLoader -I./bullet3/Extras/Serialize/BulletWorldImporter -L./bullet3/Extras/Serialize/BulletWorldImporter/
GLM=-I./glm/
STB_IMAGE=-I./stb_image/ ./stb_image/stb_image.cpp
BUILD=$(CC) $(SOURCE) $(ASSIMP) $(BULLET) $(GLM) $(INCLUDE_OSX) $(FREETYPE_OSX) $(STB_IMAGE) $(FLAGS) $(LINKED_LIBRARIES) -o $(PROGRAM)

build:
	$(BUILD) && ./run.sh
