#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${EVCS_BUILD_ROOT:-$(dirname "$project_dir")/evcs-qmake-build}"
exec "$build_dir/user/evcs_user_client" "$@"
