#!/usr/bin/env bash
# Check the compile flags of Storm for the given build type.
#
# Usage: check-flags.sh <storm.dir> <build-type>
#   <storm.dir>   CMake target directory of Storm, e.g. build/src/storm/CMakeFiles/storm.dir
#   <build-type>  CMake build type (Release or Debug)
#
# Release: no '-g', '-O3', '-DNDEBUG' and LTO ('-flto' compile/link flag and LTO IR in the object files)
# Debug:   '-g', no '-O3', no '-DNDEBUG'
set -euo pipefail

if [ $# -ne 2 ]; then
    echo "Usage: $0 <storm.dir> <build-type>" >&2
    exit 2
fi
dir=$1
build_type=$2
flags="$dir/flags.make"
link="$dir/link.txt"

fail() {
    echo "Error: $1" >&2
    exit 1
}

case "$build_type" in
    Release)
        if grep -q -e '-g' "$flags"; then fail "Unexpected flag '-g' for release build."; fi
        if ! grep -q -e '-O3' "$flags"; then fail "Missing flag '-O3' for release build."; fi
        if ! grep -q -e '-DNDEBUG' "$flags"; then fail "Missing flag '-DNDEBUG' for release build."; fi
        if ! grep -q -e '-flto' "$flags"; then fail "Missing '-flto' compile flag for release build."; fi
        if ! grep -q -e '-flto' "$link"; then fail "Missing '-flto' link flag for release build."; fi
        bash "$(dirname "${BASH_SOURCE[0]}")/check-lto.sh" "$dir"
        ;;
    Debug)
        if ! grep -q -e '-g' "$flags"; then fail "Missing flag '-g' for debug build."; fi
        if grep -q -e '-O3' "$flags"; then fail "Unexpected flag '-O3' for debug build."; fi
        if grep -q -e '-DNDEBUG' "$flags"; then fail "Unexpected flag '-DNDEBUG' for debug build."; fi
        ;;
    *)
        echo "No compile flag checks for build type '$build_type'."
        ;;
esac
echo "Compile flags for $build_type build are as expected."
