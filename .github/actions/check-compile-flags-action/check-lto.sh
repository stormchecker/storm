#!/usr/bin/env bash
# Check that a compiled object file actually carries embedded LTO intermediate representation
#
# Usage: check-lto-ir.sh <dir-containing-object-files>
set -euo pipefail

search_dir="$1"

obj=$(find "$search_dir" -name '*.o' -print -quit)
if [ -z "$obj" ]; then
    echo "Error: no object files found under '$search_dir' to inspect for LTO IR."
    exit 1
fi

file_out=$(file "$obj")

if echo "$file_out" | grep -qi 'LLVM'; then
    # Clang's default (non-fat) -flto/-flto=thin objects are pure LLVM bitcode files.
    echo "LTO IR confirmed: $obj is LLVM bitcode ($file_out)"
    exit 0
elif echo "$file_out" | grep -q 'Mach-O'; then
    # Apple clang wraps embedded bitcode in a __LLVM,__bitcode segment.
    if otool -l "$obj" 2>/dev/null | grep -q '__bitcode'; then
        echo "LTO IR confirmed: $obj contains an embedded __LLVM,__bitcode segment"
        exit 0
    fi
elif echo "$file_out" | grep -q 'ELF'; then
    # GCC's (and Linux Clang fat-object) slim LTO objects embed GIMPLE/bitcode
    # in .gnu.lto_* ELF sections instead of (or alongside) real machine code.
    if readelf -S "$obj" 2>/dev/null | grep -q '\.gnu\.lto_'; then
        echo "LTO IR confirmed: $obj contains .gnu.lto_* sections"
        exit 0
    fi
fi

echo "Error: object file $obj does not appear to contain embedded LTO IR ($file_out)."
exit 1
