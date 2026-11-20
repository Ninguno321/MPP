CC = gcc
CFLAGS = -Wall -O3 -std=c99 -g

# P3 CUESTIÓN 3: parámetros de los experimentos. Se pueden cambiar al llamar a make,
# por ejemplo:  make run_omp NE=3    o    make run_omp HILOS="3 5 7"
N = 30000
M = 16000
N_GEN = 0
NE = 6
HILOS = 1 2 4 6 8 10 12 14 16 18 20
SERIES = 5
OUT = ../output/c3_n$(N)_m$(M)_ne$(NE).txt

omp: main.c mh.c io.c
	$(CC) $(CFLAGS) main.c mh.c io.c -lm -fopenmp -o omp -DTIMEGenPobIni

# cada línea del fichero de salida: <hilos> <tiempo de FOR_INI>
run_omp:
	for s in $$(seq $(SERIES)); do \
		for h in $(HILOS); do \
			printf "%s " $$h >> $(OUT); \
			./omp $(N) $(M) $(N_GEN) $(NE) $$h 1 >> $(OUT); \
		done; \
	done

# una fila por número de hilos: h, el tiempo de cada serie y la media
resumen:
	LC_ALL=C awk '{ s[$$1] += $$2; c[$$1]++; v[$$1] = v[$$1] "  " $$2 } \
		END { for (h in s) printf "%s %s   media= %.6f\n", h, v[h], s[h]/c[h] }' $(OUT) | sort -n

clean:
	rm -f omp
