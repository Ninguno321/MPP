/*
 * CUESTIÓN 6 - Sustitución de qsort por mergeSort y paralelización con tareas
 *
 * Se ordena la POBLACIÓN por fitness (el array de punteros a Individuo),
 * no los elementos de cada individuo.
 *
 * Individuo = { int *array_int; double fitness; }  (16 bytes)
 *
 * La versión se elige al compilar con -DVERSION=n:
 *
 *   VERSION 0 : qsort original (referencia, el código de partida).
 *   VERSION 1 : mergeSort secuencial + mezclar original (malloc por individuo
 *               y copia completa con memmove). Sirve de referencia secuencial.
 *   VERSION 2 : mergeSort con tareas OpenMP + mezclar original.
 *   VERSION 3 : (a) mergeSort con tareas + mezclar optimizado en memoria
 *               (solo se mueven punteros, un único buffer auxiliar).
 *   VERSION 4 : (b) mergeSort con tareas + cláusula if(tamaño > UMBRAL)
 *               + mezclar original.
 *   VERSION 5 : (a)+(b) versión final: mezclar optimizado + corte por umbral
 *               (por debajo de UMBRAL se ordena secuencialmente sin crear
 *               tareas) + el hilo que divide procesa él mismo una mitad.
 *
 * Por defecto (sin -DVERSION) se compila la VERSION 5.
 * El umbral se fija con -DUMBRAL=n (por defecto 64 individuos).
 *
 * Ejemplo: gcc -O2 -fopenmp -DVERSION=5 -DUMBRAL=128 ...
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <omp.h>
#include "../include/mergesort.h"

/* Comparador original de mh.c (mayor fitness primero), solo para VERSION 0 */
int comp_fitness(const void *a, const void *b);

#ifndef VERSION
#define VERSION 5
#endif

#ifndef UMBRAL
#define UMBRAL 64
#endif

/* Criterio de orden. El MDP es un problema de MAXIMIZACIÓN, así que se deja
 * el mejor individuo (mayor fitness) en la posición 0, igual que hacía qsort.
 * Si el comparador de qsort del código original ordenaba al revés, cambiar
 * ">" por "<" aquí y en ningún sitio más. */
#define ANTES(a, b) ((a)->fitness > (b)->fitness)

double t_ordenacion = 0.0;


/* =====================================================================
 *  MEZCLAR ORIGINAL (fichero mezclar.c completado)
 *  Usado en las versiones 1, 2 y 4.
 *  Mezcla los subvectores ordenados [izq, med) y [med, der).
 *  Coste de memoria: (der - izq) malloc + free por llamada (es lo caro)
 *  y dos copias de cada struct (16 bytes: el puntero array_int y el
 *  fitness). Copiar el struct es correcto: el array_int de cada individuo
 *  "viaja" con su fitness, solo cambia en qué struct está guardado.
 * ===================================================================== */
#if VERSION == 1 || VERSION == 2 || VERSION == 4
static void mezclar(Individuo **poblacion, int izq, int med, int der)
{
	int i, j, k;

	Individuo **pob = (Individuo **) malloc((der - izq) * sizeof(Individuo *));
	assert(pob);

	for (i = 0; i < (der - izq); i++) {
		pob[i] = (Individuo *) malloc(sizeof(Individuo));
		assert(pob[i]);
	}

	k = 0;
	i = izq;
	j = med;
	while ((i < med) && (j < der)) {
		if (ANTES(poblacion[i], poblacion[j])) {
			memmove(pob[k++], poblacion[i++], sizeof(Individuo));
		}
		else {
			memmove(pob[k++], poblacion[j++], sizeof(Individuo));
		}
	}

	/* Restos de la mitad izquierda */
	for (; i < med; i++) {
		memmove(pob[k++], poblacion[i], sizeof(Individuo));
	}

	/* Restos de la mitad derecha */
	for (; j < der; j++) {
		memmove(pob[k++], poblacion[j], sizeof(Individuo));
	}

	/* Copiar el resultado ordenado de vuelta a la población y liberar */
	i = 0;
	while (i < (der - izq)) {
		memmove(poblacion[i + izq], pob[i], sizeof(Individuo));
		free(pob[i]);
		i++;
	}
	free(pob);
}
#endif


/* =====================================================================
 *  MEZCLAR OPTIMIZADO (apartado a)
 *  Usado en las versiones 3 y 5.
 *  - No copia individuos: solo reordena los PUNTEROS de la población
 *    (8 bytes) en vez de copiar los structs dos veces.
 *  - No hace ningún malloc: usa un buffer auxiliar 'aux' del tamaño de la
 *    población reservado UNA vez antes de ordenar. Cada llamada usa
 *    aux[izq..der), así que llamadas sobre rangos disjuntos (tareas
 *    hermanas) nunca pisan la misma zona -> no hay condiciones de carrera.
 * ===================================================================== */
#if VERSION == 3 || VERSION == 5
static void mezclar_opt(Individuo **poblacion, Individuo **aux, int izq, int med, int der)
{
	int i = izq, j = med, k = izq;

	while ((i < med) && (j < der)) {
		if (ANTES(poblacion[i], poblacion[j])) {
			aux[k++] = poblacion[i++];
		}
		else {
			aux[k++] = poblacion[j++];
		}
	}
	while (i < med) aux[k++] = poblacion[i++];
	while (j < der) aux[k++] = poblacion[j++];

	memcpy(&poblacion[izq], &aux[izq], (der - izq) * sizeof(Individuo *));
}
#endif


/* =====================================================================
 *  VERSIÓN 1: mergeSort secuencial (el del enunciado)
 * ===================================================================== */
#if VERSION == 1
static void mergeSort(Individuo **poblacion, int izq, int der)
{
	int med = (izq + der) / 2;
	if ((der - izq) < 2) { return; }
	mergeSort(poblacion, izq, med);
	mergeSort(poblacion, med, der);
	mezclar(poblacion, izq, med, der);
}
#endif


/* =====================================================================
 *  VERSIÓN 2: mergeSort con tareas
 *  - Cada mitad es una tarea independiente (trabajan sobre rangos
 *    disjuntos del vector, no hay dependencias entre ellas).
 *  - taskwait: no se puede mezclar hasta que ambas mitades estén
 *    ordenadas (espera solo a las tareas hijas de esta tarea).
 *  - poblacion es un puntero: al hacerlo firstprivate cada tarea tiene
 *    su copia del puntero, pero todas apuntan al MISMO vector (compartido).
 *    izq, med, der deben ser firstprivate porque la tarea puede ejecutarse
 *    cuando la función padre ya ha cambiado o perdido esas variables.
 * ===================================================================== */
#if VERSION == 2
static void mergeSort_tareas(Individuo **poblacion, int izq, int der)
{
	int med = (izq + der) / 2;
	if ((der - izq) < 2) { return; }

	#pragma omp task default(none) firstprivate(poblacion, izq, med)
	mergeSort_tareas(poblacion, izq, med);

	#pragma omp task default(none) firstprivate(poblacion, med, der)
	mergeSort_tareas(poblacion, med, der);

	#pragma omp taskwait
	mezclar(poblacion, izq, med, der);
}
#endif


/* =====================================================================
 *  VERSIÓN 3: tareas + mezclar optimizado (apartado a)
 *  Igual que la 2 pero sin mallocs ni copias de structs.
 * ===================================================================== */
#if VERSION == 3
static void mergeSort_tareas_opt(Individuo **poblacion, Individuo **aux, int izq, int der)
{
	int med = (izq + der) / 2;
	if ((der - izq) < 2) { return; }

	#pragma omp task default(none) firstprivate(poblacion, aux, izq, med)
	mergeSort_tareas_opt(poblacion, aux, izq, med);

	#pragma omp task default(none) firstprivate(poblacion, aux, med, der)
	mergeSort_tareas_opt(poblacion, aux, med, der);

	#pragma omp taskwait
	mezclar_opt(poblacion, aux, izq, med, der);
}
#endif


/* =====================================================================
 *  VERSIÓN 4: tareas con cláusula if (apartado b)
 *  if(expr) falso -> la tarea es "undeferred": la ejecuta inmediatamente
 *  el hilo que la encuentra, no se encola. Así solo se reparten entre
 *  hilos los trozos grandes. OJO: el runtime sigue creando la estructura
 *  de la tarea aunque no la encole, así que el ahorro es parcial.
 * ===================================================================== */
#if VERSION == 4
static void mergeSort_tareas_if(Individuo **poblacion, int izq, int der)
{
	int med = (izq + der) / 2;
	if ((der - izq) < 2) { return; }

	#pragma omp task default(none) firstprivate(poblacion, izq, med) if((der - izq) > UMBRAL)
	mergeSort_tareas_if(poblacion, izq, med);

	#pragma omp task default(none) firstprivate(poblacion, med, der) if((der - izq) > UMBRAL)
	mergeSort_tareas_if(poblacion, med, der);

	#pragma omp taskwait
	mezclar(poblacion, izq, med, der);
}
#endif


/* =====================================================================
 *  VERSIÓN 5: versión final, (a) + (b)
 *  1. Por debajo de UMBRAL se llama a una función totalmente secuencial:
 *     cero sobrecarga de OpenMP (mejor que if(), que aún crea la tarea).
 *  2. Solo se crea UNA tarea por nivel (mitad izquierda); la mitad derecha
 *     la ordena el propio hilo. Se crean la mitad de tareas y el hilo no
 *     se queda parado esperando en el taskwait.
 *  3. mezclar optimizado: sin malloc, solo punteros.
 * ===================================================================== */
#if VERSION == 5
static void mergeSort_seq_opt(Individuo **poblacion, Individuo **aux, int izq, int der)
{
	int med = (izq + der) / 2;
	if ((der - izq) < 2) { return; }
	mergeSort_seq_opt(poblacion, aux, izq, med);
	mergeSort_seq_opt(poblacion, aux, med, der);
	mezclar_opt(poblacion, aux, izq, med, der);
}

static void mergeSort_final(Individuo **poblacion, Individuo **aux, int izq, int der)
{
	int med = (izq + der) / 2;

	if ((der - izq) <= UMBRAL) {
		mergeSort_seq_opt(poblacion, aux, izq, der);
		return;
	}

	#pragma omp task default(none) firstprivate(poblacion, aux, izq, med)
	mergeSort_final(poblacion, aux, izq, med);

	mergeSort_final(poblacion, aux, med, der);   /* la hace este mismo hilo */

	#pragma omp taskwait
	mezclar_opt(poblacion, aux, izq, med, der);
}
#endif


/* =====================================================================
 *  PUNTO DE ENTRADA: sustituye a la llamada a qsort en mh.c
 *  - Las tareas necesitan un equipo de hilos: se abre una región paralela
 *    y UN solo hilo (single) lanza la recursión; el resto de hilos del
 *    equipo esperan en la barrera implícita del single ejecutando las
 *    tareas que se van generando.
 *  - Se mide el tiempo de la ordenación y se acumula en t_ordenacion.
 * ===================================================================== */
void ordenar_poblacion(Individuo **poblacion, int tam_pob)
{
	double t0 = omp_get_wtime();

#if VERSION == 0
	qsort(poblacion, tam_pob, sizeof(Individuo *), comp_fitness);

#elif VERSION == 1
	mergeSort(poblacion, 0, tam_pob);

#elif VERSION == 2
	#pragma omp parallel default(none) shared(poblacion, tam_pob)
	#pragma omp single
	mergeSort_tareas(poblacion, 0, tam_pob);

#elif VERSION == 4
	#pragma omp parallel default(none) shared(poblacion, tam_pob)
	#pragma omp single
	mergeSort_tareas_if(poblacion, 0, tam_pob);

#elif VERSION == 3 || VERSION == 5
	/* Un único buffer auxiliar para toda la ordenación */
	Individuo **aux = (Individuo **) malloc(tam_pob * sizeof(Individuo *));
	assert(aux);

	#pragma omp parallel default(none) shared(poblacion, aux, tam_pob)
	#pragma omp single
	{
	#if VERSION == 3
		mergeSort_tareas_opt(poblacion, aux, 0, tam_pob);
	#else
		mergeSort_final(poblacion, aux, 0, tam_pob);
	#endif
	}

	free(aux);

#else
	#error "VERSION debe estar entre 0 y 5"
#endif

	t_ordenacion += omp_get_wtime() - t0;
}
