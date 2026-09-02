#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${EVCS_BUILD_ROOT:-$(dirname "$project_dir")/evcs-qmake-build}"
runtime_dir="${EVCS_RUNTIME_ROOT:-$(dirname "$project_dir")/evcs-runtime}"
mkdir -p "$runtime_dir"
exec "$build_dir/server/evcs_server" \
  --config "$project_dir/server.json" \
  --schema "$project_dir/schema.sql" \
  --database "$runtime_dir/evcharging.db" \
  --log "$runtime_dir/evcs-server.jsonl" "$@"
