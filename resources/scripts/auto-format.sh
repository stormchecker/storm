#!/usr/bin/env bash

set -euo pipefail

# Settings
auto_format_file_extensions=( .h .cpp )
auto_format_src_dir=./src
# File that pins the clang-format version
auto_format_version_file=resources/scripts/clang-versions

# Command-line flags
auto_format_check_only=0
case "${1:-}" in
	--check)
		auto_format_check_only=1
		;;
	--help|-h)
		echo "Usage: auto-format.sh [--check]"
		echo ""
		echo "Formats (or with --check, only verifies) all source files under src/ using clang-format."
		echo ""
		echo "Options:"
		echo "  --check     Do not modify any files; exit with a non-zero status if any file would be"
		echo "              reformatted (uses clang-format's --dry-run -Werror)."
		echo "  -h, --help  Show this help and exit."
		exit 0
		;;
	"")
		;;
	*)
		echo "ERROR: Unknown option '$1'."
		echo "Usage: auto-format.sh [--check]"
		exit 1
		;;
esac

# Set-up directories
script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
storm_root="$script_dir/../.."

# Sanity checks
for expected_file in .clang-format .clang-format-ignore $auto_format_version_file
do
	if [ ! -f $storm_root/$expected_file ]; then
    	echo "ERROR: There does not seem to be a file '$storm_root/$expected_file'. Have you moved this script?"
		exit 1
	fi
done

# go to correct directory
cd $storm_root

# Determine which clang-format version should be used.
# Different clang-format versions can format identical input differently, which is a common
# source of "it's formatted locally but CI still complains" confusion.
source "$storm_root/$auto_format_version_file"
required_clang_format_version=${CLANG_FORMAT_VERSION:-}
if [ -z "$required_clang_format_version" ]; then
	echo "WARNING: Could not determine the clang-format version from '$auto_format_version_file'."
	echo "         Falling back to whatever 'clang-format' resolves to on PATH."
fi

# Allow an explicit override, e.g. CLANG_FORMAT_BIN=/opt/homebrew/opt/llvm@20/bin/clang-format
clang_format_bin="${CLANG_FORMAT_BIN:-}"

# Otherwise, search for a version-matching binary in a few common locations before falling
# back to a plain, unversioned 'clang-format' on PATH.
if [ -z "$clang_format_bin" ] && [ -n "$required_clang_format_version" ]; then
	# 1. Versioned binary name, as installed by e.g. apt (clang-format-20) or some package managers.
	if command -v "clang-format-$required_clang_format_version" &> /dev/null; then
		clang_format_bin="clang-format-$required_clang_format_version"
	# 2. Homebrew's keg-only versioned llvm formula (llvm@20), common on macOS.
	elif command -v brew &> /dev/null; then
		brew_llvm_prefix=$(brew --prefix "llvm@$required_clang_format_version" 2> /dev/null || true)
		if [ -n "$brew_llvm_prefix" ] && [ -x "$brew_llvm_prefix/bin/clang-format" ]; then
			clang_format_bin="$brew_llvm_prefix/bin/clang-format"
		fi
	fi
fi

# Fall back to whatever plain 'clang-format' is on PATH.
if [ -z "$clang_format_bin" ]; then
	if ! command -v clang-format &> /dev/null; then
		echo "ERROR: Unable to find a clang-format executable (looked for a version-matching one and a plain 'clang-format'). Is it installed?"
		if [ -n "$required_clang_format_version" ]; then
			echo "       CI uses clang-format $required_clang_format_version. On macOS: 'brew install llvm@$required_clang_format_version'."
			echo "       Alternatively install it via pip: 'pip install clang-format==$required_clang_format_version.*'."
		fi
		exit 1
	fi
	clang_format_bin="clang-format"
	if [ -n "$required_clang_format_version" ]; then
		found_version=$("$clang_format_bin" --version | grep -o '[0-9]\+' | head -1 || true)
		if [ "$found_version" != "$required_clang_format_version" ]; then
			echo "WARNING: Using 'clang-format' version $found_version, but expected formatting with version $required_clang_format_version."
			echo "         Formatting may differ from what CI expects. To match CI exactly, install clang-format $required_clang_format_version"
			echo "         (e.g. 'brew install llvm@$required_clang_format_version' on macOS) and either put it first on PATH or set"
			echo "         CLANG_FORMAT_BIN=/path/to/clang-format-$required_clang_format_version."
		fi
	fi
fi

echo "Using clang-format executable: $clang_format_bin ($("$clang_format_bin" --version))"

# Build an expression for the find command
# 1. look in the correct directory
auto_format_find_expression="$auto_format_src_dir ( ("
# 2. the file should have one of the specified file extensions
for extension in "${auto_format_file_extensions[@]}"
do
	auto_format_find_expression+=" -name *$extension -or"
done
# 3. the file path should not match one of the excluding patterns in .clang-format-ignore
auto_format_find_expression+=" -false ) -and -not ("
while read exclude_pattern || [[ -n "$exclude_pattern" ]]
do
	if [[ -z "$exclude_pattern" || "$exclude_pattern" == \#* ]]
	then 
		continue #ignore white spaces and lines starting with #
	fi 
	auto_format_find_expression+=" -path $exclude_pattern -or"
done < .clang-format-ignore
auto_format_find_expression+=" -false ) ) -print"

# disable bash expansion of *
set -f 

# find files and invoke clang-format
# (in check mode, do not modify anything, but fail if a file would be reformatted)
if [ "$auto_format_check_only" = "1" ]; then
	if find $auto_format_find_expression | xargs "$clang_format_bin" --dry-run -Werror -style=file; then
		exit 0
	else
		echo "Some files are not properly formatted. Run 'auto-format.sh' (or 'make format') to fix them."
		exit 1
	fi
else
	find $auto_format_find_expression | xargs "$clang_format_bin" -i -style=file
fi
