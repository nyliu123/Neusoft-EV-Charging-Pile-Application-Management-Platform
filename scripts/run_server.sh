#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$project_dir"
exec "$project_dir/build/bin/evcs_server" --config "$project_dir/config/server.json" "$@"
