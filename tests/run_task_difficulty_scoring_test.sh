#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
BUILD_TYPE="${BUILD_TYPE:-Release}"

if command -v sysctl >/dev/null 2>&1; then
  JOBS="${JOBS:-$(sysctl -n hw.ncpu)}"
else
  JOBS="${JOBS:-4}"
fi

echo "[1/3] 配置 CMake 到 ${BUILD_DIR}"
cmake \
  -S "${ROOT_DIR}" \
  -B "${BUILD_DIR}" \
  -DPYTHON=OFF \
  -DBUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

echo "[2/3] 编译 task_difficulty_scoring_test"
cmake --build "${BUILD_DIR}" --target task_difficulty_scoring_test -j "${JOBS}"

echo "[3/3] 运行 task_difficulty_scoring_test"
"${BUILD_DIR}/task_difficulty_scoring_test"
