#!/usr/bin/env bash
# Check that compiled object files actually carry LTO intermediate representation.
#
# Usage: check-lto.sh <dir-or-object-file>...
#   Directories are searched recursively for *.o files; every object must contain LTO IR.
#
# Recognized LTO objects:
#   * Clang -flto / -flto=thin  : the .o is LLVM bitcode (raw or Apple wrapper magic)
#   * Clang -ffat-lto-objects   : ELF object with a .llvm.lto section
#   * GCC -flto (slim or fat)   : ELF object with .gnu.lto_* sections
set -euo pipefail

if [ $# -eq 0 ]; then
    echo "Usage: $0 <dir-or-object-file>..." >&2
    exit 2
fi

# Classify one object file. Prints a description; returns 0 if it contains LTO IR, 1 otherwise.
check_object() {
    local obj=$1 magic sections
    magic=$(od -An -tx1 -N4 "$obj" | tr -d ' \n')
    case "$magic" in
        4243c0de) return 0 ;;  # LLVM bitcode
        dec0170b) return 0 ;;  # LLVM bitcode (wrapper)
        7f454c46)
            sections=$(readelf -SW "$obj")
            case "$sections" in
                *.gnu.lto_*) return 0 ;;  # ELF with GCC LTO sections (.gnu.lto_*)
                *.llvm.lto*) return 0 ;;  # ELF with LLVM fat-LTO section (.llvm.lto)
            esac
            echo "ELF without LTO sections"; return 1 ;;
        cffaedfe|cefaedfe) echo "Mach-O without LTO IR"; return 1 ;;
        *) echo "unrecognized format (magic $magic)"; return 1 ;;
    esac
}

total=0
failed=0
while IFS= read -r -d '' obj; do
    total=$((total + 1))
    if ! desc=$(check_object "$obj"); then
        failed=$((failed + 1))
        echo "Error: no LTO IR in $obj: $desc"
    fi
done < <(find "$@" -type f -name '*.o' -print0)

if [ "$total" -eq 0 ]; then
    echo "Error: no object files found under '$*' to inspect for LTO IR."
    exit 1
fi
if [ "$failed" -gt 0 ]; then
    echo "Error: $failed of $total object files do not contain LTO IR."
    exit 1
fi
echo "LTO IR confirmed in all $total object files."
