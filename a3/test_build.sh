#!/bin/bash -xe

SRC=src
TEST=test
DFLAG=-ggdb
WFLAG="-pedantic -Wextra -Wall"

make -C $SRC DFLAG="$DFLAG" WFLAG="$WFLAG"
cp $SRC/librpc.a $TEST
make -C $TEST DFLAG="$DFLAG" WFLAG="$WFLAG"
