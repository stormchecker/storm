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

# Whether the given flag occurs as a separate word in the file
has_flag() {
    grep -q -E -e "(^|[[:space:]])$1([[:space:]]|$)" "$2"
}

for file in "$flags" "$link"; do
    [ -f "$file" ] || fail "File '$file' not found."
done

case "$build_type" in
    Release)
        if has_flag '-g' "$flags"; then fail "Unexpected flag '-g' for release build."; fi
        if ! has_flag '-O3' "$flags"; then fail "Missing flag '-O3' for release build."; fi
        if ! has_flag '-DNDEBUG' "$flags"; then fail "Missing flag '-DNDEBUG' for release build."; fi
        if ! has_flag '-flto(=[^[:space:]]*)?' "$flags"; then fail "Missing '-flto' compile flag for release build."; fi
        if ! has_flag '-flto(=[^[:space:]]*)?' "$link"; then fail "Missing '-flto' link flag for release build."; fi
        bash "$(dirname "${BASH_SOURCE[0]}")/check-lto.sh" "$dir"
        ;;
    Debug)
        if ! has_flag '-g' "$flags"; then fail "Missing flag '-g' for debug build."; fi
        if has_flag '-O3' "$flags"; then fail "Unexpected flag '-O3' for debug build."; fi
        if has_flag '-DNDEBUG' "$flags"; then fail "Unexpected flag '-DNDEBUG' for debug build."; fi
        ;;
    *)
        echo "No compile flag checks for build type '$build_type'."
        exit 0
        ;;
esac
echo "Compile flags for $build_type build are as expected."
