#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SERVER_ROOT="${BOOST_GATEWAY_SERVER_ROOT:-"${REPO_ROOT}/../BoostAsioDemo"}"

cmake -S "${REPO_ROOT}" -B "${REPO_ROOT}/build" \
  -DBOOST_GATEWAY_SERVER_ROOT="${SERVER_ROOT}" \
  "$@"
