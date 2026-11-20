#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/time.h>

#include "../include/io.h"

// P3 CUESTIÓN 3: dos parámetros nuevos, los hilos de FOR_INI y de FOR_FIT
extern double aplicar_mh(const double *, int, int, int, int, double, int *, int, int);

#ifdef TIME   // P3 CUESTIÓN 3: solo se usa si se mide el tiempo total
static double mseconds() {
	struct timeval t ; //sexto arreglo, se quejaba porque tenia bytes basura 
	gettimeofday(&t, NULL);
	return t.tv_sec*1000 + t.tv_usec/1000;
}
#endif

int main(int argc, char **argv)
{
//	Check Number of Input Args
		// P3 CUESTIÓN 3: nuevos parámetros N_HILOS_INI y N_HILOS_FIT
	if(argc < 7) {
		fprintf(stderr,"Ayuda:\n"); 
		fprintf(stderr,"  ./programa n m nGen tamPob N_HILOS_INI N_HILOS_FIT [m_rate]\n");
		return(EXIT_FAILURE);
	}
	
	int n = atoi(argv[1]);
	int m = atoi(argv[2]);
	int n_gen = atoi(argv[3]);
	int tam_pob = atoi(argv[4]);
	int n_hilos_ini = atoi(argv[5]);   // hilos del bucle de la población inicial (FOR_INI)
	int n_hilos_fit = atoi(argv[6]);   // hilos del bucle del fitness (FOR_FIT)
	// m_rate pasa a ser opcional (por defecto 0.3) porque el Makefile del profesor no lo pasa
	double m_rate = (argc > 7) ? atof(argv[7]) : 0.3;
	assert(n_hilos_ini >= 1 && n_hilos_fit >= 1);
	
//	Check that 'm' is less than 'n'
	assert(m < n);

//  comprobamos que m_rate esté entre 0 y 1
	assert(m_rate >= 0.0 && m_rate <= 1.0);	
	
//	Generate matrix D with distance values among elements
	double *d = read_distances(n);
	
	#ifdef DEBUG
//		print_distances(d, n);
	#endif
	
//	Allocate memory for output data
	int *sol = (int *) malloc(m * sizeof(int));
	
	#ifdef TIME
		double ti = mseconds();
	#endif
	
//	Call Metaheuristic
	double value = aplicar_mh(d, n, m, n_gen, tam_pob, m_rate, sol, n_hilos_ini, n_hilos_fit);
		(void) value;   // P3 CUESTIÓN 3: sin el -DDEBUG no se imprime la solución	
	#ifdef TIME
		double tf = mseconds();
		printf("Execution Time: %.3lf sec\n", (tf - ti)/1000);
	#endif
	

	
	#ifdef DEBUG
		print_solution(n, m, sol, value);
	#endif
	//	Free Allocated Memory	
	free(sol);
	free(d); //otro cambio 
	
	return(EXIT_SUCCESS);
}
