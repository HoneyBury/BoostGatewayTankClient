#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

export BGTC_GATEWAY_HOST="${BGTC_GATEWAY_HOST:-127.0.0.1}"
export BGTC_GATEWAY_PORT="${BGTC_GATEWAY_PORT:-9201}"

exec "${REPO_ROOT}/build/boost_gateway_tank_client"
