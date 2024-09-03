#!/bin/sh

# https://github.com/libsdl-org/SDL/releases/latest/download/package.zip

LIBS="-lSDL2 -lSDL2_gfx -lSDL2_ttf -lSDL2_mixer -lm"
DEBUG="-fsanitize=address -g"
PROD="-O3 -msse4.2 -march=native -mtune=native"

if [ "$1" = "debug" ]; then
    PERF="$DEBUG"
else
    PERF="$PROD"
fi

xxd -i bell.wav  | sed 's/\([0-9a-f]\)$/\0, 0x00/' > bell_wav.xxd
xxd -i lemon.ttf | sed 's/\([0-9a-f]\)$/\0, 0x00/' > lemon_ttf.xxd
cc filepicker.c -o filepicker $LIBS $PERF

if [ "$1" = "python" ]; then
    cc graph_viewer.c -o graph_viewer.so $LIBS $PERF -DPYTHON_MODULE -shared -fPIC $(python3-config --cflags --ldflags)
else
    cc graph_viewer.c -o graph_viewer $LIBS $PERF
fi
