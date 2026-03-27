#!/bin/sh
#
# This script is used to compile your program on CodeCrafters
#
# This runs before .codecrafters/run.sh
#
# Learn more: https://codecrafters.io/program-interface

set -e # Exit on failure

# Keep Codecrafters default behavior, but allow local MSYS2 curl overrides.
EXTRA_CMAKE_ARGS=""
TOOLCHAIN_ARG=""

# Avoid stale generator/compiler cache from previous local attempts.
if [ -f build/CMakeCache.txt ]; then
	rm -rf build
fi

if [ -f /c/msys64/ucrt64/lib/libcurl.dll.a ]; then
	EXTRA_CMAKE_ARGS="${EXTRA_CMAKE_ARGS} -DCURL_INCLUDE_DIR=/c/msys64/ucrt64/include -DCURL_LIBRARY=/c/msys64/ucrt64/lib/libcurl.dll.a"
fi

if [ -n "${VCPKG_ROOT}" ]; then
	TOOLCHAIN_ARG="-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
fi

OS_NAME="$(uname -s 2>/dev/null || echo '')"
if [ -z "${CMAKE_GENERATOR}" ] && echo "${OS_NAME}" | grep -Eiq "mingw|msys|cygwin" && ! command -v nmake >/dev/null 2>&1 && command -v g++ >/dev/null 2>&1; then
	cmake -B build -S . -G "MinGW Makefiles" ${TOOLCHAIN_ARG} ${EXTRA_CMAKE_ARGS}
else
	cmake -B build -S . ${TOOLCHAIN_ARG} ${EXTRA_CMAKE_ARGS}
fi

cmake --build ./build
