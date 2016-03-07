#!/bin/bash -xe

SRC=src
TEST=test
DFLAG=-ggdb
WFLAG="-pedantic -Wextra -Wall"

make -C $SRC DFLAG="$DFLAG" WFLAG="$WFLAG"
cp src/librpc.a src/binder test
cp src/binder .
make -C $TEST DFLAG="$DFLAG" WFLAG="$WFLAG"
