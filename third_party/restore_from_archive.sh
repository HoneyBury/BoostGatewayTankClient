#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${ROOT_DIR}/.." && pwd)"
ARCHIVE="${1:-"${PROJECT_DIR}/third_party-client-assets.zip"}"

if [[ ! -f "${ARCHIVE}" ]]; then
    echo "ERROR: archive not found: ${ARCHIVE}" >&2
    exit 1
fi

unzip -o "${ARCHIVE}" -d "${PROJECT_DIR}"
echo "Restored client third-party assets from ${ARCHIVE}"
