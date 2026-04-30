#!/usr/bin/env bash
set -euo pipefail

# Benchmark Storm compile time across:
# - PCH enabled/disabled
# - ccache cold/warm
#
# The script uses ccache through CMake compiler launchers and controls PCH via
# CMAKE_DISABLE_PRECOMPILE_HEADERS, so all four combinations are measurable.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SOURCE_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_ROOT="${SOURCE_DIR}/build-bench-pch-ccache"
CCACHE_ROOT=""
BUILD_TYPE="Release"
TARGET="storm"
JOBS="$(command -v nproc >/dev/null 2>&1 && nproc || echo 4)"
GENERATOR=""
EXTRA_CMAKE_ARGS=()
ENABLE_TRACE=0
NINJATRACING_BIN=""
NINJATRACING_URL="https://raw.githubusercontent.com/nico/ninjatracing/master/ninjatracing"

usage() {
    cat <<'EOF'
Usage: benchmark-pch-ccache.sh [options]

Options:
    --source-dir <path>     Storm source directory (default: repo root)
    --build-root <path>     Directory for benchmark build trees/results
    --ccache-root <path>    Directory for benchmark ccache data (default: <build-root>/ccache)
    --build-type <type>     CMake build type (default: Release)
    --target <target>       Build target (default: storm)
    --jobs <n>              Parallel build jobs (default: nproc or 4)
    --generator <name>      CMake generator (e.g. Ninja)
    --cmake-arg <arg>       Extra CMake argument (can be repeated)
    --trace                 Generate Chrome trace files via ninjatracing
    --ninjatracing <path>   Path to local ninjatracing script/binary
    --help                  Show this help

Output:
  - Summary table in stdout
  - CSV at <build-root>/results.csv
  - Per-run ccache stats in <build-root>/results/
    - Optional traces at <build-root>/results/trace-<pch>-<cache>.json
EOF
}

require_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "Error: required command '$1' not found" >&2
        exit 1
    fi
}

extract_stat() {
    local label="$1"
    local file="$2"
    awk -F: -v key="$label" '
        $1 ~ key {
            gsub(/^[[:space:]]+|[[:space:]]+$/, "", $2)
            gsub(/[[:space:]]/, "", $2)
            print $2
            found=1
            exit
        }
        END { if (!found) print 0 }
    ' "$file"
}

run_case() {
    local pch_mode="$1"      # on|off
    local cache_state="$2"   # cold|warm
    local namespace="storm-bench-pch-${pch_mode}"
    local ccache_dir="${CCACHE_ROOT}/${namespace}"
    local enable_pch="ON"

    if [[ "$pch_mode" == "off" ]]; then
        enable_pch="OFF"
    fi

    local build_dir="${BUILD_ROOT}/build-${pch_mode}"
    local result_dir="${BUILD_ROOT}/results"
    mkdir -p "$result_dir"

    export CCACHE_DIR="$ccache_dir"
    export CCACHE_NAMESPACE="$namespace"
    export CCACHE_BASEDIR="$SOURCE_DIR"
    export CCACHE_NOHASHDIR="1"

    if [[ "$cache_state" == "cold" ]]; then
        rm -rf "$ccache_dir"
        rm -rf "$build_dir"
    fi
    mkdir -p "$ccache_dir"
    ccache --zero-stats >/dev/null

    local cmake_cmd=(cmake -S "$SOURCE_DIR" -B "$build_dir"
        -DSTORM_COMPILE_WITH_PCH="$enable_pch"
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE")

    if [[ -n "$GENERATOR" ]]; then
        cmake_cmd+=(-G "$GENERATOR")
    fi

    if [[ ${#EXTRA_CMAKE_ARGS[@]} -gt 0 ]]; then
        cmake_cmd+=("${EXTRA_CMAKE_ARGS[@]}")
    fi

    if [[ "$cache_state" == "cold" || ! -d "$build_dir" ]]; then
        echo "[benchmark] Configure: pch=${pch_mode}, cache=${cache_state}"
        "${cmake_cmd[@]}" >/dev/null
    fi

    if [[ "$cache_state" == "warm" ]]; then
        # Rebuild in the same build directory to keep paths stable for ccache.
        cmake --build "$build_dir" --target clean -j "$JOBS" >/dev/null || true
    fi

    local time_file="${result_dir}/time-${pch_mode}-${cache_state}.txt"
    echo "[benchmark] Build:     pch=${pch_mode}, cache=${cache_state}"
    /usr/bin/time -f "%e" -o "$time_file" \
        cmake --build "$build_dir" --target "$TARGET" -j "$JOBS" >/dev/null

    local stats_file="${result_dir}/ccache-${pch_mode}-${cache_state}.txt"
    ccache --show-stats --verbose >"$stats_file"

    if [[ "$ENABLE_TRACE" == "1" ]]; then
        local ninja_log="${build_dir}/.ninja_log"
        local trace_file="${result_dir}/trace-${pch_mode}-${cache_state}.json"
        if [[ ! -f "$ninja_log" ]]; then
            echo "Error: ninja log not found at $ninja_log" >&2
            exit 1
        fi
        python3 "$NINJATRACING_BIN" "$ninja_log" >"$trace_file"
    fi

    local elapsed
    elapsed="$(cat "$time_file")"

    local cacheable hits misses uncacheable
    cacheable="$(extract_stat "Cacheable calls" "$stats_file")"
    hits="$(extract_stat "Hits" "$stats_file")"
    misses="$(extract_stat "Misses" "$stats_file")"
    uncacheable="$(extract_stat "Uncacheable calls" "$stats_file")"

    local hit_rate="0.00"
    if [[ "$cacheable" != "0" ]]; then
        hit_rate="$(awk -v h="$hits" -v c="$cacheable" 'BEGIN { printf "%.2f", (100.0*h)/c }')"
    fi

    printf "%s,%s,%s,%s,%s,%s,%s\n" \
        "$pch_mode" "$cache_state" "$elapsed" "$cacheable" "$hits" "$misses" "$uncacheable" \
        >>"${BUILD_ROOT}/results.csv"

    printf "%-8s %-6s %10s %12s %10s %10s %12s %9s\n" \
        "$pch_mode" "$cache_state" "$elapsed" "$cacheable" "$hits" "$misses" "$uncacheable" "${hit_rate}%"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --source-dir)
            SOURCE_DIR="$2"
            shift 2
            ;;
        --build-root)
            BUILD_ROOT="$2"
            shift 2
            ;;
        --ccache-root)
            CCACHE_ROOT="$2"
            shift 2
            ;;
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --target)
            TARGET="$2"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --generator)
            GENERATOR="$2"
            shift 2
            ;;
        --cmake-arg)
            EXTRA_CMAKE_ARGS+=("$2")
            shift 2
            ;;
        --trace)
            ENABLE_TRACE=1
            shift
            ;;
        --ninjatracing)
            NINJATRACING_BIN="$2"
            shift 2
            ;;
        --help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1" >&2
            usage
            exit 1
            ;;
    esac
done

require_cmd cmake
require_cmd ccache
require_cmd /usr/bin/time

if [[ "$ENABLE_TRACE" == "1" ]]; then
    require_cmd python3
    if [[ -z "$GENERATOR" ]]; then
        GENERATOR="Ninja"
    fi
    if [[ "$GENERATOR" != "Ninja" ]]; then
        echo "Error: --trace requires --generator Ninja (or no generator, which defaults to Ninja)." >&2
        exit 1
    fi
    require_cmd ninja
fi

if [[ -z "$CCACHE_ROOT" ]]; then
    CCACHE_ROOT="${BUILD_ROOT}/ccache"
fi

mkdir -p "$BUILD_ROOT"
mkdir -p "$CCACHE_ROOT"
: >"${BUILD_ROOT}/results.csv"
echo "pch,cache_state,build_seconds,cacheable_calls,hits,misses,uncacheable_calls" >>"${BUILD_ROOT}/results.csv"

if [[ "$ENABLE_TRACE" == "1" ]]; then
    if [[ -z "$NINJATRACING_BIN" ]]; then
        NINJATRACING_BIN="${BUILD_ROOT}/tools/ninjatracing"
        mkdir -p "${BUILD_ROOT}/tools"
        if [[ ! -f "$NINJATRACING_BIN" ]]; then
            if command -v curl >/dev/null 2>&1; then
                curl -fsSL "$NINJATRACING_URL" -o "$NINJATRACING_BIN"
            elif command -v wget >/dev/null 2>&1; then
                wget -qO "$NINJATRACING_BIN" "$NINJATRACING_URL"
            else
                echo "Error: neither curl nor wget found; provide --ninjatracing <path>." >&2
                exit 1
            fi
            chmod +x "$NINJATRACING_BIN"
        fi
    fi
fi

echo
echo "Benchmark settings"
echo "  source-dir:  $SOURCE_DIR"
echo "  build-root:  $BUILD_ROOT"
echo "  ccache-root: $CCACHE_ROOT"
echo "  build-type:  $BUILD_TYPE"
echo "  target:      $TARGET"
echo "  jobs:        $JOBS"
if [[ -n "$GENERATOR" ]]; then
    echo "  generator:   $GENERATOR"
fi
if [[ "$ENABLE_TRACE" == "1" ]]; then
    echo "  trace:       enabled"
    echo "  ninjatrace:  $NINJATRACING_BIN"
else
    echo "  trace:       disabled"
fi
if [[ ${#EXTRA_CMAKE_ARGS[@]} -gt 0 ]]; then
    echo "  extra args:  ${EXTRA_CMAKE_ARGS[*]}"
fi

echo
echo "Results"
printf "%-8s %-6s %10s %12s %10s %10s %12s %9s\n" \
    "pch" "cache" "seconds" "cacheable" "hits" "misses" "uncacheable" "hit-rate"
printf "%-8s %-6s %10s %12s %10s %10s %12s %9s\n" \
    "--------" "------" "----------" "------------" "----------" "----------" "------------" "---------"

run_case off cold
run_case off warm
run_case on cold
run_case on warm

echo
echo "CSV written to: ${BUILD_ROOT}/results.csv"
echo "Raw stats in:   ${BUILD_ROOT}/results"
if [[ "$ENABLE_TRACE" == "1" ]]; then
    echo "Traces in:      ${BUILD_ROOT}/results"
fi
