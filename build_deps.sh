#!/bin/bash 
# Quick and dirty script to build dependencies

git submodule update --init --recursive

cd bullet3/build3
./premake4_osx gmake
cd ..
sed -i -e 's/\s\+\(printf("unknown chunk\\n")\);/\/\/\1/' */*/*/*.cpp
cd build3/gmake
make
cd ../../bin
for i in *.a
do
  mv -- "$i" "${i/_*/.a}"
done

cd ../../assimp
cmake -G 'Unix Makefiles'
make
