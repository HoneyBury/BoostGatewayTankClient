#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_ROOT="${1:-"${ROOT_DIR}/../../BoostAsioDemo"}"
SDK_PREFIX="${SERVER_ROOT}/runtime/sdk-package-prefix"
DEST="${ROOT_DIR}/sdk-package"

if [[ ! -f "${SDK_PREFIX}/lib/cmake/boost_gateway_sdk/boost_gateway_sdk-config.cmake" ]]; then
    echo "ERROR: SDK package config not found under ${SDK_PREFIX}" >&2
    echo "Build and install the server SDK first." >&2
    exit 1
fi

if [[ -d "${DEST}" ]]; then
    echo "[skip] third_party/sdk-package already exists"
    exit 0
fi

echo "[copy] ${SDK_PREFIX} -> ${DEST}"
cp -R "${SDK_PREFIX}" "${DEST}"
echo "Done. Client third_party SDK package is ready."

