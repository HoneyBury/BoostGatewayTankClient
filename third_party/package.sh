#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${ROOT_DIR}/.." && pwd)"
SOURCE_DIR="${ROOT_DIR}/sdk-package"
OUTPUT="${PROJECT_DIR}/third_party-client-assets.zip"

if [[ ! -f "${SOURCE_DIR}/lib/cmake/boost_gateway_sdk/boost_gateway_sdk-config.cmake" ]]; then
    echo "ERROR: third_party/sdk-package is missing." >&2
    echo "Run third_party/bootstrap_from_server.sh first." >&2
    exit 1
fi

rm -f "${OUTPUT}"
cd "${PROJECT_DIR}"
zip -r "${OUTPUT}" third_party/sdk-package
echo "Package created: ${OUTPUT}"

