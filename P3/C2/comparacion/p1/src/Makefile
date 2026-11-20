# P3 CUESTIÓN 2: mejor versión de la P1 (OpenMP) y versión secuencial, para comparar
CC = gcc
# _DEFAULT_SOURCE: con -std=c99 la cabecera stdlib.h no declara rand_r (es POSIX)
CFLAGS = -O3 -Wall -std=c99 -g -D_DEFAULT_SOURCE
C_FILES = main.c io.c mh.c

# versión OpenMP de la P1 (los hilos se fijan con OMP_NUM_THREADS)
omp: $(C_FILES)
	$(CC) $(CFLAGS) -fopenmp $(C_FILES) -o omp -DTIME -DDEBUG
omp_fijo: $(C_FILES)
	$(CC) $(CFLAGS) -fopenmp $(C_FILES) -o omp_fijo -DTIME -DDEBUG -DSIN_CONVERGENCIA

# versión secuencial: el mismo código sin -fopenmp (las directivas se ignoran)
sec: $(C_FILES)
	$(CC) $(CFLAGS) -Wno-unknown-pragmas $(C_FILES) -o sec -DTIME -DDEBUG
sec_fijo: $(C_FILES)
	$(CC) $(CFLAGS) -Wno-unknown-pragmas $(C_FILES) -o sec_fijo -DTIME -DDEBUG -DSIN_CONVERGENCIA

clean:
	rm -f omp omp_fijo sec sec_fijo