#!/bin/sh
if [ $# -eq 0 ]
then
    echo "usage: m d|r|a [target]"
    exit 1
fi
python3 src/gen.py
third_party/ninja/ninja-linux -C out/l$1 $2 $3 $4 $5
