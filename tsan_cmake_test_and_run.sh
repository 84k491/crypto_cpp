#!/usr/bin/bash

mkdir -p build
cd build

export CC="clang"
export CXX="clang++"

np=$(nproc)
res_np=$(expr $np - "1")
# res_np=1

cmake .. -DCustomBuildType=Tsan
echo "Using $res_np threads"
cmake --build . -j$res_np && ctest --exclude-regex "LIVE" .

cd ..
sup_file="tsan_suppressions.txt"
export TSAN_OPTIONS="suppressions=$sup_file"
./build/frontend/frontend 2>&1 | tee output.txt
echo $TSAN_OPTIONS
cat $sup_file
