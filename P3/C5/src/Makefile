CC = mpicc
CFLAGS = -O3 -Wall -std=c99 -g

# P3 CUESTIÓN 5: instancia del Makefile de datos/ejecucion/pm (NGM = 1 por defecto en main.c)
N = 5000
M = 2000
N_GEN = 1
T_POB = 500
PROCS = 1 2 4 6 8 10 12 14 16 18 20 24 30
SERIES = 3
OUT = ../output/c5_n$(N)_m$(M)_ngen$(N_GEN)_pob$(T_POB).txt

mpi: main.c mh.c io.c
	$(CC) $(CFLAGS) main.c mh.c io.c -lm -o mpi -DTIME -DTIME_ORD

# cada línea del fichero de salida: <procesos> <tiempo total> <tiempo de ordenación>
run_mpi:
	for s in $$(seq $(SERIES)); do \
		for p in $(PROCS); do \
			printf "%s " $$p >> $(OUT); \
			mpirun --oversubscribe -np $$p ./mpi $(N) $(M) $(N_GEN) $(T_POB) | \
				awk '/Execution Time/ {t = $$3} /Sort Time/ {o = $$3} END {print t, o}' >> $(OUT); \
		done; \
	done

# una fila por número de procesos: los tiempos de cada serie, la media y el t_ord medio
resumen:
	LC_ALL=C awk '{ s[$$1] += $$2; o[$$1] += $$3; c[$$1]++; v[$$1] = v[$$1] "  " $$2 } \
		END { for (p in s) printf "%s %s   media= %.4f   t_ord= %.6f\n", p, v[p], s[p]/c[p], o[p]/c[p] }' $(OUT) | sort -n

clean:
	rm -f mpi