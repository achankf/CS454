#!/bin/bash -xe

TEST=PartialTestCode

make -C librpc
cp librpc/librpc.a $TEST
make -C $TEST
