#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${1:-$(dirname "$project_dir")/evcs-qmake-build}"
artifact_dir="${EVCS_TEST_ARTIFACT_DIR:-$(dirname "$project_dir")/evcs-test-artifacts}"
qmake_bin="${QMAKE_BIN:-qmake6}"
jobs="${EVCS_BUILD_JOBS:-$(nproc)}"

build_target() {
  local name="$1"
  local project_file="$2"
  mkdir -p "$build_dir/$name"
  (cd "$build_dir/$name" && "$qmake_bin" "$project_dir/$project_file" && make -j"$jobs")
}

build_target server evcs_server.pro
build_target user evcs_user_client.pro
build_target admin evcs_admin_client.pro
build_target protocol-test evcs_protocol_test.pro
build_target business-test evcs_business_flow_test.pro
build_target socket-test evcs_socket_integration_test.pro
build_target ui-test evcs_ui_acceptance_test.pro

"$build_dir/protocol-test/protocol_test"
"$build_dir/business-test/business_flow_test"
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
  "$build_dir/socket-test/socket_integration_test"
mkdir -p "$artifact_dir"
QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-offscreen}" \
  EVCS_TEST_ARTIFACT_DIR="$artifact_dir" "$build_dir/ui-test/ui_acceptance_test"
PYTHONPYCACHEPREFIX="$build_dir/pycache" python3 -m py_compile \
  "$project_dir/dashboard_bridge.py" "$project_dir/preprocess_analytics.py"
python3 "$project_dir/preprocessing_test.py" "$project_dir/preprocess_analytics.py"
python3 "$project_dir/structure_test.py"

echo "构建与测试完成：$build_dir"
echo "界面验收截图：$artifact_dir"
