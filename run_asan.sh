#!/bin/bash

export LSAN_OPTIONS=suppressions=suppressions.txt
./build/frontend/frontend #> output.txt 2>&1
