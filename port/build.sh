#!/usr/bin/env bash
# Build libiistupid.so for Quest (arm64-v8a).
# Requires: Android NDK r25+ exported as NDK_HOME or ANDROID_NDK_HOME.

set -euo pipefail

NDK="${NDK_HOME:-${ANDROID_NDK_HOME:-}}"
if [[ -z "${NDK}" ]]; then
    echo "error: set NDK_HOME or ANDROID_NDK_HOME to your Android NDK path" >&2
    exit 1
fi

BUILD_TYPE="${1:-Release}"
ABI="${ABI:-arm64-v8a}"
API="${API:-29}"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build/${ABI}-${BUILD_TYPE}"
mkdir -p "${BUILD_DIR}"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${NDK}/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="${ABI}" \
    -DANDROID_PLATFORM="android-${API}" \
    -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
    -GNinja

cmake --build "${BUILD_DIR}" --parallel

OUT="${SCRIPT_DIR}/out/${ABI}"
mkdir -p "${OUT}"
cp "${BUILD_DIR}/libiistupid.so" "${OUT}/libiistupid.so"
echo "built: ${OUT}/libiistupid.so"
