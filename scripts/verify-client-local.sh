#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SERVER_ROOT="${BOOST_GATEWAY_SERVER_ROOT:-"${REPO_ROOT}/../BoostAsioDemo"}"
SERVER_BUILD_DIR="${BOOST_GATEWAY_SERVER_BUILD_DIR:-"${SERVER_ROOT}/build"}"

cmake -S "${REPO_ROOT}" -B "${REPO_ROOT}/build/local" \
  -DBOOST_GATEWAY_SERVER_ROOT="${SERVER_ROOT}" \
  -DBOOST_GATEWAY_SERVER_BUILD_DIR="${SERVER_BUILD_DIR}"
cmake --build "${REPO_ROOT}/build/local" \
  --target boost_gateway_tank_client tank_protocol_smoke_test tank_ui_smoke_test tank_headless_gate
ctest --test-dir "${REPO_ROOT}/build/local" --output-on-failure
