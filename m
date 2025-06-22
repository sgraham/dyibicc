#!/bin/sh
if [ $# -eq 0 ]
then
    echo "usage: m d|r|a [target]"
    exit 1
fi
python3 src/gen.py

OS="$(uname -s)"

case "$OS" in
  Darwin) exec third_party/ninja/ninja-mac -C out/m$1 $2 $3 $3 $5;;
  Linux)  exec third_party/ninja/ninja-linux -C out/l$1 $2 $3 $3 $5;;
  *)      echo "Unsupported OS ${OS}"
          exit 1;;
esac
