#!/bin/bash

SCRIPT="$(cd "$(dirname "$0")" && pwd)/test3.py"
DATA_BASE="/home/onoda/ANLNext/Erica_git_dev/EricaNext/mount_data/storage_g0"
OUTPUT_DIR="/home/onoda/ANLNext/Erica_git_dev/EricaNext/root_data/board/output20_0618"
TMP_SCRIPT="/tmp/test3_run.py"

mkdir -p "$OUTPUT_DIR"

for data_dir in "$DATA_BASE"/run_20260618*/; do
    data_dir="${data_dir%/}"
    run_name="$(basename "$data_dir")"
    out_file="$OUTPUT_DIR/output_${run_name}.root"

    echo "========================================="
    echo "Processing: $run_name"
    echo "Output:     $out_file"
    echo "========================================="

    sed \
        -e "s|^\(\s\+\"data_dir\"\): \".*\"|\1: \"$data_dir\"|" \
        -e "s|\"filename\": \".*\"|\"filename\": \"$out_file\"|" \
        "$SCRIPT" > "$TMP_SCRIPT"

    python3 "$TMP_SCRIPT"
    status=$?

    if [ $status -ne 0 ]; then
        echo "ERROR: exited with status $status for $run_name"
    fi
done

echo "All runs done."
