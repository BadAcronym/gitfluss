#!/bin/bash

./clean
./run debug --compile-only
valgrind --tool=callgrind ./bin/debug/gitfluss
kcachegrind ./callgrind.*
