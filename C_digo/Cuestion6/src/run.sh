#!/bin/bash
# CUESTIÓN 6 - Ejecuciones de las versiones de ordenación (qsort / mergeSort)
#
# Uso: ./run.sh [n m n_gen tam_pob m_rate]
#   Por defecto: 2000 200 400 2000 0.15
#   El fichero de entrada solo contiene la matriz de distancias de tamaño n
#   (io.c solo usa n), así que se usa el primer ../input/input_<n>_*.txt que
#   haya, sea cual sea el tam_pob con el que se generó.
#
# Salidas (en ../output/c6/):
#   versiones.txt : tiempo de ordenación de V0..V5 con 1,2,4,6 hilos, REPS repeticiones
#   umbrales.txt  : V4 y V5 con distintos UMBRAL, 6 hilos
# Cada línea: version hilos [umbral] t_ordenacion_total_ms t_media_por_ordenacion_ms t_total_s

N=${1:-2000}; M=${2:-200}; GEN=${3:-400}; TAM=${4:-2000}; MR=${5:-0.15}
IN=$(ls ../input/input_${N}_*.txt 2>/dev/null | head -1)
OUTDIR=../output/c6
HILOS="1 2 4 6"
REPS=5
UMBRALES="16 32 64 128 256 512"

if [ -z "$IN" ]; then echo "No hay fichero ../input/input_${N}_*.txt"; exit 1; fi
echo "Entrada: $IN"
mkdir -p $OUTDIR

# --- 1) Las 6 versiones -------------------------------------------------
make versiones
OUT=$OUTDIR/versiones_${N}_${M}_${GEN}_${TAM}_${MR}.txt
echo "# version hilos t_ordenacion_total_ms t_media_por_ordenacion_ms t_total_s  (n m n_gen tam_pob m_rate = $N $M $GEN $TAM $MR)" > $OUT
for v in 0 1 2 3 4 5; do
  for h in $HILOS; do
    for r in $(seq $REPS); do
      salida=$(OMP_NUM_THREADS=$h ./sec_v$v $N $M $GEN $TAM $MR < $IN)
      t_ord=$(echo "$salida" | grep "Tiempo ORDENACION" | awk '{print $5, $10}')
      t_tot=$(echo "$salida" | grep "Execution Time" | awk '{print $3}')
      echo "$v $h $t_ord $t_tot" | tee -a $OUT
    done
  done
done

# --- 2) Barrido de umbral para las versiones con corte (V4 y V5) --------
OUT=$OUTDIR/umbrales_${N}_${M}_${GEN}_${TAM}_${MR}.txt
echo "# version hilos umbral t_ordenacion_total_ms t_media_por_ordenacion_ms t_total_s  (n m n_gen tam_pob m_rate = $N $M $GEN $TAM $MR)" > $OUT
for u in $UMBRALES; do
  for v in 4 5; do
    gcc -O3 -Wall -std=c99 -fopenmp main.c io.c mh.c mergesort.c -o sec_u -DTIME -DVERSION=$v -DUMBRAL=$u
    for r in $(seq $REPS); do
      salida=$(OMP_NUM_THREADS=6 ./sec_u $N $M $GEN $TAM $MR < $IN)
      t_ord=$(echo "$salida" | grep "Tiempo ORDENACION" | awk '{print $5, $10}')
      t_tot=$(echo "$salida" | grep "Execution Time" | awk '{print $3}')
      echo "$v 6 $u $t_ord $t_tot" | tee -a $OUT
    done
  done
done
rm -f sec_u
