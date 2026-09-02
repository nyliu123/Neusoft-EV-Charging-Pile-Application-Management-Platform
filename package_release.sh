#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
release_dir="${EVCS_RELEASE_ROOT:-$(dirname "$project_dir")/evcs-release}"
version="1.2.0"
archive="$release_dir/evcs-platform-$version-source.tar.gz"

mkdir -p "$release_dir"
tar -czf "$archive" \
  --exclude='./.git' \
  -C "$project_dir" .
sha256sum "$archive" | tee "$archive.sha256"
echo "交付包已生成：$archive"
