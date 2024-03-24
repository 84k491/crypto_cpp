#!/bin/bash

valgrind --tool=callgrind --instr-atstart=no ./build/frontend/frontend
#callgrind_control -i on

#CALLGRIND_START_INSTRUMENTATION
#CALLGRIND_TOGGLE_COLLECT

#https://www.oracle.com/application-development/technologies/developerstudio-features.html#performance-analyzer-tab
