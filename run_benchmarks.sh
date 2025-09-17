#!/bin/bash

# ================================
# Benchmark OpenMP / std::thread
# Mide tiempo y eventos de hardware
# ================================

# --- Parámetros ---
N=100000000
R=65536
REPS=3
THREADS_LIST="1 2 4"
VARIANTS=("private" "mutex" "atomic")
SEED=12345

# --- Carpeta de salida ---
OUT_DIR="outputs"
mkdir -p $OUT_DIR

CSV_FILE="$OUT_DIR/results.csv"
echo "method,variant,threads,rep,t_gen,t_count,t_merge,total,real,user,sys,cpu%,cycles,instructions,cache_ref,cache_miss" > $CSV_FILE

# --- Compilación ---
echo "Compiling..."
g++ -O3 -march=native -fopenmp -std=c++17 histo_openmp.cpp -o histo_openmp
g++ -O3 -march=native -pthread -std=c++17 histo_threads.cpp -o histo_threads

# --- Función auxiliar ---
run_with_perf() {
    local cmd="$1"

    # Archivos temporales en carpeta de salida
    TIME_FILE="$OUT_DIR/temp_time.txt"
    OUT_FILE="$OUT_DIR/temp_out.txt"
    PERF_FILE="$OUT_DIR/perf_out.txt"

    # Medir tiempos con /usr/bin/time
    /usr/bin/time -f "%e %U %S %P" -o $TIME_FILE $cmd > $OUT_FILE 2>&1

    # Extraer tiempos internos del programa
    TG=$(grep -oP 't_gen=\K[0-9eE\.\-]+' $OUT_FILE)
    TC=$(grep -oP 't_count=\K[0-9eE\.\-]+' $OUT_FILE)
    TM=$(grep -oP 't_merge=\K[0-9eE\.\-]+' $OUT_FILE)
    TT=$(grep -oP 'total=\K[0-9eE\.\-]+' $OUT_FILE)

    # Extraer tiempos de CPU del comando /usr/bin/time
    read REAL USER SYS PCT < $TIME_FILE

    # Medir eventos de hardware con perf (sin encabezados)
    perf stat -x, -e cycles,instructions,cache-references,cache-misses $cmd 2> $PERF_FILE >/dev/null
    CYCLES=$(awk -F, 'NR==1 {print $1}' $PERF_FILE)
    INST=$(awk -F, 'NR==2 {print $1}' $PERF_FILE)
    CACHE_REF=$(awk -F, 'NR==3 {print $1}' $PERF_FILE)
    CACHE_MISS=$(awk -F, 'NR==4 {print $1}' $PERF_FILE)

    echo "$TG,$TC,$TM,$TT,$REAL,$USER,$SYS,$PCT,$CYCLES,$INST,$CACHE_REF,$CACHE_MISS"
}

# --- Ejecutar OpenMP ---
for VAR_IDX in 0 1 2; do
    VAR_NAME=${VARIANTS[$VAR_IDX]}
    for T in $THREADS_LIST; do
        export OMP_NUM_THREADS=$T
        for REP in $(seq 1 $REPS); do
            METRICS=$(run_with_perf "./histo_openmp $N $R $VAR_IDX")
            echo "openmp,$VAR_NAME,$T,$REP,$METRICS" >> $CSV_FILE
        done
    done
done

# --- Ejecutar std::thread ---
for VAR_IDX in 0 1 2; do
    VAR_NAME=${VARIANTS[$VAR_IDX]}
    for T in $THREADS_LIST; do
        for REP in $(seq 1 $REPS); do
            METRICS=$(run_with_perf "./histo_threads $N $R $T $SEED $VAR_IDX")
            echo "stdthread,$VAR_NAME,$T,$REP,$METRICS" >> $CSV_FILE
        done
    done
done

echo "Benchmark finished. Results in $CSV_FILE"
