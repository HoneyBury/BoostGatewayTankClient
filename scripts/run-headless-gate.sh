#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
HOST="${BGTC_GATEWAY_HOST:-127.0.0.1}"
PORT="${BGTC_GATEWAY_PORT:-9201}"

"${REPO_ROOT}/build/local/tank_headless_gate" "${HOST}" "${PORT}"
