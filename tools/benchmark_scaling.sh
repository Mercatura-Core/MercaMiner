#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(
    cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &&
    pwd
)"
REPO_ROOT="$(
    cd -- "$SCRIPT_DIR/.." &&
    pwd
)"

cd "$REPO_ROOT"

DURATION="${1:-10}"
REPS="${2:-3}"

if (($# >= 3)); then
    THREADS=("${@:3}")
else
    THREADS=(1 2 4 6 8 12)
fi

if ! [[ "$DURATION" =~ ^[1-9][0-9]*$ ]] ||
   ((DURATION > 3600)); then
    echo "duration must be an integer from 1 to 3600 seconds" >&2
    exit 2
fi

if ! [[ "$REPS" =~ ^[1-9][0-9]*$ ]]; then
    echo "repetitions must be a positive integer" >&2
    exit 2
fi

for threads in "${THREADS[@]}"; do
    if ! [[ "$threads" =~ ^[1-9][0-9]*$ ]]; then
        echo "thread counts must be positive integers" >&2
        exit 2
    fi
done

OUT="${TMPDIR:-/tmp}/mercaminer-benchmark"
rm -rf "$OUT"
mkdir -p "$OUT"

echo "MercaMiner scaling benchmark"
echo "measurement_seconds=$DURATION"
echo "repetitions=$REPS"
echo "thread_counts=${THREADS[*]}"
echo

echo "System:"
lscpu | grep -E \
    'Model name|Socket|Core\(s\) per socket|Thread\(s\) per core|CPU\(s\):' \
    || true
echo

for threads in "${THREADS[@]}"; do
    for ((rep = 1; rep <= REPS; ++rep)); do
        LOG="$OUT/t${threads}-r${rep}.log"

        echo "Running threads=$threads repetition=$rep ..."

        /usr/bin/time -v \
            ./build/mercaminer_benchmark \
            "$threads" \
            "$DURATION" \
            >"$LOG" 2>&1

        HPS=$(
            grep '^hashes_per_second=' "$LOG" |
            cut -d= -f2
        )

        PER_WORKER=$(
            grep '^hashes_per_worker_second=' "$LOG" |
            cut -d= -f2
        )

        HASHES=$(
            grep '^hashes=' "$LOG" |
            cut -d= -f2
        )

        RSS=$(
            grep 'Maximum resident set size' "$LOG" |
            awk '{print $6}'
        )

        CPU=$(
            grep 'Percent of CPU this job got' "$LOG" |
            awk '{print $7}'
        )

        echo \
            "threads=$threads rep=$rep hashes=$HASHES hps=$HPS per_worker=$PER_WORKER rss_kib=$RSS cpu=$CPU"

        sleep 3
    done

    echo
done

echo "Raw logs: $OUT"
