#!/usr/bin/env bash
set -eu

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-${project_root}/build}"
smoke_port="${EV_SMOKE_PORT:-18888}"
smoke_dir="${build_dir}/smoke"

server="${build_dir}/bin/ev_server"
user_client="${build_dir}/bin/ev_user_client"
admin_client="${build_dir}/bin/ev_admin_client"

for executable in "$server" "$user_client" "$admin_client"; do
    if [[ ! -x "$executable" ]]; then
        echo "Missing executable: $executable" >&2
        echo "Build the project before running this check." >&2
        exit 2
    fi
done

mkdir -p "$smoke_dir"
"$server" \
    --host 127.0.0.1 \
    --port "$smoke_port" \
    --database "$smoke_dir/platform.sqlite3" \
    >"$smoke_dir/server.log" 2>&1 &
server_pid=$!

cleanup() {
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

QT_QPA_PLATFORM=offscreen "$user_client" --check --host 127.0.0.1 --port "$smoke_port"
QT_QPA_PLATFORM=offscreen "$admin_client" --check --host 127.0.0.1 --port "$smoke_port"

echo "Smoke test passed: server, user client and admin client completed the protocol handshake."

