CC = mpicc
CFLAGS = -O3 -Wall -std=c99 -g

N =
M =
N_GEN =
T_POB =
M_RATE =
# CUESTIÓN 2: número de procesos MPI y parámetros del modelo de islas
NP =
NGM =
NEM =

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
test_mpi:
	mpirun --oversubscribe -np $(NP) ./$(EXEC) $(N) $(M) $(N_GEN) $(T_POB) $(M_RATE) $(NGM) $(NEM) >> $(OUTFILE)_$(N)_$(M)_$(N_GEN)_$(T_POB)_$(M_RATE)_np$(NP)_ngm$(NGM)_nem$(NEM).txt

clean:
	rm -f $(EXEC) mpi_fijo
