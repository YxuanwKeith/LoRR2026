#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
RESULT_DIR="$ROOT_DIR/result"
BUILD_DIR="$ROOT_DIR/build"

mkdir -p "$RESULT_DIR"

cmake -S "$ROOT_DIR" -B "$BUILD_DIR" -DPYTHON=false -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" --parallel

ctest --test-dir "$BUILD_DIR" --output-on-failure > "$RESULT_DIR/ctest.log"

declare -a CASES=(
  "city example_problems/city.domain/paris_1_256_250.json city_paris_1_256_250.viz.json"
  "game example_problems/game.domain/brc202d_500.json game_brc202d_500.viz.json"
  "random example_problems/random.domain/random_32_32_20_100.json random_32_32_20_100.viz.json"
  "warehouse example_problems/warehouse.domain/warehouse_small_200.json warehouse_small_200.viz.json"
)

for entry in "${CASES[@]}"; do
  read -r _ input_file output_file <<<"$entry"
  "$BUILD_DIR/lifelong" \
    -i "$ROOT_DIR/$input_file" \
    -o "$RESULT_DIR/$output_file" \
    -s 1000 \
    -c 1 \
    --prettyPrintJson \
    > /dev/null
done

python3.11 "$ROOT_DIR/result/write_local_regression_summary.py"

