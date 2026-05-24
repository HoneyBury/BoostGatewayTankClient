#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
SERVER_ROOT="${BOOST_GATEWAY_SERVER_ROOT:-"${REPO_ROOT}/../BoostAsioDemo"}"
SERVER_BUILD_DIR="${BOOST_GATEWAY_SERVER_BUILD_DIR:-"${SERVER_ROOT}/build"}"
BUILD_DIR="${BGTC_PACKAGE_BUILD_DIR:-"${REPO_ROOT}/build/package"}"

cmake -S "${REPO_ROOT}" -B "${BUILD_DIR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBOOST_GATEWAY_SERVER_ROOT="${SERVER_ROOT}" \
  -DBOOST_GATEWAY_SERVER_BUILD_DIR="${SERVER_BUILD_DIR}"
cmake --build "${BUILD_DIR}" --target boost_gateway_tank_client
cmake --build "${BUILD_DIR}" --target package

echo "package output:"
find "${BUILD_DIR}" -maxdepth 1 \( -name "*.tar.gz" -o -name "*.zip" -o -name "*.dmg" \) -print
