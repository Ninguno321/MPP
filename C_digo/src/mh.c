#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "../include/mh.h"

#define MUTATION_RATE 0.15
#define PRINT 0

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

double aplicar_mh(const double *d, int n, int m, int n_gen, int tam_pob, int *sol)
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
	
	// evoluciona la poblacion durante un numero de generaciones
	for(g = 0; g < n_gen; g++)
	{
		// los hijos de los ascendientes mas aptos sustituyen a la ultima mitad de los individuos menos aptos
		for(i = 0; i < (tam_pob/2) - 1; i += 2) {
			cruzar(poblacion[i], poblacion[i+1], poblacion[tam_pob/2 + i], poblacion[tam_pob/2 + i + 1], n, m);
		}
		
		// inicia la mutacion a partir de 1/4 de la poblacion
		mutation_start = tam_pob/4;
		
		// muta 3/4 partes de la poblacion
		for(i = mutation_start; i < tam_pob; i++) {
			mutar(poblacion[i], n, m);
		}
		
		// recalcula el fitness del individuo
		for(i = 0; i < tam_pob; i++) {
			fitness(d, poblacion[i], n, m);
		}
		
		// ordena individuos segun la funcion de bondad (mayor "fitness" --> mas aptos)
		qsort(poblacion, tam_pob, sizeof(Individuo *), comp_fitness);
		
		if (PRINT) {
			printf("Generacion %d - ", g);
			printf("Fitness = %.0lf\n", poblacion[0]->fitness);
		}
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
	// devuelve el valor obtenido para el fitness
	return value;
}

void cruzar(Individuo *padre1, Individuo *padre2, Individuo *hijo1, Individuo *hijo2, int n, int m)
{	/*
	Cruzar: para cada par de individuos selecciona un punto aleatorio a partir del que se produce
	la mezcla y los nuevos descendientes se crean intercambiando los genes de los padres entre
	sí.
	*/
	
	// no pueden haber elementos repetidos, por lo que hay que llevar control con las mezclas del padre. revisamos el hijo y llamamos a aleatorio para 
	// que no tenga elementos repetidos
	// Elegir un "punto" de corte aleatorio a partir del que se realiza el intercambio de los genes
	
	// Los primeros genes del padre1 van al hijo1. Idem para el padre2 e hijo2.
	
	// Y los restantes son del otro padre, respectivamente.
	
	// Factibilizar: eliminar posibles repetidos de ambos hijos
	// Si encuentro alguno repetido en el hijo1, lo cambio por otro que no este en el conjunto

	//Primer paso, generar el punto de corte. Cruce en un solo punto.
	// El punto de corte no se incluye como gen izquierdo.
	int corte = 1 + aleatorio(m-1); //esto soluciona el problema de que salga corte = 0 y se copie directamente el padre2.

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
			} //Hay una opcion con un array de marcas que es mas eficiente.
		}
	}
}

void mutar(Individuo *actual, int n, int m)
{
	/*
		Mutar: produce mutaciones en los descendientes en función de una cierta probabilidad
(normalmente baja). Esto permite, por un lado, que la población sea diversa y, por otro, evita
que el algoritmo converja de forma prematura.
	*/


	//for m hasta cierta mutation rate 0.15
	// Decidir cuantos elementos mutar:
	// Si el valor es demasiado pequeño la convergencia es muy pequeña y si es demasiado alto diverge
	
	// Cambia el valor de algunos elementos de array_int de forma aleatoria
	// teniendo en cuenta que no puede haber elementos repetidos:
        // una posibilidad podría ser usar una variable, m_rate, para establecer la intensidad de la mutación 
        // (un bucle for con un número de iteraciones que dependa, por ejemplo, de m_rate*m)

	//Normalizamos para trabajar con enteros
	//Mutamos un gen de un individuo el 15% de las veces
	//La otra opcion es mutar cada gen con probabilidad 15% 
	//pero en numeros mayores mutamos una gran cantidad de genes 
	//por lo que no lo consideramos
	int nmutar = (int)(MUTATION_RATE * 100);
	
	if(aleatorio(100) < nmutar){
		int pos = aleatorio(m);
		int nuevo = 0;
		//nos ahorramos un if si siempre se generan entradas validas
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
