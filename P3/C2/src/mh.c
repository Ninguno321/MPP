#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stddef.h>   
#include <mpi.h>

// P3 CUESTIÓN 1: cabecera de OpenMP (solo si se compila con -fopenmp)
#if defined (_OPENMP)
#include <omp.h>
#endif

#include "../include/mh.h"

#define PRINT 0
#define PCT_CONVER 0.02   // valor porcentual de mejora esperado (2%)
#ifdef SIN_CONVERGENCIA
// CUESTIÓN 2: versión de trabajo fijo para medir prestaciones (se compila con -DSIN_CONVERGENCIA).
// Con un valor tan grande nunca se cumple el criterio de convergencia y todas las ejecuciones
// hacen n_gen generaciones, sea cual sea el número de procesos.
#define NGENERACIONES_ATRAS 1000000
#else
#define NGENERACIONES_ATRAS 15      // se compara con el mejor fitness de hace X generaciones y esto es porque a lo mejor puede haber una generacion sin mejora pero luego otra que si mejora
#endif

// P3 CUESTIÓN 1: rand() usa un único estado interno compartido. Con varios hilos, o todos
// generan la misma secuencia o las llamadas se serializan y se pierde paralelismo.
// rand_r recibe el estado (la semilla) como parámetro, así que es thread safe, y con
// threadprivate cada hilo tiene su propia copia de la semilla
static unsigned int semilla = 1;
#pragma omp threadprivate(semilla)

int aleatorio(int n) {
	return (rand_r(&semilla) % n);  // genera un numero aleatorio entre 0 y n-1
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

//ahora ya no lo reserva porque lo hacemos en struct y devuelve, solo inicializa sus elementos
void crear_individuo(int n, int m, int *individuo)
{
	int i=0, value;
	
	// inicializa array de elementos (ya reservado dentro del struct)
	memset(individuo, -1, m * sizeof(int));
	
	while(i < m) {
		value = aleatorio(n);
		// si el nuevo elemento no está en el array...
		if(!find_element(individuo, i, value)) {
			individuo[i] = value;  // lo incluimos
			i++;
		}
	}
}

int comp_array_int(const void *a, const void *b) {
	return (*(int *)a - *(int *)b);
}


int comp_fitness(const void *a, const void *b) {
	/* qsort pasa un puntero al elemento que está ordenando */
	double fa = (*(Individuo **)a)->fitness;
	double fb = (*(Individuo **)b)->fitness;
	return (fb > fa) - (fb < fa);   // orden descendente sin truncar a int
}

//nuevas funciones
// Copia los k mejores individuos (población ya ordenada) a un buffer contiguo
void copiar_mejores(Individuo **poblacion, Individuo *buffer, int k)
{
	for (int i = 0; i < k; i++)
		buffer[i] = *poblacion[i];
}

// Sustituye los k peores individuos por los recibidos y reordena la población
void reemplazar_peores(Individuo **poblacion, int tam, Individuo *recibidos, int k)
{
	for (int i = 0; i < k; i++)
		*poblacion[tam - 1 - i] = recibidos[i];
	qsort(poblacion, tam, sizeof(Individuo *), comp_fitness);
}


/**
 *  CUESTIÓN 2 (fichero derivados_mpi.c proporcionado)
 *  Crea un nuevo tipo de datos derivado en MPI
 *  Necesario para el envio/recepcion de mensajes con datos de tipo Individuo
 *  Solo se envían los m genes que se usan (no los MAX_M) y el fitness. Como el tipo
 *  abarca desde el inicio del struct hasta el final de fitness, su extensión coincide con
 *  sizeof(Individuo) y se pueden enviar arrays contiguos de Individuo con count > 1.
 **/
void crear_tipo_datos(int m, MPI_Datatype *individuo_type)
{
	int blocklen[2] = {m, 1};
	MPI_Datatype dtype[2] = { MPI_INT, MPI_DOUBLE };

	MPI_Aint disp[2];
	disp[0] = offsetof(Individuo, array_int);
	disp[1] = offsetof(Individuo, fitness);

	MPI_Type_create_struct(2, blocklen, disp, dtype, individuo_type);
	MPI_Type_commit(individuo_type);
}

/*
 *  P3 CUESTIÓN 2: algoritmo híbrido MPI + OpenMP.
 *
 *  Nivel MPI (P2, versión elegida en la cuestión 6: comunicaciones colectivas): cada proceso
 *  es una isla con tam_pob/p individuos; cada NGM generaciones el proceso 0 recoge los NEM
 *  mejores de cada isla con MPI_Gather y difunde los NEM mejores globales y la variable
 *  continuar con MPI_Bcast.
 *
 *  Nivel OpenMP (P1, versión final de la cuestión 8): dentro de cada proceso, una única
 *  región paralela para todo el algoritmo, con los bucles sobre la población repartidos
 *  entre los hilos:
 *    - población inicial:    for schedule(dynamic)  (coste variable de crear_individuo)
 *    - cruce:                for schedule(dynamic)  (coste variable al reparar duplicados)
 *    - mutación + fitness:   for schedule(static)   (coste dominado por fitness, uniforme)
 *    - ordenación:           single
 *  Las comunicaciones MPI solo las hace el hilo maestro de cada proceso (master + barrier),
 *  por eso basta con el nivel MPI_THREAD_FUNNELED (ver main.c).
 */
double aplicar_mh(const double *d, int n, int m, int n_gen, int tam_pob, double m_rate, int *sol, int NGM, int NEM)
{
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

	assert(m <= MAX_M);   // el individuo tiene que caber en el array estático

	// tipo derivado MPI para enviar y recibir individuos
	MPI_Datatype individuo_type;
	crear_tipo_datos(m, &individuo_type);

	// tamaño de la isla de este proceso. Si tam_pob no es divisible entre size,
	// los primeros (tam_pob % size) procesos tienen un individuo más
	int tam_local = tam_pob / size;
	if (rank < tam_pob % size)
		tam_local++;

	// bloque contiguo con todos los individuos de la isla (para MPI)
	Individuo *bloque = (Individuo *) malloc(tam_local * sizeof(Individuo));
	assert(bloque);

	// array de punteros al bloque que es lo que se ordena con qsort
	Individuo **poblacion = (Individuo **) malloc(tam_local * sizeof(Individuo *));
	assert(poblacion);

	// buffers para la migración
	Individuo *emigrantes = (Individuo *) malloc(NEM * sizeof(Individuo));
	Individuo *inmigrantes = (Individuo *) malloc(NEM * sizeof(Individuo));
	assert(emigrantes && inmigrantes);

	// solo el proceso 0: mezcla con los NEM*size individuos de todas las islas
	Individuo *mezcla = NULL;
	Individuo **p_mezcla = NULL;
	if (rank == 0) {
		mezcla = (Individuo *) malloc(NEM * size * sizeof(Individuo));
		p_mezcla = (Individuo **) malloc(NEM * size * sizeof(Individuo *));
		assert(mezcla && p_mezcla);
	}

	// variables compartidas por los hilos del proceso. fitness_modelo y ngeneracion
	// solo las usa el hilo maestro; continuar la escribe el maestro y la leen todos
	// después de una barrera
	double fitness_modelo = 0.0;
	int ngeneracion = 0;
	int continuar = 1;
	double value = 0.0;

	// P3: con default(none) hay que declarar el alcance de todo lo que se usa dentro, y en
	// OpenMPI MPI_COMM_WORLD y MPI_INT son variables globales; se copian a variables locales
	MPI_Comm comm = MPI_COMM_WORLD;
	MPI_Datatype mpi_int = MPI_INT;

	// P3: una única región paralela para todo el algoritmo (como en la P1): los hilos
	// se crean una vez y no en cada generación
	#pragma omp parallel default(none) \
		shared(d, n, m, n_gen, tam_pob, m_rate, sol, NGM, NEM, rank, size, individuo_type, comm, mpi_int, \
		       tam_local, bloque, poblacion, emigrantes, inmigrantes, mezcla, p_mezcla, \
		       fitness_modelo, ngeneracion, continuar, value)
	{
		// P3 CUESTIÓN 1: semilla distinta para cada hilo de cada proceso
		int id = 0;
#if defined (_OPENMP)
		id = omp_get_thread_num();
#endif
		semilla = (unsigned int) (time(NULL) + getpid() + rank * 1000 + 7919 * id);

		// población inicial: iteraciones independientes (cada una escribe solo su
		// individuo) y de coste variable, por eso schedule(dynamic)
		#pragma omp for schedule(dynamic)
		for (int i = 0; i < tam_local; i++) {
			poblacion[i] = &bloque[i];
			crear_individuo(n, m, poblacion[i]->array_int);
			fitness(d, poblacion[i], n, m);
		}

		#pragma omp single
		{
			qsort(poblacion, tam_local, sizeof(Individuo *), comp_fitness);
			// valor inicial del modelo: el mejor de la isla 0 (solo lo usa el proceso 0)
			fitness_modelo = poblacion[0]->fitness;
		}

		// g la lleva cada hilo por su cuenta (privada): todos hacen las mismas
		// generaciones, así que tiene el mismo valor en todos los hilos y procesos
		int g = 0;
		while (g < n_gen && continuar)
		{
			// Computación: NGM generaciones en la isla, sin comunicaciones
			int gen_bloque = 0;
			for (int k = 0; k < NGM && g < n_gen; k++, g++, gen_bloque++)
			{
				// cruce: cada iteración lee dos padres de la primera mitad y escribe dos
				// hijos distintos de la segunda mitad (independientes); coste variable
				#pragma omp for schedule(dynamic)
				for (int i = 0; i < (tam_local/2) - 1; i += 2) {
					cruzar(poblacion[i], poblacion[i+1], poblacion[tam_local/2 + i], poblacion[tam_local/2 + i + 1], n, m);
				}
				// (barrera implícita necesaria: la mutación modifica individuos que
				// son padres en el cruce)

				// mutación (3/4 de la isla) y fitness fusionados en un bucle: el fitness
				// de un individuo solo depende de su propia mutación; coste uniforme
				#pragma omp for schedule(static)
				for (int i = 0; i < tam_local; i++) {
					if (i >= tam_local/4)
						mutar(poblacion[i], n, m, m_rate);
					fitness(d, poblacion[i], n, m);
				}

				// ordenación secuencial, la hace un hilo (barrera implícita al final)
				#pragma omp single
				qsort(poblacion, tam_local, sizeof(Individuo *), comp_fitness);
			}

			// Migración: solo el hilo maestro de cada proceso llama a MPI
			#pragma omp master
			{
				copiar_mejores(poblacion, emigrantes, NEM);
				MPI_Gather(emigrantes, NEM, individuo_type, mezcla, NEM, individuo_type, 0, comm);

				if (rank == 0) {
					for (int i = 0; i < NEM * size; i++)
						p_mezcla[i] = &mezcla[i];
					qsort(p_mezcla, NEM * size, sizeof(Individuo *), comp_fitness);
					copiar_mejores(p_mezcla, inmigrantes, NEM);

					// convergencia con el mejor global, contando las generaciones del bloque
					double fitness_esperado = fitness_modelo * (1 + PCT_CONVER);
					if (p_mezcla[0]->fitness > fitness_esperado) {
						fitness_modelo = p_mezcla[0]->fitness;
						ngeneracion = 0;
					} else {
						ngeneracion += gen_bloque;
					}
					continuar = (ngeneracion < NGENERACIONES_ATRAS);

					if (PRINT) {
						printf("Generacion %d - ", g);
						printf("Fitness = %.0lf\n", p_mezcla[0]->fitness);
					}
				}

				MPI_Bcast(inmigrantes, NEM, individuo_type, 0, comm);
				MPI_Bcast(&continuar, 1, mpi_int, 0, comm);

				reemplazar_peores(poblacion, tam_local, inmigrantes, NEM);
			}
			// master no tiene barrera implícita: hace falta para que ningún hilo siga con
			// la población ni lea continuar antes de que el maestro termine la migración
			#pragma omp barrier
		}

		// Resultado final: el proceso 0 recoge el mejor de cada isla (solo el maestro)
		#pragma omp master
		{
			Individuo mejor = *poblacion[0];
			MPI_Gather(&mejor, 1, individuo_type, mezcla, 1, individuo_type, 0, comm);
			if (rank == 0) {
				for (int k = 1; k < size; k++) {
					if (mezcla[k].fitness > mejor.fitness)
						mejor = mezcla[k];
				}
			}
			qsort(mejor.array_int, m, sizeof(int), comp_array_int);
			memmove(sol, mejor.array_int, m*sizeof(int));
			value = mejor.fitness;
		}
	} // fin de la región paralela (barrera implícita)

	free(bloque);
	free(poblacion);
	free(emigrantes);
	free(inmigrantes);
	if (rank == 0) {
		free(mezcla);
		free(p_mezcla);
	}
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
	// m_rate es una probabilidad en [0,1]; aleatorio(100) es un entero
	// en [0,99], por lo que se compara con el porcentaje m_rate*100 
    //haciendo redondeo a un enter.
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
