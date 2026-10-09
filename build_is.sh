#!/usr/bin/env bash
# Builds the IS benchmark (pydrofoil_app) for every combination of problem
# class and core count and copies the ELFs to the vcml-pydrofoil benchmark
# folder as zephyr_is_<class>_<n>cores.elf.
#
#   ./build_is.sh                          all classes, 1/2/4/8 cores
#   ./build_is.sh -c "M W" -n "1 4"        only classes M and W on 1 and 4 cores
#   ./build_is.sh -o /some/dir             other output folder
#
# Per build the core count is set in all three places Zephyr needs it:
# NRCPU (board devicetree), CONFIG_MP_MAX_NUM_CPUS and NUM_CORES (main.c).
# The RAM size is fixed in the board devicetree (256 MB, enough up to class A);
# a too large class fails at link time with "region `RAM' overflowed".

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ZEPHYR_PROJECT="${ZEPHYR_PROJECT:-$HOME/thesis/zephyrproject}"
OUT_DIR="$SCRIPT_DIR/../vcml-pydrofoil/benchmark/zephyr_is/build"
BUILD_ROOT="$SCRIPT_DIR/benchmark/build_is"
CLASSES="M S P W A"
CORES="1 2 4 8"

while getopts "c:n:o:h" opt; do
    case $opt in
        c) CLASSES="$OPTARG" ;;
        n) CORES="$OPTARG" ;;
        o) OUT_DIR="$OPTARG" ;;
        *) sed -n '2,13p' "$0"; exit 1 ;;
    esac
done

# shellcheck disable=SC1091
source "$ZEPHYR_PROJECT/.venv/bin/activate" || { echo "No venv in $ZEPHYR_PROJECT"; exit 1; }
mkdir -p "$OUT_DIR" "$BUILD_ROOT"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

failed=()
for class in $CLASSES; do
    for n in $CORES; do
        name="zephyr_is_${class}_${n}cores"
        build_dir="$BUILD_ROOT/${class}_${n}"
        log="$build_dir.log"
        echo "=== $name"

        (cd "$ZEPHYR_PROJECT/zephyr" && west build -b pydrofoil_32 "$SCRIPT_DIR/pydrofoil_app" \
            --pristine -d "$build_dir" -- \
            -DBOARD_ROOT="$SCRIPT_DIR" -DSOC_ROOT="$SCRIPT_DIR" \
            -DDTS_EXTRA_CPPFLAGS="-DNRCPU=$n" \
            -DCONFIG_MP_MAX_NUM_CPUS="$n" \
            -DIS_NUM_CORES="$n" -DIS_CLASS="$class") > "$log" 2>&1

        if [[ $? -ne 0 || ! -f "$build_dir/zephyr/zephyr.elf" ]]; then
            echo "    FAILED, see $log"
            grep -m3 -E "error|overflowed" "$log" | sed 's/^/    /'
            failed+=("$name")
            continue
        fi

        grep -E "^\s+RAM:" "$log" | sed 's/^ */    /'
        cp "$build_dir/zephyr/zephyr.elf" "$OUT_DIR/$name.elf"
    done
done

echo
echo "ELFs in $OUT_DIR"
if [[ ${#failed[@]} -gt 0 ]]; then
    echo "Failed: ${failed[*]}"
    exit 1
fi
