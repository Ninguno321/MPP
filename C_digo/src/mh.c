#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "../include/mh.h"

#define PRINT 0
//#define PCT_CONVER 0.02   // valor porcentual de mejora esperado (2%)
//#define NGENERACIONES_ATRAS 5      // se compara con el mejor fitness de hace X generaciones y esto es porque a lo mejor puede haber una generacion sin mejora pero luego otra que si mejora



#define PCT_CONVER 0.02
#define NGENERACIONES_ATRAS 5

//#define PCT_CONVER 0.02
//#define NGENERACIONES_ATRAS 10

//#define PCT_CONVER 0.02
//#define NGENERACIONES_ATRAS 15



//#define PCT_CONVER 0.05
//#define NGENERACIONES_ATRAS 5

//#define PCT_CONVER 0.05
//#define NGENERACIONES_ATRAS 10

//#define PCT_CONVER 0.05
//#define NGENERACIONES_ATRAS 15





//#define PCT_CONVER 0.10
//#define NGENERACIONES_ATRAS 5

//#define PCT_CONVER 0.10
//#define NGENERACIONES_ATRAS 10

//#define PCT_CONVER 0.10
//#define NGENERACIONES_ATRAS 15




//#define PCT_CONVER 0.02
//#define NGENERACIONES_ATRAS 5


int aleatorio(int n) {
	return (rand() % n);  // genera un numero aleatorio entre 0 y n-1
}

int find_element(int *array, int end, int element)
{
	int i=0;
	int found=0;
	
	// comprueba que un elemento no está incluido en el individuo (el cual no admite enteros repetidos)
	while((i < end) && ! found) {
		if(array[i] == element) {
			found = 1;
		}
		i++;
	}
	return found;
}

int *crear_individuo(int n, int m)
{
	int i=0, value;
	int *individuo = (int *) malloc(m * sizeof(int));
	
	// inicializa array de elementos
	memset(individuo, -1, m * sizeof(int));
	
	while(i < m) {
		value = aleatorio(n);
		// si el nuevo elemento no está en el array...
		if(!find_element(individuo, i, value)) {
			individuo[i] = value;  // lo incluimos
			i++;
		}
	}
	return individuo;
} 

int comp_array_int(const void *a, const void *b) {
	return (*(int *)a - *(int *)b);
}

int comp_fitness(const void *a, const void *b) {
	/* qsort pasa un puntero al elemento que está ordenando */
	return (*(Individuo **)b)->fitness - (*(Individuo **)a)->fitness;
}

double aplicar_mh(const double *d, int n, int m, int n_gen, int tam_pob, double m_rate, int *sol)
{
        // para reiniciar la secuencia pseudoaleatoria en cada ejecución
        srand(time(NULL) + getpid());
	int i, g, mutation_start;
	
	// crea poblacion inicial (array de individuos)
	Individuo **poblacion = (Individuo **) malloc(tam_pob * sizeof(Individuo *));
	assert(poblacion);
	
	// crea cada individuo (array de enteros aleatorios)
	for(i = 0; i < tam_pob; i++) {
		poblacion[i] = (Individuo *) malloc(sizeof(Individuo));
		poblacion[i]->array_int = crear_individuo(n, m);
		
		// calcula el fitness del individuo
		fitness(d, poblacion[i], n, m);
	}
	
	// ordena individuos segun la funcion de bondad (mayor "fitness" --> mas aptos)
	qsort(poblacion, tam_pob, sizeof(Individuo *), comp_fitness);

	//segunda opcion
	// historial del mejor fitness: hist[k] = mejor fitness tras k generaciones
	//el array hist guarda el mejor fitness tras cada generación 
	//(hist[k] = mejor fitness tras k generaciones, con hist[0] el de la población inicial).
	//double *hist = (double *) malloc((n_gen + 1) * sizeof(double));
	//assert(hist);
	//hist[0] = poblacion[0]->fitness;
	
	// evoluciona la poblacion durante un numero de generaciones
	//Para tener un valor inicial y saber cuando mejora una generacion con respecto a las otras
	double fitness_modelo = poblacion[0]->fitness;
	//Contador para saber si tenemos que seguir avanzando o no en generaciones
	int ngeneracion = 0;

	//Mantenemos n_gen como limite para evitar un posible bucle infinito
	for(g = 0; g < n_gen ; g++)
	{//&& ngeneracion < NGENERACIONES_ATRAS
		// los hijos de los ascendientes mas aptos sustituyen a la ultima mitad de los individuos menos aptos
		for(i = 0; i < (tam_pob/2) - 1; i += 2) {
			cruzar(poblacion[i], poblacion[i+1], poblacion[tam_pob/2 + i], poblacion[tam_pob/2 + i + 1], n, m);
		}
		
		// inicia la mutacion a partir de 1/4 de la poblacion
		mutation_start = tam_pob/4;
		
		// muta 3/4 partes de la poblacion
		for(i = mutation_start; i < tam_pob; i++) {
			mutar(poblacion[i], n, m, m_rate);
		}
		
		// recalcula el fitness del individuo
		for(i = 0; i < tam_pob; i++) {
			fitness(d, poblacion[i], n, m);
		}
		
		// ordena individuos segun la funcion de bondad (mayor "fitness" --> mas aptos)
		qsort(poblacion, tam_pob, sizeof(Individuo *), comp_fitness);
		

		//Valor de fitness que buscamos, esperamos esta mejora
		double fitness_esperado = fitness_modelo * (1 + PCT_CONVER);
		//Si la poblacion actual supera el fitness esperado entonces reiniciamos 
		//el contador para poder seguir avanzando y mirar las siguientes 
		// X generaciones (si X =5 y estamos en la posicion 5 quiere 
		//decir que en las 5 anteriores no se ha conseguido superar 
		//la mejora esperada y por ende converge)
		if(poblacion[0]->fitness > fitness_esperado){
			fitness_modelo = poblacion[0]->fitness;
			ngeneracion=0;
		}else{
			ngeneracion++;
		}		


		if (PRINT) {
			printf("Generacion %d - ", g);
			printf("Fitness = %.0lf\n", poblacion[0]->fitness);
		}

		//segunda opcion
		//esto es porque Justo antes, el qsort ordena la población de mayor a menor fitness y
		// por eso poblacion[0] es el mejor individuo de esa generación
		//hist[g + 1] = poblacion[0]->fitness;

		// criterio de convergencia: si en las últimas NGENERACIONES_ATRAS generaciones
		// el fitness no ha mejorado al menos un PCT_CONVER , paramos
		//if (g + 1 >= NGENERACIONES_ATRAS) {
   		//	 double antes = hist[g + 1 - NGENERACIONES_ATRAS];
   		//	 double mejora = (hist[g + 1] - antes) / antes;
    	//	 if (mejora < PCT_CONVER)
        //		 break;
		//}


	}
	
	// ordena el array solucion
	qsort(poblacion[0]->array_int, m, sizeof(int), comp_array_int);

        // y lo mueve a sol para escribirlo
	memmove(sol, poblacion[0]->array_int, m*sizeof(int));
	
	// almacena el mejor valor obtenido para el fitness
	double value = poblacion[0]->fitness;
	
	// se libera la memoria reservada
	for (int  i=0; i<tam_pob ; i++){ 
		free(poblacion[i]->array_int);// segundo arreglo leak memoria 
		free(poblacion[i]); // primer arreglo leak memoria. El tercero esta en io.c free d
	}
	free(poblacion);
	//hist se libera al final junto con el resto de estructuras.
	//free(hist);
	// devuelve el valor obtenido para el fitness
	return value;
}

void cruzar(Individuo *padre1, Individuo *padre2, Individuo *hijo1, Individuo *hijo2, int n, int m)
{	
	/*Cruzar: para cada par de individuos selecciona un punto aleatorio a partir del que se produce
	la mezcla y los nuevos descendientes se crean intercambiando los genes de los padres entre
	sí.*/
	
	//Primer paso, generar el punto de corte. Cruce en un solo punto.
	//El punto de corte no se incluye como parte de los genes izquierdos.
	//Esto soluciona el problema de que salga corte = 0 y se copie 
	//directamente el padre2 entero.
	int corte = 1 + aleatorio(m-1); 

	//Copiamos al hijo1 la primera parte de los genes del padre1
	//al hijo2 lo mismo pero con el padre2
	for (int i = 0 ; i < corte ; i ++){
		hijo1->array_int[i] = padre1->array_int[i];
		hijo2->array_int[i] = padre2->array_int[i];
	}
	//Copiamos la parte restante del padre contrario
	//Es decir, al hijo1 le copiamos la segunda parte del padre2
	for (int i = corte ; i < m ; i ++){
		hijo1->array_int[i] = padre2->array_int[i];
		hijo2->array_int[i] = padre1->array_int[i];
	}
	
	//Doble bucle para revisar que no existan genes duplicados 
	//Partimos de que los padres NO tienen repetidos asi podemos 
	//ahorrarnos parte del recorrido y solo comparar los genes de 
	// la parte del padre1 con las del otro.
	for (int i = 0 ; i < corte ; i ++){
		for(int j = corte ; j < m ; j++){
			//Verificamos duplicado
			if(hijo1->array_int[i] == hijo1->array_int[j]){
				int nuevo;
				do{
					nuevo = aleatorio(n);
				}while (find_element(hijo1->array_int, m, nuevo));
				//Seguimos buscando un numero hasta que no sea uno que ya 
				//este presente. Luego actualizamos
				hijo1->array_int[j] = nuevo;
			}
			
			if(hijo2->array_int[i] == hijo2->array_int[j]){
				int nuevo;
				do{
					nuevo = aleatorio(n);
				}while (find_element(hijo2->array_int, m, nuevo));
				hijo2->array_int[j] = nuevo;
			}
		}
	}
}

void mutar(Individuo *actual, int n, int m, double m_rate)
{
	/*
		Mutar: produce mutaciones en los descendientes en función de una cierta probabilidad
(normalmente baja). Esto permite, por un lado, que la población sea diversa y, por otro, evita
que el algoritmo converja de forma prematura.
	*/
	//Mutamos X genes con probabilidad m_rate
	//Siempre mutan porque solo mutan los ya seleccionados antes.

	int nmutar = (int)(m_rate * m);
	//minimo muta una vez, si m_rate no es 0.0
	if(nmutar < 1 && m_rate > 0.0){
		nmutar=1;
	}
	
	//Para mutar los X genes
	for(int i = 0; i<nmutar; i++){
		//posicion aleatoria a mutar
		int pos = aleatorio(m);
		int nuevo = 0;
		//nos ahorramos un if si siempre se generan entradas validas
		//Buscamos otro mientras que sea repetido
		do{
			nuevo = aleatorio(n);
		}while (find_element(actual->array_int, m, nuevo));
		actual->array_int[pos] = nuevo;
	}
}

double distancia_ij(const double *d, int i, int j, int n)
{
    // Devuelve la distancia entre dos elementos i, j de la matriz 'd'
	// Formula de K
	//Añadimos este if para comprobar porque la fórmula solo es válida con i < j, 
	//el if garantiza que siempre se cumpla i < j, que es la condición de validez 
	//de la fórmula, y que es correcto porque la distancia es simétrica
	if (i > j) { int t = i; i = j; j = t; }
	int val = (((n * n)- n)/2)-((((n-i)*(n-i))-(n-i))/2)+j-i-1;
	return d[val];
}


void fitness(const double *d, Individuo *individuo, int n, int m)
{
	// Determina la calidad del individuo calculando la suma de la distancia entre cada par de enteros
	// 235. distancia del 2 al 3 mas 2 al 5 mas 3 al 5. la suma de uno a todos los de la derecha orden cuadratico. FOR i hasta n -1(FOR j = i+1 hasta n)
	//el fitnes s mas fitness mas distania la podemos hacer en distancia, esta distancia es la formula de K 
	double fitness = 0.0;
	//Doble bucle para recorrer todas las posiciones del individuo para calcular su fitness
	//Se suman todas las combinaciones de los valores del subconjunto tomando 
	//como valor su representacion en el array de distancias.
	for(int i = 0 ; i < m-1 ; i++){
		for(int j = i + 1; j < m ; j++){
			fitness += distancia_ij(d,individuo->array_int[i], individuo->array_int[j], n);
		}
	}
	individuo->fitness = fitness;

}
