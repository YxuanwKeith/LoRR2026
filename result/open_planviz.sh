#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 1 ]]; then
  echo "usage: $0 <city|game|random|warehouse>" >&2
  exit 1
fi

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
CASE_NAME="$1"

DEFAULT_PYTHON_BIN="$ROOT_DIR/PlanViz/.venv/bin/python"
PYTHON_BIN=${PYTHON_BIN:-$DEFAULT_PYTHON_BIN}

if ! command -v "$PYTHON_BIN" >/dev/null 2>&1; then
  echo "PlanViz python not found: $PYTHON_BIN" >&2
  echo "Override with PYTHON_BIN=<python> if needed." >&2
  exit 1
fi

case "$CASE_NAME" in
  city)
    MAP_PATH="$ROOT_DIR/example_problems/city.domain/maps/Paris_1_256.map"
    PLAN_PATH="$ROOT_DIR/result/city_paris_1_256_250.viz.json"
    ;;
  game)
    MAP_PATH="$ROOT_DIR/example_problems/game.domain/maps/brc202d.map"
    PLAN_PATH="$ROOT_DIR/result/game_brc202d_500.viz.json"
    ;;
  random)
    MAP_PATH="$ROOT_DIR/example_problems/random.domain/maps/random-32-32-20.map"
    PLAN_PATH="$ROOT_DIR/result/random_32_32_20_100.viz.json"
    ;;
  warehouse)
    MAP_PATH="$ROOT_DIR/example_problems/warehouse.domain/maps/warehouse_small.map"
    PLAN_PATH="$ROOT_DIR/result/warehouse_small_200.viz.json"
    ;;
  *)
    echo "unknown case: $CASE_NAME" >&2
    exit 1
    ;;
esac

if [[ ! -f "$PLAN_PATH" ]]; then
  echo "missing plan file: $PLAN_PATH" >&2
  echo "run result/run_local_regression.sh first" >&2
  exit 1
fi

exec "$PYTHON_BIN" "$ROOT_DIR/PlanViz/script/run.py" --map "$MAP_PATH" --plan "$PLAN_PATH"
