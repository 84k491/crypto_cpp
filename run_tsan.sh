#!/bin/bash

sup_file="tsan_suppressions.txt"
export TSAN_OPTIONS="suppressions=$sup_file"
./build/frontend/frontend 2>&1 | tee output.txt
echo $TSAN_OPTIONS
cat $sup_file
