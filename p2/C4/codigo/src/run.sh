#!/bin/bash

make mpi

in_file="../input/igen/in.txt"
in_file_distances="../input/input"
out_file="../output/output.txt"

# CUESTIÓN 2: parámetros de las ejecuciones paralelas
# - procesos: número de procesos MPI con los que se lanza cada instancia (se puede pasar
#   de los núcleos físicos para ver cómo se degrada el rendimiento)
# - NGM: cada cuántas generaciones hay migración
# - NEM: cuántos individuos migran de cada isla (no puede ser mayor que tam_pob/procesos)
# - repeticiones: ejecuciones de cada configuración para hacer la media de los tiempos
procesos="1 2 4 6 8 10 12 16"
NGM=10
NEM=3
repeticiones=3

while read line ; do

	if [[ $line != \#* ]]
	then
		n=$(echo $line | tr -s ' ' | cut -f1 -d ' ')
		m=$(echo $line | tr -s ' ' | cut -f2 -d ' ')
		gen=$(echo $line | tr -s ' ' | cut -f3 -d ' ')
		tam=$(echo $line | tr -s ' ' | cut -f4 -d ' ')
		mrate=$(echo $line | tr -s ' ' | cut -f5 -d ' ')

		for np in $procesos ; do
			for ((r = 1; r <= repeticiones; r++)) ; do
				echo -e
				echo -n "Executing with: "
				echo -e "N = "$n" M = "$m" N_GEN = "$gen" TAM_POB = "$tam" M_RATE = "$mrate" NP = "$np" NGM = "$NGM" NEM = "$NEM" (rep "$r")"
				make test_mpi N=$n M=$m N_GEN=$gen T_POB=$tam M_RATE=$mrate NP=$np NGM=$NGM NEM=$NEM < "${in_file_distances}_${n}_${m}_${gen}_${tam}_${mrate}.txt"
			done
		done
	fi
done < $in_file
