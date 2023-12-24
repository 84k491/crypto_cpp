#!/usr/bin/bash

mkdir -p build
cd build

export CC="clang"
export CXX="clang++"

cmake ../
cmake --build . -j8
