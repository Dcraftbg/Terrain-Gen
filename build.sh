#!/bin/sh
set -xe


cc -g -fPIC -shared src/game.c -o game.so -O2 -lm
cc -g src/main.c -o main -lX11 -O2  
