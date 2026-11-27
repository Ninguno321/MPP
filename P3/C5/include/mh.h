#ifndef _MH
#define _MH

		#define MAX_M 2000  // P3 CUESTIÓN 5, la instancia de datos/ejecucion/pm usa m = 2000
	typedef struct {
		int array_int[MAX_M]; // los genes van dentro del struct, ya no son un puntero
		double fitness;
	} Individuo;

	
	
	void cruzar(Individuo *, Individuo *, Individuo *, Individuo *, int, int);
	void mutar(Individuo *, int, int, double);
	void fitness(const double *, Individuo *, int, int);
#endif
