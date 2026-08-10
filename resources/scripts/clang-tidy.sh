#!/usr/bin/env bash
#
# Wrapper around run-clang-tidy that optionally restricts the analysis to a set
# of selected files or to the files changed in git.

set -euo pipefail

die() {
    echo "clang-tidy.sh: $*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: clang-tidy.sh -p <build-dir> [-j <jobs>] [-source-filter <regex>]
       [--fix] [--run-clang-tidy <binary>] [--source-dir <dir>]
       [--git-ref <ref>] [<file>...]

Selection:
  --git-ref <ref>  Only check the files changed relative to <ref>.
                   <ref>=HEAD checks the staged and unstaged changes plus
                   untracked files. Any other ref checks the three-dot diff
                   <ref>...HEAD plus untracked files.
  <file>...        Only check the given files. Arguments are treated as
                   literal paths (matched as a substring of the recorded
                   absolute path), relative to the repository root or absolute.
  (neither)        Check the whole code base (the default).

Options:
  -p <build-dir>          Build directory containing compile_commands.json.
  -j <jobs>               Number of parallel jobs.
  -source-filter <regex>  Restrict analysis to paths matching <regex>.
  --fix                   Apply the suggested fixes.
  --run-clang-tidy <bin>  run-clang-tidy executable (default: the version pinned in
                          resources/scripts/clang-versions, falling back to PATH).
                          May also be set via the RUN_CLANG_TIDY_BIN environment variable.
  --source-dir <dir>      Repository root to resolve git refs and paths against
                          (default: the current directory).
  --dry-run               Only print the run-clang-tidy command that would run.

Notes:
  clang-tidy can only analyze translation units (typically .cpp files).
  Headers are checked transitively through the TUs that include them, so a
  selection containing only headers matches no TU.
EOF
}

# Run git in the repository root, so that relative paths are consistent and
# the script works even when invoked from outside the repository (e.g. from a
# build directory).
repo_dir=""
git_() {
    if [ -n "$repo_dir" ]; then
        git -C "$repo_dir" "$@"
    else
        git "$@"
    fi
}

# Escape the characters that are special in the Python regexes used by
# run-clang-tidy to match file paths.
escape_regex() {
    printf '%s' "$1" | sed 's/[.[\*^$()+?{|]/\\&/g'
}

is_cpp_file() {
    case "$1" in
        *.c|*.cc|*.cpp|*.cxx) return 0 ;;
        *) return 1 ;;
    esac
}

is_header_file() {
    case "$1" in
        *.h|*.hh|*.hpp|*.hxx) return 0 ;;
        *) return 1 ;;
    esac
}

is_source_file() {
    is_cpp_file "$1" || is_header_file "$1"
}

# Reads newline-separated repository-relative paths from stdin and fills the
# globals $files (displayable) and $regex (escaped absolute paths, '|'-joined).
files=()
regex=""
has_cpp=0
has_header=0
build_file_lists() {
    local root file escaped
    root=$(git_ rev-parse --show-toplevel) || die "not inside a git repository"
    while IFS= read -r file; do
        [ -z "$file" ] && continue
        is_source_file "$file" || continue
        case "$file" in
            /*) escaped=$(escape_regex "$file") ;;
            *) escaped=$(escape_regex "$root/$file") ;;
        esac
        regex="${regex:+$regex|}${escaped}"
        files+=("$file")
        is_cpp_file "$file" && has_cpp=1
        is_header_file "$file" && has_header=1
    done
    return 0
}

# Prints the repository-relative paths of the files changed relative to $1.
changed_files() {
    if [ "$1" = "HEAD" ]; then
        git_ diff --name-only HEAD
    else
        git_ merge-base "$1" HEAD >/dev/null 2>&1 || die "invalid git ref: $1"
        git_ diff --name-only "$1"...HEAD
    fi
    git_ ls-files --others --exclude-standard
}

build_dir=""
jobs=""
source_filter=""
fix=0
git_ref=""
git_ref_mode=0
run_clang_tidy_bin=""
dry_run=0

while [ "$#" -gt 0 ]; do
    case "$1" in
        -p) [ "$#" -ge 2 ] || die "option $1 requires an argument"; build_dir=$2; shift 2 ;;
        -j) [ "$#" -ge 2 ] || die "option $1 requires an argument"; jobs=$2; shift 2 ;;
        -source-filter) [ "$#" -ge 2 ] || die "option $1 requires an argument"; source_filter=$2; shift 2 ;;
        -source-filter=*) source_filter=${1#-source-filter=}; shift ;;
        --fix) fix=1; shift ;;
        --git-ref) [ "$#" -ge 2 ] || die "option $1 requires an argument"; git_ref_mode=1; git_ref=${2:-HEAD}; shift 2 ;;
        --git-ref=*) git_ref_mode=1; git_ref=${1#--git-ref=}; git_ref=${git_ref:-HEAD}; shift ;;
        --run-clang-tidy) [ "$#" -ge 2 ] || die "option $1 requires an argument"; run_clang_tidy_bin=$2; shift 2 ;;
        --source-dir) [ "$#" -ge 2 ] || die "option $1 requires an argument"; repo_dir=$2; shift 2 ;;
        --dry-run) dry_run=1; shift ;;
        -h|--help) usage; exit 0 ;;
        --) shift; break ;;
        -*) die "unknown option: $1" ;;
        *) break ;;
    esac
done

# Resolve this script's location so that we can find resources/scripts/clang-versions
# even when the script is invoked from a build directory.
script_dir=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
storm_root="$script_dir/../.."
clang_tidy_version_file=resources/scripts/clang-versions
if [ ! -f "$storm_root/$clang_tidy_version_file" ]; then
    die "could not find '$storm_root/$clang_tidy_version_file'. Have you moved this script?"
fi

# Determine which run-clang-tidy version should be used.
# The version is pinned in resources/scripts/clang-versions so that local runs match CI.
source "$storm_root/$clang_tidy_version_file"
required_clang_tidy_version=$CLANG_TIDY_VERSION
if [ -z "$required_clang_tidy_version" ]; then
    echo "WARNING: Could not determine the clang-tidy version from '$clang_tidy_version_file'."
    echo "         Falling back to whatever 'run-clang-tidy' resolves to on PATH."
fi

# Allow an explicit override, e.g. RUN_CLANG_TIDY_BIN=/opt/homebrew/opt/llvm@22/bin/run-clang-tidy
if [ -z "$run_clang_tidy_bin" ]; then
    run_clang_tidy_bin="${RUN_CLANG_TIDY_BIN:-}"
fi

if [ -n "$run_clang_tidy_bin" ] && [ ! -x "$run_clang_tidy_bin" ]; then
    resolved=$(command -v "$run_clang_tidy_bin" 2>/dev/null || true)
    if [ -n "$resolved" ]; then
        run_clang_tidy_bin="$resolved"
    else
        die "run-clang-tidy binary not found or not executable: $run_clang_tidy_bin"
    fi
fi

# Otherwise, search for a version-matching binary in a few common locations before falling
# back to a plain, unversioned 'run-clang-tidy' on PATH.
if [ -z "$run_clang_tidy_bin" ] && [ -n "$required_clang_tidy_version" ]; then
    # 1. Versioned binary name, as installed by e.g. apt (run-clang-tidy-22) or some package managers.
    if command -v "run-clang-tidy-$required_clang_tidy_version" &> /dev/null; then
        run_clang_tidy_bin="run-clang-tidy-$required_clang_tidy_version"
    # 2. Homebrew's keg-only versioned llvm formula (llvm@22), common on macOS.
    elif command -v brew &> /dev/null; then
        brew_llvm_prefix=$(brew --prefix "llvm@$required_clang_tidy_version" 2> /dev/null || true)
        if [ -n "$brew_llvm_prefix" ] && [ -x "$brew_llvm_prefix/bin/run-clang-tidy" ]; then
            run_clang_tidy_bin="$brew_llvm_prefix/bin/run-clang-tidy"
        fi
    fi
fi

# Fall back to whatever plain 'run-clang-tidy' is on PATH.
if [ -z "$run_clang_tidy_bin" ]; then
    if ! command -v run-clang-tidy &> /dev/null; then
        die "no run-clang-tidy executable found on PATH (looked for a version-matching one and a plain 'run-clang-tidy'). Is it installed?"
    fi
    run_clang_tidy_bin="run-clang-tidy"
    if [ -n "$required_clang_tidy_version" ]; then
        echo "WARNING: Using 'run-clang-tidy' from PATH, but expected clang-tidy version $required_clang_tidy_version"
        echo "         (pinned in resources/scripts/clang-versions). Checks may differ from CI. To match CI exactly,"
        echo "         install run-clang-tidy $required_clang_tidy_version and put it first on PATH or set"
        echo "         RUN_CLANG_TIDY_BIN=/path/to/run-clang-tidy-$required_clang_tidy_version."
    fi
fi

echo "Using run-clang-tidy executable: $run_clang_tidy_bin"

if [ "$git_ref_mode" -eq 1 ]; then
    build_file_lists < <(changed_files "$git_ref")
    if [ "${#files[@]}" -eq 0 ]; then
        echo "No source files changed relative to $git_ref; nothing to check."
        exit 0
    fi
    echo "Checking ${#files[@]} file(s) changed relative to $git_ref:"
    printf '  %s\n' "${files[@]}"
elif [ "$#" -gt 0 ]; then
    build_file_lists < <(printf '%s\n' "$@")
    if [ "${#files[@]}" -eq 0 ]; then
        echo "No source files selected; nothing to check."
        exit 0
    fi
    echo "Checking ${#files[@]} file(s):"
    printf '  %s\n' "${files[@]}"
fi

if [ "$has_header" -eq 1 ] && [ "$has_cpp" -eq 0 ]; then
    echo "Note: the selection contains only headers. clang-tidy checks headers transitively"
    echo "through the translation units that include them, so nothing will be analyzed."
    echo "Run the full target (without a file selection) to check them."
fi

args=()
[ -n "$build_dir" ] && args+=(-p "$build_dir")
[ -n "$jobs" ] && args+=(-j "$jobs")
[ -n "$source_filter" ] && args+=(-source-filter "$source_filter")
[ "$fix" -eq 1 ] && args+=(-fix)

if [ "$dry_run" -eq 1 ]; then
    printf 'PYTHONUNBUFFERED=1 %s' "$run_clang_tidy_bin"
    printf ' %q' "${args[@]}"
    if [ "${#files[@]}" -gt 0 ]; then
        printf ' %q' "$regex"
    fi
    printf '\n'
    exit 0
fi

if [ "${#files[@]}" -gt 0 ]; then
    PYTHONUNBUFFERED=1 "$run_clang_tidy_bin" "${args[@]}" "$regex"
else
    PYTHONUNBUFFERED=1 "$run_clang_tidy_bin" "${args[@]}"
fi
