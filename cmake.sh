#!/usr/bin/bash

mkdir -p build
cd build

export CC="clang-19"
export CXX="clang++-19"

np=$(nproc)
res_np=$(expr $np - "1")
# res_np=1

cmake ../
echo "Using $res_np threads"
cmake --build . -j$res_np && ctest .
