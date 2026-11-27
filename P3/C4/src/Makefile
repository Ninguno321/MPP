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
# P3 CUESTIÓN 4: dos niveles de paralelismo. Cada línea del fichero de salida:
# <hilos FOR_INI (h1)> <hilos FOR_FIT (h2)> <tiempo de FOR_INI>
HILOS1 = 1 2 3 6
HILOS2 = 1 2 4 6 8 12
SERIES2 = 3
OUT2 = ../output/c4_n$(N)_m$(M)_ne$(NE).txt

run_omp2:
	for s in $$(seq $(SERIES2)); do \
		for h1 in $(HILOS1); do \
			for h2 in $(HILOS2); do \
				printf "%s %s " $$h1 $$h2 >> $(OUT2); \
				./omp $(N) $(M) $(N_GEN) $(NE) $$h1 $$h2 >> $(OUT2); \
			done; \
		done; \
	done

# una fila por combinación (h1, h2): los tiempos de cada serie y la media
resumen2:
	LC_ALL=C awk '{ k = $$1 " " $$2; s[k] += $$3; c[k]++; v[k] = v[k] "  " $$3 } \
		END { for (k in s) printf "%s %s   media= %.6f\n", k, v[k], s[k]/c[k] }' $(OUT2) | sort -n -k1,1 -k2,2
		
clean:
	rm -f omp
