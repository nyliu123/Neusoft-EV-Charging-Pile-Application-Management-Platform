#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
temp_dir="$(mktemp -d)"
server_port="${EVCS_TEST_SERVER_PORT:-45465}"
http_port="${EVCS_TEST_HTTP_PORT:-18085}"
server_pid=""
bridge_pid=""

cleanup() {
  [[ -n "$bridge_pid" ]] && kill "$bridge_pid" 2>/dev/null || true
  [[ -n "$server_pid" ]] && kill "$server_pid" 2>/dev/null || true
  rm -rf -- "$temp_dir"
}
trap cleanup EXIT

cd "$project_dir"
"$project_dir/build/bin/evcs_server" --port "$server_port" \
  --database "$temp_dir/dashboard-test.db" --log "$temp_dir/server.jsonl" >"$temp_dir/server.out" 2>&1 &
server_pid=$!
python3 "$project_dir/tools/dashboard_bridge.py" --server-port "$server_port" \
  --http-port "$http_port" >"$temp_dir/bridge.out" 2>&1 &
bridge_pid=$!
sleep 2

python3 - "$http_port" <<'PY'
import json
import sys
import urllib.request

base = f"http://127.0.0.1:{sys.argv[1]}"
def call(path, method="GET"):
    request = urllib.request.Request(base + path, method=method)
    with urllib.request.urlopen(request, timeout=8) as response:
        return json.loads(response.read())

assert call("/api/health")["ok"]
generated = call("/api/generate-demo", "POST")
assert generated["ok"] and generated["data"]["insertedOrders"] >= 60
assert call("/api/generate-demo", "POST")["data"]["insertedOrders"] == 0
dashboard = call("/api/dashboard")["data"]
assert dashboard["summary"]["orderCount"] >= 60
assert len(dashboard["dailyTrend"]) == 30
assert len(dashboard["stationRanking"]) >= 3
print("DASHBOARD_E2E=PASS", json.dumps(dashboard["summary"], ensure_ascii=False))
PY
