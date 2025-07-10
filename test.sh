#!/bin/bash

GOTO=$1
found=0

for i in $(ls dtbs/* | sort); do
    if [[ $found -eq 0 ]]; then
        if [[ "$i" == "$GOTO" ]]; then
            found=1
        else
            echo "skipping $i"
            continue
        fi
    fi
    echo "$i"
    ./src/fdt "$i" -hint | xdot -
done
