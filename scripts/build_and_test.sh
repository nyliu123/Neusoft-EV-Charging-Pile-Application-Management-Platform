#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$project_dir/build}"

cmake -S "$project_dir" -B "$build_dir" -DCMAKE_BUILD_TYPE=Debug
cmake --build "$build_dir" -j"$(nproc)"
ctest --test-dir "$build_dir" --output-on-failure
PYTHONPYCACHEPREFIX="$build_dir/pycache" python3 -m py_compile "$project_dir/tools/dashboard_bridge.py"
PYTHONPYCACHEPREFIX="$build_dir/pycache" python3 -m py_compile "$project_dir/tools/preprocess_analytics.py"

echo "构建与测试完成，程序位于：$build_dir/bin"
