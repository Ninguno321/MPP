#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>
#include <stddef.h>   
#include <mpi.h>

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
 *  CUESTIÓN 4: hace avanzar las comunicaciones asíncronas pendientes.
 *  Los mensajes de individuos son grandes (unos KB), y MPI solo los transmite cuando los
 *  procesos implicados hacen alguna llamada a MPI. Si un proceso está calculando sin llamar a
 *  MPI, el otro se queda esperando. MPI_Test no bloquea: comprueba si cada operación ha
 *  terminado y, de paso, hace que avance. Se llama mientras se calcula el fitness.
 */
void progresar(MPI_Request *reqs, int n)
{
	int terminada;
	for (int i = 0; i < n; i++)
		MPI_Test(&reqs[i], &terminada, MPI_STATUS_IGNORE);
}

/*
 *  CUESTIÓN 2: algoritmo evolutivo con modelo de islas y comunicaciones síncronas.
 *  CUESTIÓN 4: misma estructura, pero con comunicaciones asíncronas (MPI_Isend, MPI_Irecv,
 *  MPI_Wait y MPI_Test). Las islas no esperan la respuesta del 0 después de enviar: siguen
 *  calculando y la incorporan en cuanto llega.
 *
 *  Cada proceso (también el 0) evoluciona su isla por su cuenta durante NGM generaciones
 *  y después hay una migración:
 *    1. Cada isla envía al proceso 0 sus NEM mejores individuos (MPI_Ssend).
 *    2. El proceso 0 los recibe con MPI_ANY_SOURCE, en el orden en que llegan, y los junta
 *       con los suyos. Así no se queda esperando al proceso i si el j ya ha terminado.
 *    3. El proceso 0 ordena la mezcla, decide si se ha convergido y envía a cada isla los
 *       NEM mejores y la variable continuar (MPI_Ssend).
 *    4. Cada isla sustituye sus NEM peores por los recibidos y reordena.
 *  Al final cada isla envía su mejor individuo al proceso 0, que se queda con el mejor.
 *
 *  Primitivas que hemos usado:
 *    - MPI_Ssend: envío síncrono, que termina cuando el receptor ha empezado a recibir.
 *    - MPI_Recv:  recepción bloqueante.
 *  Para cambiar a envío estándar basta con sustituir MPI_Ssend por MPI_Send (mismos
 *  parámetros). No hay interbloqueo porque las islas siempre envían primero y luego
 *  reciben, y el proceso 0 siempre recibe primero y luego envía.
 */
double aplicar_mh(const double *d, int n, int m, int n_gen, int tam_pob, double m_rate, int *sol, int NGM, int NEM)
{
	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);

        // para reiniciar la secuencia pseudoaleatoria en cada ejecución
        // CUESTIÓN 2: le sumamos el rank para asegurar que cada isla tenga una semilla distinta
        // y no empiecen todas con los mismos individuos
        srand(time(NULL) + getpid() + rank * 1000);
	int i, g, k, mutation_start;

	assert(m <= MAX_M);   // el individuo tiene que caber en el array estático ( esto lo ponemos por si en un futuro ejecutamos con un m mayor que MAX_M, el programa se pare con un mensaje claro en vez de escribir fuera del array)

	// CUESTIÓN 2: tipo derivado MPI para enviar y recibir individuos
	MPI_Datatype individuo_type;
	crear_tipo_datos(m, &individuo_type);

	// CUESTIÓN 2: tamaño de la isla de este proceso. Si tam_pob no es divisible entre
	// size, los primeros (tam_pob % size) procesos tienen un individuo más
	int tam_local = tam_pob / size;
	if (rank < tam_pob % size)
		tam_local++;

	// bloque contiguo con todos los individuos de la isla (para MPI)
	Individuo *bloque = (Individuo *) malloc(tam_local * sizeof(Individuo));
	assert(bloque);

	// array de punteros al bloque que es lo que se ordena con qsort
	Individuo **poblacion = (Individuo **) malloc(tam_local * sizeof(Individuo *));
	assert(poblacion);

	// CUESTIÓN 2: cada proceso crea su propia isla (no la reparte el 0, así también se
	// reparte este trabajo y no hay que enviar la población inicial)
	for(i = 0; i < tam_local; i++) {
		poblacion[i] = &bloque[i];
		crear_individuo(n, m, poblacion[i]->array_int);

		// calcula el fitness del individuo
		fitness(d, poblacion[i], n, m);
	}

	// ordena individuos segun la funcion de bondad (mayor "fitness" entonces mas aptos)
	qsort(poblacion, tam_local, sizeof(Individuo *), comp_fitness);

	// CUESTIÓN 2: buffers para la migración
	// emigrantes: los NEM mejores de esta isla, contiguos para poder enviarlos en un mensaje
	// inmigrantes: los NEM mejores globales que manda el proceso 0
	Individuo *emigrantes = (Individuo *) malloc(NEM * sizeof(Individuo));
	Individuo *inmigrantes = (Individuo *) malloc(NEM * sizeof(Individuo));
	assert(emigrantes && inmigrantes);

	// solo el proceso 0: mezcla con los NEM*size individuos que llegan de todas las islas
	// y array de punteros a la mezcla para ordenarla con qsort sin mover los structs
	Individuo *mezcla = NULL;
	Individuo **p_mezcla = NULL;
	if (rank == 0) {
		mezcla = (Individuo *) malloc(NEM * size * sizeof(Individuo));
		p_mezcla = (Individuo **) malloc(NEM * size * sizeof(Individuo *));
		assert(mezcla && p_mezcla);
	}

	// CUESTIÓN 2: criterio de convergencia. Ahora lo decide el proceso 0 en cada migración
	// mirando el mejor fitness global, y se lo comunica al resto con la variable continuar.
	// Si cada isla decidiera por su cuenta, una podría parar mientras las demás le
	// envían individuos y el programa se quedaría bloqueado.
	// Valor inicial: el mejor de la isla 0 (el 0 todavía no conoce el resto)
	double fitness_modelo = poblacion[0]->fitness;
	//Contador de generaciones seguidas sin la mejora esperada (solo lo usa el proceso 0)
	int ngeneracion = 0;
	int continuar = 1;


	// CUESTIÓN 4: peticiones de las comunicaciones asíncronas. Un buffer que se usa en un
	// MPI_Isend no se puede modificar, ni uno de un MPI_Irecv leer, hasta hacer MPI_Wait
	// sobre su petición. MPI_REQUEST_NULL indica que no hay operación pendiente (MPI_Wait
	// sobre ella vuelve enseguida)
	// islas: [0] envío de emigrantes al 0, [1] recepción de inmigrantes, [2] de continuar
	MPI_Request req_isla[3] = {MPI_REQUEST_NULL, MPI_REQUEST_NULL, MPI_REQUEST_NULL};
	int pendiente = 0;                         // islas: hay una respuesta del 0 por recibir
	int continuar_rec;                         // islas: aquí se recibe continuar
	MPI_Request *req_mig = NULL;               // proceso 0: recepciones de cada isla
	MPI_Request *req_resp = NULL;              // proceso 0: envíos a cada isla (2 por isla)
	if (rank == 0) {
		req_mig = (MPI_Request *) malloc(size * sizeof(MPI_Request));
		req_resp = (MPI_Request *) malloc(2 * size * sizeof(MPI_Request));
		assert(req_mig && req_resp);
		for (k = 0; k < size; k++)
			req_mig[k] = MPI_REQUEST_NULL;
		for (k = 0; k < 2 * size; k++)
			req_resp[k] = MPI_REQUEST_NULL;
	}

	g = 0;
	//Mantenemos n_gen como limite para evitar un posible bucle infinito
		while (g < n_gen && continuar)
	{
		// CUESTIÓN 4: el proceso 0 deja preparadas las recepciones de esta migración antes
		// de calcular, para que los individuos de las islas que acaben antes vayan llegando
		// mientras él evoluciona su isla (MPI_ANY_SOURCE: en el orden en que lleguen)
		if (rank == 0) {
			for (k = 1; k < size; k++)
				MPI_Irecv(&mezcla[k * NEM], NEM, individuo_type, MPI_ANY_SOURCE, 1,
						  MPI_COMM_WORLD, &req_mig[k]);
		}

		// Computación: NGM generaciones en la isla
		// (CUESTIÓN 4: una isla puede terminar el bloque antes si se entera de que el 0 ha
		// decidido parar)
		int gen_bloque = 0;
		for (k = 0; k < NGM && g < n_gen; k++, g++, gen_bloque++)
		{
			// los hijos de los ascendientes mas aptos sustituyen a la ultima mitad de los individuos menos aptos
			for(i = 0; i < (tam_local/2) - 1; i += 2) {
				cruzar(poblacion[i], poblacion[i+1], poblacion[tam_local/2 + i], poblacion[tam_local/2 + i + 1], n, m);
			}

			// inicia la mutacion a partir de 1/4 de la poblacion
			mutation_start = tam_local/4;

			// muta 3/4 partes de la poblacion
			for(i = mutation_start; i < tam_local; i++) {
				mutar(poblacion[i], n, m, m_rate);
			}

			// recalcula el fitness del individuo
			for(i = 0; i < tam_local; i++) {
				fitness(d, poblacion[i], n, m);

				// CUESTIÓN 4: entre individuo e individuo se hacen avanzar los mensajes
				// pendientes, para que el otro proceso no tenga que esperar a que este
				// termine la generación
				if (rank == 0) {
					progresar(req_mig, size);
					progresar(req_resp, 2 * size);
				} else {
					progresar(req_isla, 3);
				}
			}

			// ordena individuos segun la funcion de bondad (mayor "fitness" --> mas aptos)
			qsort(poblacion, tam_local, sizeof(Individuo *), comp_fitness);

			// CUESTIÓN 4: en cada generación la isla comprueba con MPI_Test, sin bloquearse,
			// si ya ha llegado la respuesta del 0 a su último envío. Si ha llegado, incorpora
			// los inmigrantes en ese momento y, si el 0 ha decidido parar, deja de calcular
			if (rank != 0 && pendiente) {
				int listo_ind, listo_cont;
				MPI_Test(&req_isla[1], &listo_ind, MPI_STATUS_IGNORE);
				MPI_Test(&req_isla[2], &listo_cont, MPI_STATUS_IGNORE);
				if (listo_ind && listo_cont) {
					pendiente = 0;
					continuar = continuar_rec;
					reemplazar_peores(poblacion, tam_local, inmigrantes, NEM);
					if (!continuar)
						break;
				}
			}
		}

		// CUESTIÓN 4: si el 0 ha decidido parar, la isla no hace más migraciones
		if (!continuar)
			break;

				// Migración 
		if (rank == 0) {
			// los NEM mejores de la isla 0 van directamente al principio de la mezcla
			for (i = 0; i < NEM; i++)
				mezcla[i] = *poblacion[i];

			// CUESTIÓN 4: se espera a que lleguen los individuos de todas las islas
			for (k = 1; k < size; k++)
				MPI_Wait(&req_mig[k], MPI_STATUS_IGNORE);

			// se ordena la mezcla (por punteros, con la misma función que la isla)
			for (i = 0; i < NEM * size; i++)
				p_mezcla[i] = &mezcla[i];
			qsort(p_mezcla, NEM * size, sizeof(Individuo *), comp_fitness);

			// CUESTIÓN 4: antes de sobrescribir inmigrantes y continuar hay que asegurarse de
			// que los envíos de la migración anterior han terminado
			for (k = 0; k < 2 * size; k++)
				MPI_Wait(&req_resp[k], MPI_STATUS_IGNORE);

			// los NEM mejores globales son los que se envían a todas las islas
			copiar_mejores(p_mezcla, inmigrantes, NEM);

			// convergencia con el mejor global y contando las
			// generaciones del bloque: si no hay una mejora del PCT_CONVER respecto al
			// modelo durante NGENERACIONES_ATRAS generaciones, se para
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

			// CUESTIÓN 4: se envían los mejores globales y continuar a cada isla sin esperar
			// a que terminen; el proceso 0 sigue con su isla y los espera en la siguiente
			// migración (o al final)
			for (k = 1; k < size; k++) {
				MPI_Isend(inmigrantes, NEM, individuo_type, k, 2, MPI_COMM_WORLD, &req_resp[2 * k]);
				MPI_Isend(&continuar, 1, MPI_INT, k, 3, MPI_COMM_WORLD, &req_resp[2 * k + 1]);
			}

			// la isla 0 pone los mejores globales en el lugar de sus peores y reordena
			reemplazar_peores(poblacion, tam_local, inmigrantes, NEM);
		} else {
			// CUESTIÓN 4: migración asíncrona. La isla no espera la respuesta del 0 justo
			// después de enviar, sino que sigue evolucionando y la incorpora en la siguiente
			// migración. Así, mientras el 0 junta los individuos de todas las islas, el resto
			// sigue calculando en vez de estar parado.

			// 1. Si la respuesta de la migración anterior no ha llegado durante el cálculo,
			// se espera a ella aquí y se incorpora
			if (pendiente) {
				MPI_Wait(&req_isla[1], MPI_STATUS_IGNORE);
				MPI_Wait(&req_isla[2], MPI_STATUS_IGNORE);
				pendiente = 0;
				continuar = continuar_rec;
				// los recibidos ocupan el lugar de los peores (se copia el contenido) y se reordena
				reemplazar_peores(poblacion, tam_local, inmigrantes, NEM);
				// si el 0 decidió parar en la migración anterior, ya no se envía nada más
				if (!continuar)
					break;
			}

			// 2. El envío anterior tiene que haber terminado antes de reutilizar emigrantes
			MPI_Wait(&req_isla[0], MPI_STATUS_IGNORE);

			// 3. Se envían los NEM mejores al 0 y se deja preparada la recepción de su
			// respuesta, sin esperar a ninguna de las dos operaciones
			copiar_mejores(poblacion, emigrantes, NEM);
			MPI_Isend(emigrantes, NEM, individuo_type, 0, 1, MPI_COMM_WORLD, &req_isla[0]);
			MPI_Irecv(inmigrantes, NEM, individuo_type, 0, 2, MPI_COMM_WORLD, &req_isla[1]);
			MPI_Irecv(&continuar_rec, 1, MPI_INT, 0, 3, MPI_COMM_WORLD, &req_isla[2]);
			pendiente = 1;
		}
	}

		// CUESTIÓN 4: antes de terminar se completan las operaciones pendientes, para que no
	// quede ningún mensaje sin recibir y se puedan liberar los buffers
	if (rank == 0) {
		for (k = 0; k < 2 * size; k++)
			MPI_Wait(&req_resp[k], MPI_STATUS_IGNORE);
	} else {
		if (pendiente) {
			MPI_Wait(&req_isla[1], MPI_STATUS_IGNORE);
			MPI_Wait(&req_isla[2], MPI_STATUS_IGNORE);
			// la última respuesta también se aprovecha
			reemplazar_peores(poblacion, tam_local, inmigrantes, NEM);
		}
		MPI_Wait(&req_isla[0], MPI_STATUS_IGNORE);
	}

	// Resultado final 
	// cada isla envía su mejor individuo al proceso 0, que se queda con el mejor de todos.
	// CUESTIÓN 4: el 0 deja preparadas a la vez las recepciones de todas las islas (en la
	// mezcla, que ya no se usa) con MPI_ANY_SOURCE y después espera a que lleguen
	Individuo mejor = *poblacion[0];
	if (rank == 0) {
		for (k = 1; k < size; k++)
			MPI_Irecv(&mezcla[k], 1, individuo_type, MPI_ANY_SOURCE, 4, MPI_COMM_WORLD, &req_mig[k]);
		for (k = 1; k < size; k++) {
			MPI_Wait(&req_mig[k], MPI_STATUS_IGNORE);
			if (mezcla[k].fitness > mejor.fitness)
				mejor = mezcla[k];
		}
	} else {
		MPI_Isend(&mejor, 1, individuo_type, 0, 4, MPI_COMM_WORLD, &req_isla[0]);
		MPI_Wait(&req_isla[0], MPI_STATUS_IGNORE);
	}
	// ordena el array solucion
	qsort(mejor.array_int, m, sizeof(int), comp_array_int);

        // y lo mueve a sol para escribirlo (solo el del proceso 0 es la solución global)
	memmove(sol, mejor.array_int, m*sizeof(int));

	// almacena el mejor valor obtenido para el fitness
	double value = mejor.fitness;

	// se libera la memoria reservada (un solo bloque y el array de punteros)
	free(bloque);
	free(poblacion);
	free(emigrantes);
	free(inmigrantes);
		if (rank == 0) {
		free(mezcla);
		free(p_mezcla);
		free(req_mig);
		free(req_resp);
	}

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
