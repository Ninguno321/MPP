#!/bin/bash
# =====================================================================================
#  P3 CUESTIÓN 2 - experimentos_c2.sh
#  Compara, en la misma máquina, con la misma instancia y alternando las versiones:
#    - sec:      versión secuencial (código de la P1 compilado sin OpenMP)
#    - p1_omp:   mejor versión de la P1 (OpenMP), variando los hilos
#    - p2_mpi:   mejor versión de la P2 (MPI, comunicaciones colectivas), variando los procesos
#    - hibrido:  versión híbrida MPI+OpenMP de la P3, variando procesos e hilos
#  Cada configuración se ejecuta en dos modos:
#    - fijo:          siempre GEN_FIJO generaciones (para comparar tiempos con el mismo trabajo)
#    - convergencia:  versión normal (para comparar tiempo y fitness con I = fitness/tiempo)
#
#  Uso (desde la carpeta C2):  chmod +x experimentos_c2.sh && ./experimentos_c2.sh
#  Para recalcular solo el resumen a partir de resultados_c2/resultados.csv, sin volver a
#  ejecutar nada:                ./experimentos_c2.sh resumen
#  Duración aproximada: 10-15 minutos. Portátil enchufado y sin usarlo mientras tanto.
#  Resultado: resultados_c2/resumen.txt (y resultados_c2/resultados.csv con cada ejecución)
# =====================================================================================

# los números decimales con punto (en un sistema en español awk usaría la coma y leería mal
# los tiempos)
export LC_ALL=C

INSTANCIA="2000 800 400 200 0.3"     # n m n_gen tam_pob m_rate
GEN_FIJO=60                           # generaciones de la versión de trabajo fijo
NGM=10
NEM=3
REPS=3
IN="input/input_2000_800_400_200_0.3.txt"

HILOS_P1="2 4 6 12"
PROCS_P2="2 4 6 12"
# configuraciones procesos x hilos del híbrido (EP = procesos * hilos = 1, 2, 4, 6, 12)
CONF_HIB="1x1 1x2 2x1 1x4 2x2 4x1 1x6 2x3 3x2 6x1 1x12 2x6 3x4 4x3 6x2 12x1"

[ -f "$IN" ] || { echo "ERROR: no existe $IN (ejecuta el script desde la carpeta C2)"; exit 1; }

# compilación de todas las versiones
[ "$1" = "resumen" ] || ( cd src && make mpi > /dev/null && make mpi_fijo > /dev/null ) || { echo "ERROR compilando el híbrido"; exit 1; }
[ "$1" = "resumen" ] || ( cd comparacion/p1/src && make sec sec_fijo omp omp_fijo > /dev/null ) || { echo "ERROR compilando la P1"; exit 1; }
[ "$1" = "resumen" ] || ( cd comparacion/p2/src && make mpi mpi_fijo > /dev/null ) || { echo "ERROR compilando la P2"; exit 1; }

OUT="resultados_c2"
mkdir -p "$OUT"
CSV="$OUT/resultados.csv"
SOLO_RESUMEN=0
[ "$1" = "resumen" ] && SOLO_RESUMEN=1
[ $SOLO_RESUMEN -eq 0 ] && echo "version,modo,p,h,ep,rep,tiempo,fitness" > "$CSV"

# parámetros de la instancia según el modo
args() { if [ "$1" = "fijo" ]; then echo "$INSTANCIA" | awk -v g=$GEN_FIJO '{$3=g; print}'; else echo "$INSTANCIA"; fi; }
suf() { [ "$1" = "fijo" ] && echo "_fijo"; }

guardar() {   # guardar <version> <modo> <p> <h> <rep> <salida>
	local t f
	t=$(echo "$6" | grep -m1 "Execution Time" | awk '{print $3}')
	f=$(echo "$6" | grep -m1 "Distance" | awk '{print $2}')
	[ -z "$t" ] && { t="NA"; f="NA"; echo "   FALLO: $1 $2 p=$3 h=$4"; }
	echo "$1,$2,$3,$4,$(( $3 * $4 )),$5,$t,$f" >> "$CSV"
	printf "   rep=%s %-8s %-12s p=%-2s h=%-2s t=%-8s fitness=%s\n" "$5" "$1" "$2" "$3" "$4" "$t" "$f"
}

for rep in $(seq $REPS); do
	[ $SOLO_RESUMEN -eq 1 ] && break
	for modo in fijo convergencia; do
		A=$(args $modo); S=$(suf $modo)

		# secuencial
		guardar sec $modo 1 1 $rep "$(comparacion/p1/src/sec$S $A < $IN)"

		# P1: OpenMP (los hilos se fijan con OMP_NUM_THREADS)
		for h in $HILOS_P1; do
			guardar p1_omp $modo 1 $h $rep "$(OMP_NUM_THREADS=$h comparacion/p1/src/omp$S $A < $IN)"
		done

		# P2: MPI con comunicaciones colectivas
		for p in $PROCS_P2; do
			guardar p2_mpi $modo $p 1 $rep "$(mpirun --oversubscribe -np $p comparacion/p2/src/mpi$S $A $NGM $NEM < $IN 2>&1)"
		done

		# P3: híbrido MPI + OpenMP (--bind-to none para que los hilos usen todos los núcleos)
		for c in $CONF_HIB; do
			p=${c%x*}; h=${c#*x}
			guardar hibrido $modo $p $h $rep "$(mpirun --oversubscribe --bind-to none -np $p src/mpi$S $A $NGM $NEM $h < $IN 2>&1)"
		done
	done
done

# resumen: tiempo y fitness medios, speed-up y eficiencia respecto al secuencial del
# mismo modo (EP = procesos * hilos) e indicador I = fitness / tiempo
awk -F, '
NR == 1 || $7 == "NA" { next }
{ k = $2 "|" $1 "|" $3 "|" $4 "|" $5; st[k] += $7; sf[k] += $8; n[k]++ }
END {
	for (k in st) { split(k, a, "|"); if (a[2] == "sec") ts[a[1]] = st[k] / n[k] }
	for (k in st) {
		split(k, a, "|"); tm = st[k] / n[k]; fm = sf[k] / n[k]; s = ts[a[1]] / tm
		printf "%-12s %-8s p=%-2s h=%-2s EP=%-2s  t=%7.3f s  speedup=%5.2f  eficiencia=%4.2f  fitness=%10.0f  I=%9.0f\n", a[1], a[2], a[3], a[4], a[5], tm, s, s / a[5], fm, fm / tm
	}
}' "$CSV" | sort -k1,1 -k2,2 -k5,5V -k3,3V > "$OUT/resumen.txt"

echo
echo "================= RESUMEN ($OUT/resumen.txt) ================="
cat "$OUT/resumen.txt"
