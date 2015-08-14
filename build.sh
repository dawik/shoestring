#!/bin/bash 

git submodule update --recursive
cd bullet3/build3
./premake4_linux64
cd ..
cmake .
sed -i 's/\s\+\(printf("unknown chunk\\n")\);/\/\/\1/' */*/*/*.cpp
make
