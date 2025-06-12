#!/usr/bin/bash

mkdir -p build
cd build

export CC="clang"
export CXX="clang++"

np=$(nproc)
res_np=$(expr $np - "1")
# res_np=1

cmake .. -DCustomBuildType=Asan
echo "Using $res_np threads"
cmake --build . -j$res_np && ctest --exclude-regex "LIVE" .

cd ..
export LSAN_OPTIONS=suppressions=suppressions.txt
./build/frontend/frontend | tee output.txt
