#!/usr/bin/env bash
# Quick one-shot build without CMake. Requires SDL3 + pkg-config.
set -e
cd "$(dirname "$0")"
g++ -std=c++17 -O2 -Wall -Wextra src/main.cpp $(pkg-config --cflags --libs sdl3) -o bombjack
echo "Built ./bombjack — run it with ./bombjack"
