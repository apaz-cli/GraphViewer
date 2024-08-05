#!/bin/sh

SOURCE_FILE="graph_viewer.c"
EXECUTABLE="graph_viewer"
#SAMPLE_FILE="data.json"
SAMPLE_FILE="object_graph.json"

LIBS="-lSDL2 -lSDL2_gfx -lSDL2_ttf -lSDL2_mixer -lm -fopenmp"

DUMP="-E -P"
DEBUG="-fsanitize=address -g"
PROD="-O3 -msse4.2 -march=native -mtune=native"

xxd -i bell.wav  | sed 's/\([0-9a-f]\)$/\0, 0x00/' > bell_wav.xxd
cc $SOURCE_FILE -o $EXECUTABLE $LIBS $DEBUG
