#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>
#include <mpi.h>

#if defined (_OPENMP)
#include <omp.h>
#endif

#include "../include/io.h"

// CUESTIÓN 2: aplicar_mh recibe además NGM y NEM (parámetros del modelo de islas)
extern double aplicar_mh(const double *, int, int, int, int, double, int *, int, int);

static double mseconds() {
	struct timeval t ; //sexto arreglo, se quejaba porque tenia bytes basura
	gettimeofday(&t, NULL);
	return t.tv_sec*1000 + t.tv_usec/1000;
}

int main(int argc, char **argv)
{
	int rank, size;

	// P3: programa híbrido. Las llamadas a MPI solo las hace el hilo maestro de cada
	// proceso (fuera de las zonas paralelas o dentro de "omp master"), así que se pide el
	// nivel MPI_THREAD_FUNNELED y se comprueba que la implementación de MPI lo soporta
	int provided;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_FUNNELED, &provided);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	if (provided < MPI_THREAD_FUNNELED) {
		if (rank == 0)
			fprintf(stderr, "MPI no soporta MPI_THREAD_FUNNELED\n");
		MPI_Finalize();
		return(EXIT_FAILURE);
	}

//	Check Number of Input Args (ahora también NGM y NEM)
	if(argc < 9) {
		if(rank == 0) {
			fprintf(stderr,"Ayuda:\n");
			fprintf(stderr,"  mpirun -np P ./programa n m nGen tamPob m_rate NGM NEM NHILOS\n");		}
		MPI_Finalize();
		return(EXIT_FAILURE);
	}

//	Todos los procesos leen los parámetros de los argumentos (todos los reciben igual,
//	así que no hace falta enviarlos)
	int n = atoi(argv[1]);
	int m = atoi(argv[2]);
	int n_gen = atoi(argv[3]);
	int tam_pob = atoi(argv[4]);
	double m_rate = atof(argv[5]);
	int NGM = atoi(argv[6]);   // cada cuántas generaciones hay migración
	int NEM = atoi(argv[7]);   // cuántos individuos migran de cada isla
	int NHILOS = atoi(argv[8]); // P3: hilos OpenMP que pone en marcha cada proceso
	assert(NHILOS >= 1);
#if defined (_OPENMP)
	omp_set_num_threads(NHILOS);
#endif

//	Check que m es menor que n
	assert(m < n);

//  comprobamos que m_rate esté entre 0 y 1
	assert(m_rate >= 0.0 && m_rate <= 1.0);

//	comprobamos los parámetros de las islas ya que cada isla necesita al menos 4 individuos
//	para que se puedan cruzar (los 2 mejores generan 2 hijos) y NEM tiene que caber en
//	la isla más pequeña (tam_pob/size individuos)
	assert(NGM >= 1);
	assert(NEM >= 1);
	assert(tam_pob / size >= 4);
	assert(NEM <= tam_pob / size);

//	CUESTIÓN 2: la matriz de las distancias la lee solo el proceso 0 (la entrada estándar
//	solo le llega a él) y se la envía al resto con comunicación síncrona punto a punto.
//	Todos la necesitan para calcular el fitness de su isla. Solo se hace una vez.
	int tam_d = (n*n - n)/2;
	double *d;
	if(rank == 0) {
		d = read_distances(n);
	} else {
		d = (double *) malloc(tam_d * sizeof(double));
		assert(d);
	}
	// CUESTIÓN 5: el proceso 0 difunde la matriz a todos con una sola llamada colectiva
	MPI_Bcast(d, tam_d, MPI_DOUBLE, 0, MPI_COMM_WORLD);

	#ifdef DEBUG
//		print_distances(d, n);
	#endif

//	Allocate memory for output data
	int *sol = (int *) malloc(m * sizeof(int));

//	CUESTIÓN 2: barrera antes de tomar el tiempo inicial para que todos empiecen a la vez
//	(el reparto de la matriz no se cuenta)
	MPI_Barrier(MPI_COMM_WORLD);
	#ifdef TIME
		double ti = mseconds();
	#endif

//	cada proceso trabaja con su isla
	double value = aplicar_mh(d, n, m, n_gen, tam_pob, m_rate, sol, NGM, NEM);

//	CUESTIÓN 2: barrera antes del tiempo final para medir cuando ha terminado el más lento
	MPI_Barrier(MPI_COMM_WORLD);
	#ifdef TIME
		double tf = mseconds();
	#endif

//	Solo el proceso 0 tiene la solución global y es el que escribe los resultados
	if(rank == 0) {
		#ifdef TIME
			printf("Processes: %d  Threads: %d  NGM: %d  NEM: %d\n", size, NHILOS, NGM, NEM);
			printf("Execution Time: %.3lf sec\n", (tf - ti)/1000);
		#endif

		#ifdef DEBUG
			print_solution(n, m, sol, value);
		#endif
	}

	//	Free para la memoria reservada
	free(sol);
	free(d); //otro cambio

//	CUESTIÓN 2: cierramos el entorno MPI
	MPI_Finalize();
	return(EXIT_SUCCESS);
}
