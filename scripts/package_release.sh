#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
release_dir="$project_dir/release"
version="1.1.0"
archive="$release_dir/evcs-platform-$version-source.tar.gz"

mkdir -p "$release_dir"
tar -czf "$archive" \
  --exclude='./.git' --exclude='./build' --exclude='./build-*' \
  --exclude='./data' --exclude='./logs' --exclude='./release' \
  -C "$project_dir" .
sha256sum "$archive" | tee "$archive.sha256"
echo "交付包已生成：$archive"
