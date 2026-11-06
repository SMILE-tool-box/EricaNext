#!/bin/bash
# Run run_chain.py on every run directory that matches a pattern (one output file per run).
#   bash run_runs.sh <config> <output dir> <run directory>...
#   bash run_runs.sh smile3_tpc_only ../root_data/board/output20_0618 ../mount_data/storage_g0/run_20260618*
# Output: <output dir>/output_<run name>.root. Extra run_chain.py options go after --:
#   bash run_runs.sh smile3_tpc_only out ../mount_data/storage_g0/run_2026061* -- --set CalcFADC.threshold=30.0 -n 10000
# A failed run is reported and the next one is processed; the exit code is 1 if any run failed.
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
if [ $# -lt 3 ]; then
  sed -n 2,7p "$0"; exit 1
fi
CONFIG=$1; OUTDIR=$2; shift 2
RUNS=()
while [ $# -gt 0 ] && [ "$1" != "--" ]; do RUNS+=("${1%/}"); shift; done
[ "${1:-}" = "--" ] && shift
mkdir -p "$OUTDIR"

failed=()
for run in "${RUNS[@]}"; do
  name=$(basename "$run")
  out="$OUTDIR/output_${name}.root"
  echo "========================================="
  echo "Processing: $name"
  echo "Output:     $out"
  echo "========================================="
  if ! python3 "$HERE/run_chain.py" "$CONFIG" -o "$out" --data-dir "$run" "$@"; then
    echo "ERROR: $name failed"
    failed+=("$name")
  fi
done

echo "All runs done (${#RUNS[@]} runs, ${#failed[@]} failed)."
[ ${#failed[@]} -eq 0 ] || { printf '  failed: %s\n' "${failed[@]}"; exit 1; }
