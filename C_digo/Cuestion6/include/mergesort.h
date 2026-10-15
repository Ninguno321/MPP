#ifndef MERGESORT_H
#define MERGESORT_H

#include "mh.h"

/* CUESTIÓN 6: ordena la población (array de punteros a Individuo) por
 * fitness, de mayor a menor. Sustituye a las llamadas a qsort sobre la
 * población. La versión se elige al compilar con -DVERSION=n. */
void ordenar_poblacion(Individuo **poblacion, int tam_pob);

/* Tiempo total acumulado ordenando la población (todas las ordenaciones
 * de la ejecución), en segundos. Se mide con omp_get_wtime. */
extern double t_ordenacion;

#endif
