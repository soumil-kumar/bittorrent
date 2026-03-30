#!/bin/sh
#
# Use this script to run your program LOCALLY.
#
# Note: Changing this script WILL NOT affect how CodeCrafters runs your program.
#
# Learn more: https://codecrafters.io/program-interface

set -e # Exit early if any commands fail

# Copied from .codecrafters/compile.sh
#
# - Edit this to change how your program compiles locally
# - Edit .codecrafters/compile.sh to change how your program compiles remotely
(
  cd "$(dirname "$0")" # Ensure compile steps are run within the repository directory
  cmake -B build -S . \
    -DCMAKE_C_COMPILER="C:/minGW/bin/gcc.exe" \
    -DCMAKE_CXX_COMPILER="C:/minGW/bin/g++.exe" \
    -DCMAKE_TOOLCHAIN_FILE="C:/Users/hello/vcpkg/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic \
    -DVCPKG_INSTALLED_DIR="C:/vcpkg_installed" \
    -DCMAKE_PREFIX_PATH="C:/vcpkg_installed/x64-mingw-dynamic"
  cmake --build ./build
)

# Copied from .codecrafters/run.sh
#
# - Edit this to change how your program runs locally
# - Edit .codecrafters/run.sh to change how your program runs remotely
exec $(dirname "$0")/build/bittorrent "$@"
