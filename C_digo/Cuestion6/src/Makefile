CC = gcc
CFLAGS = -O3 -Wall -std=c99 -g -fopenmp

N =
M =
N_GEN =
T_POB =
M_RATE =

# CUESTIÓN 6: versión de la ordenación y umbral de tareas
#   VERSION 0 = qsort original
#   VERSION 1 = mergeSort secuencial (mezclar original)
#   VERSION 2 = mergeSort con tareas (mezclar original)
#   VERSION 3 = tareas + mezclar optimizado            -> apartado (a)
#   VERSION 4 = tareas + if(tamaño > UMBRAL)            -> apartado (b)
#   VERSION 5 = (a) + (b), versión final
# Ejemplo: make sec VERSION=4 UMBRAL=128
VERSION = 5
UMBRAL = 64

EXEC = sec
OUTFILE = ../output/out
C_FILES = main.c io.c mh.c mergesort.c

sec: $(C_FILES) ../include/mergesort.h
	$(CC) $(CFLAGS) $(C_FILES) -o $(EXEC) -DTIME -DDEBUG -DVERSION=$(VERSION) -DUMBRAL=$(UMBRAL)

# Compila las 6 versiones de golpe: sec_v0 ... sec_v5
versiones: $(C_FILES) ../include/mergesort.h
	for v in 0 1 2 3 4 5; do \
		$(CC) $(CFLAGS) $(C_FILES) -o sec_v$$v -DTIME -DVERSION=$$v -DUMBRAL=$(UMBRAL); \
	done

#valgrind --tool=memcheck --track-origins=yes --leak-check=yes --log-file=valgrind.txt podemos poder el valgrind aqui para reutilizar el comando. Justo delante del exec
test_sec:
	 ./$(EXEC) $(N) $(M) $(N_GEN) $(T_POB) $(M_RATE)>> $(OUTFILE)_$(N)_$(M)_$(N_GEN)_$(T_POB)_$(M_RATE).txt

clean:
	rm -f $(EXEC) sec_v* sec_u*
