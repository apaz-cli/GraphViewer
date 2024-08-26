#!/bin/sh

# https://github.com/libsdl-org/SDL/releases/latest/download/package.zip

LIBS="-lSDL2 -lSDL2_gfx -lSDL2_ttf -lSDL2_mixer -lm -fopenmp"
DUMP="-E -P"
DEBUG="-fsanitize=address -g"
PROD="-O3 -msse4.2 -march=native -mtune=native"

xxd -i bell.wav  | sed 's/\([0-9a-f]\)$/\0, 0x00/' > bell_wav.xxd
xxd -i lemon.ttf | sed 's/\([0-9a-f]\)$/\0, 0x00/' > lemon_ttf.xxd
cc filepicker.c -o filepicker $LIBS $PROD
cc graph_viewer.c -o graph_viewer $LIBS $PROD
