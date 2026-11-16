#!/bin/sh
# Builds the portable parts on the host for headless testing. Usage: tools/host/build_host.sh <test.c> <out>
set -e
cd "$(dirname "$0")/../.."
gcc -std=gnu11 -O1 -g -Wall -Wno-unused-parameter -Isource -Itools/host "$1" tools/host/host_atlas.c \
    source/jl.c source/map.c source/sprites.c source/util.c source/world.c source/gfx.c source/daycycle.c \
    source/ui.c source/gfxdata.c source/atlasdata.c source/itemdata.c $EXTRA -lm -o "$2"
