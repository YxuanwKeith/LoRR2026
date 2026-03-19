#!/usr/bin/env bash
set -euo pipefail

mkdir -p build

# build exec for cpp
cmake -S . -B build -DPYTHON=false -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# build exec for python
# cmake -S . -B build -DPYTHON=true -DCMAKE_BUILD_TYPE=Release
# cmake --build build --parallel
