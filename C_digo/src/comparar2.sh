#!/bin/bash
IN=../input/input_2000_200_400_4000_0.15.txt
OUT=../output/comparativa/tiempos2.txt
ARGS="2000 20 1 200000 0.15"     # n m n_gen tam_pob m_rate
mkdir -p ../output/comparativa
rm -f $OUT

for h in 1 2 4 6; do
  for rep in 1 2 3 4 5 6; do
    for v in a b; do
      OMP_NUM_THREADS=$h ./sec_$v $ARGS < $IN | grep FOR_INI | sed "s/^/$v /" >> $OUT
    done
  done
done
