#!/bin/bash 

git submodule update --init --recursive
cd bullet3/build3
./premake4_linux64 gmake
cd ..
sed -i 's/\s\+\(printf("unknown chunk\\n")\);/\/\/\1/' */*/*/*.cpp
cd build3/gmake
make
cd ../../bin
rename _gmake_x64_release '' *.a
echo "Bullet built"

cd ../../assimp
cmake -G 'Unix Makefiles'
make

cd ../freetype-gl
cmake -G 'Unix Makefiles'
make
