#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build/linux"
CONFIG="${1:-Debug}"

: "${VCPKG_ROOT:?VCPKG_ROOT not set (export VCPKG_ROOT=/path/to/vcpkg)}"
TOOLCHAIN="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
if [[ ! -f "${TOOLCHAIN}" ]]; then
  echo "ERROR: vcpkg toolchain not found: ${TOOLCHAIN}" >&2
  exit 1
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE="${CONFIG}" \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
  -DVKMINI_ENABLE_VALIDATION=ON

cmake --build "${BUILD_DIR}" --config "${CONFIG}"

# Enable validation if vcpkg installed it under the build tree.
CACHE="${BUILD_DIR}/CMakeCache.txt"
INSTALLED_DIR="$(grep -E '^VCPKG_INSTALLED_DIR:' "${CACHE}" | head -n1 | cut -d= -f2- || true)"
TRIPLET="$(grep -E '^VCPKG_TARGET_TRIPLET:' "${CACHE}" | head -n1 | cut -d= -f2- || true)"
LAYER_JSON="${INSTALLED_DIR}/${TRIPLET}/bin/VkLayer_khronos_validation.json"

if [[ -f "${LAYER_JSON}" ]]; then
  export VK_LAYER_PATH="$(dirname "${LAYER_JSON}")"
  export VK_ADD_LAYER_PATH="${VK_LAYER_PATH}"
  export VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation"
  export LD_LIBRARY_PATH="${VK_LAYER_PATH}:${LD_LIBRARY_PATH:-}"
  echo "Validation enabled (VK_LAYER_PATH=${VK_LAYER_PATH})"
else
  echo "WARNING: validation layer json not found at ${LAYER_JSON}"
fi

exec "${BUILD_DIR}/vulkan_app"
