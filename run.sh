#!/bin/bash

XEPHYR=$(whereis -b Xephyr | cut -f2 -d' ')
xinit ./xinitrc -- \
    "$XEPHYR" \
        :100 \
        -ac \
        -screen 1728x972 \
        -glamor \
        -host-cursor \
        -noreset

