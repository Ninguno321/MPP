#ifndef _MH
#define _MH
	#define MAX_M 1000  // tamaño máximo de un individuo (tiene que ser mayor o igual que el mayor m que usemos)
	typedef struct {
		int array_int[MAX_M]; // los genes van dentro del struct, ya no son un puntero
		double fitness;
	} Individuo;

	
	
	void cruzar(Individuo *, Individuo *, Individuo *, Individuo *, int, int);
	void mutar(Individuo *, int, int, double);
	void fitness(const double *, Individuo *, int, int);
#endif
