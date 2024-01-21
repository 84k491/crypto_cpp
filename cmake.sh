#!/usr/bin/bash

mkdir -p build
cd build

export CC="clang"
export CXX="clang++"

np=$(nproc)
res_np=$(expr $np - "1")
$res_np

cmake ../
echo "Using $res_np threads"
cmake --build . -j$res_np
