#!/bin/bash
export LC_ALL=C
ARGS="2000 800 400 200 0.3"
NGM=10; NEM=3
REPS=5
IN=../input/input_2000_800_400_200_0.3.txt
[ -f "$IN" ] || IN=../input_2000_800_400_200_0.3.txt
[ -f "$IN" ] || { echo "No encuentro el fichero de entrada"; exit 1; }

for np in 1 2 4 6 8; do
  suma=0
  for r in $(seq $REPS); do
    t=$(mpirun --oversubscribe -np $np ./mpi $ARGS $NGM $NEM < $IN | grep "Execution Time" | awk '{print $3}')
    suma=$(awk -v a=$suma -v b=$t 'BEGIN{print a+b}')
  done
  media=$(awk -v s=$suma -v r=$REPS 'BEGIN{print s/r}')
  [ $np -eq 1 ] && base=$media
  awk -v np=$np -v m=$media -v b=$base 'BEGIN{printf "np=%d  tiempo=%.3f s  speedup=%.2f  eficiencia=%.2f\n", np, m, b/m, b/m/np}' | tee -a ../output/prestaciones.txt
done
