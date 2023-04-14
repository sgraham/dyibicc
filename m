#!/bin/sh
python3 src/gen.py
third_party/ninja/ninja-linux -C out/l$1 $2 $3 $4 $5
