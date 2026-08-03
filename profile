#!/bin/bash

./clean
./run debug --compile-only

valgrind              \
--tool=callgrind      \
--dump-line=yes       \
--dump-instr=yes      \
--collect-jumps=yes   \
--collect-systime=yes \
--collect-bus=yes     \
--cache-sim=yes       \
--branch-sim=yes      \
--simulate-wb=yes     \
--simulate-hwpref=yes \
--cacheuse=yes        \
--time-stamp=yes      \
./bin/debug/gitfluss

kcachegrind ./callgrind.*
