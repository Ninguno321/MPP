CC = mpicc
# P3 CUESTIÓN 1: -fopenmp para poder usar OpenMP (threadprivate y, en la cuestión 2, el resto de directivas)
CFLAGS = -O3 -Wall -std=c99 -g -fopenmp

N =
M =
N_GEN =
T_POB =
M_RATE =
# CUESTIÓN 2: número de procesos MPI y parámetros del modelo de islas
NP =
NGM =
NEM =
# P3 CUESTIÓN 2: hilos OpenMP por proceso
NHILOS =

EXEC = mpi
OUTFILE = ../output/out
C_FILES = main.c io.c mh.c

# CUESTIÓN 2: se compila con mpicc (OpenMPI)
mpi: $(C_FILES)
	$(CC) $(CFLAGS) $(C_FILES) -o $(EXEC) -DTIME -DDEBUG

# CUESTIÓN 2: versión de trabajo fijo (sin criterio de convergencia) para medir prestaciones
mpi_fijo: $(C_FILES)
	$(CC) $(CFLAGS) $(C_FILES) -o mpi_fijo -DTIME -DDEBUG -DSIN_CONVERGENCIA

# CUESTIÓN 2: se ejecuta con mpirun. --oversubscribe permite lanzar más procesos que
# núcleos físicos (observación 4 del enunciado); con menos procesos no tiene efecto
# P3 CUESTIÓN 2: --bind-to none. Pporque por defecto OpenMPI ata cada proceso a un único núcleo y
# todos sus hilos OpenMP tendrían que compartirlo y  así el sistema reparte los hilos
test_mpi:
	mpirun --oversubscribe --bind-to none -np $(NP) ./$(EXEC) $(N) $(M) $(N_GEN) $(T_POB) $(M_RATE) $(NGM) $(NEM) $(NHILOS) >> $(OUTFILE)_$(N)_$(M)_$(N_GEN)_$(T_POB)_$(M_RATE)_np$(NP)_h$(NHILOS)_ngm$(NGM)_nem$(NEM).txt

clean:
	rm -f $(EXEC) mpi_fijo
