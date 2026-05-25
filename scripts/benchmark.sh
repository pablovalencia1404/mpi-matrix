#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CLUSTER_SH="${ROOT_DIR}/cluster.sh"
OUTPUT_CSV="${1:-${ROOT_DIR}/results.csv}"

SIZES=${SIZES:-"1024 2048 4096"}
REPEATS=${REPEATS:-3}
VARIANTS=${VARIANTS:-"mpi_matrix"}

printf "variant,nodes,np,ppn,size,run,seconds,gflops,checksum\n" > "$OUTPUT_CSV"

run_case() {
    local variant="$1"
    local nodes="$2"
    local np="$3"
    local ppn="$4"
    local size="$5"

    local run=1
    while [ "$run" -le "$REPEATS" ]; do
        echo "Ejecutando variant=${variant} nodes=${nodes} np=${np} ppn=${ppn} size=${size} run=${run}/${REPEATS}" >&2
        out=$($CLUSTER_SH exec "mpirun -np ${np} -ppn ${ppn} ./${variant} ${size}")

        seconds=$(printf "%s\n" "$out" | sed -n 's/.*seconds=\([0-9.]*\).*/\1/p')
        gflops=$(printf "%s\n" "$out" | sed -n 's/.*gflops=\([0-9.]*\).*/\1/p')
        checksum=$(printf "%s\n" "$out" | sed -n 's/.*checksum=\([0-9.]*\).*/\1/p')

        if [ -z "$seconds" ] || [ -z "$gflops" ] || [ -z "$checksum" ]; then
            echo "No se pudo parsear salida de mpirun:"
            echo "$out"
            exit 1
        fi

        printf "%s,%s,%s,%s,%s,%s,%s,%s,%s\n" \
            "$variant" "$nodes" "$np" "$ppn" "$size" "$run" "$seconds" "$gflops" "$checksum" >> "$OUTPUT_CSV"

        run=$((run + 1))
    done
}

trap '$CLUSTER_SH down >/dev/null 2>&1 || true' EXIT

$CLUSTER_SH up size=1

for variant in $VARIANTS; do
    for size in $SIZES; do
        run_case "$variant" 1 1 1 "$size"
    done
done

$CLUSTER_SH scale size=4
for variant in $VARIANTS; do
    for size in $SIZES; do
        run_case "$variant" 4 4 1 "$size"
        run_case "$variant" 4 8 2 "$size"
    done
done

$CLUSTER_SH scale size=8
for variant in $VARIANTS; do
    for size in $SIZES; do
        run_case "$variant" 8 8 1 "$size"
    done
done

echo "Benchmark finalizado. Resultado: $OUTPUT_CSV"
