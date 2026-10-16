#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#if defined (_OPENMP)
#include <omp.h>
#endif

#include "../include/mh.h"

#define PRINT 0
#define PCT_CONVER 0.02   // valor porcentual de mejora esperado (2%)
#define NGENERACIONES_ATRAS 15      // se compara con el mejor fitness de hace X generaciones y esto es porque a lo mejor puede haber una generacion sin mejora pero luego otra que si mejora

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
    // crea poblacion inicial (array de individuos)
    Individuo **poblacion = (Individuo **) malloc(tam_pob * sizeof(Individuo *));
    assert(poblacion);

    // Variables que leen/escriben todos los hilos, es decir, compartidas.
    // Solo se modifican dentro de single y la barrera implicita al final del
    // single garantiza que todos los hilos ven el mismo valor despues.
    double fitness_modelo = 0.0;   // para saber cuando mejora una generacion
    int ngeneracion = 0;           // generaciones seguidas sin mejora suficiente
    double value = 0.0;            // mejor fitness obtenido (valor devuelto)

    // Usamos una sola region paralela para todo el algoritmo.
    // Asi los hilos se crean una vez (fork) y se sincronizan al final (join),
    // en lugar de crear y destruir el conjunto de hilos en cada generacion.
    // default(none) obliga a declarar el alcance de cada variable y evita
    // comparticiones accidentales que nos es útil.
    #pragma omp parallel default(none) \
        shared(d, n, m, n_gen, tam_pob, m_rate, sol, poblacion, fitness_modelo, ngeneracion, value)
    {
        // Semilla distinta para cada hilo (si no, todos generarian la misma secuencia)
        int id = 0;
#if defined (_OPENMP)
        id = omp_get_thread_num();
#endif
        semilla = (unsigned int) (time(NULL) + getpid() + 7919 * id);

        // crea cada individuo (array de enteros aleatorios) y calcula su fitness.
        // Las iteraciones son independientes (cada una escribe solo poblacion[i]).
        // schedule(dynamic): crear_individuo descarta aleatoriamente valores repetidos,
        // por lo que el coste de cada iteracion es variable.
        #pragma omp for schedule(dynamic)
        for(int i = 0; i < tam_pob; i++) {
            poblacion[i] = (Individuo *) malloc(sizeof(Individuo));
            poblacion[i]->array_int = crear_individuo(n, m);

            // calcula el fitness del individuo
            fitness(d, poblacion[i], n, m);
        }
        

        // ordena individuos segun la funcion de bondad (mayor "fitness" quiere decir que son mas aptos).
        // qsort es secuencial y opera sobre todo el array  y lo hace un unico hilo.
        #pragma omp single
        {
            qsort(poblacion, tam_pob, sizeof(Individuo *), comp_fitness);
            fitness_modelo = poblacion[0]->fitness;
            ngeneracion = 0;
        }
        // (barrera implicita del single por lo que todos ven fitness_modelo y ngeneracion)

        // Bucle de generaciones no se puede repartir entre hilos porque cada
        // generacion depende de la anterior. Lo ejecutan todos los hilos (g es
        // privada al declararse dentro de la region) y se reparte el trabajo de
        // dentro de cada generacion. Todos evaluan la condicion con el miso
        // valor de ngeneracion, asi que todos salen del bucle a la vez.
        //Mantenemos n_gen como limite para evitar un posible bucle infinito
        for(int g = 0; g < n_gen && ngeneracion < NGENERACIONES_ATRAS; g++)
        {
            // los hijos de los ascendientes mas aptos sustituyen a la ultima mitad de los individuos menos aptos.
            // Cada iteracion lee dos padres de la primera mitad y escribe dos hijos
            // distintos de la segunda mitad por lo que son iteraciones independientes.
            // schedule(dynamic) ya que la reparacion de duplicados en cruzar tiene coste aleatorio.
            #pragma omp for schedule(dynamic)
            for(int i = 0; i < (tam_pob/2) - 1; i += 2) {
                cruzar(poblacion[i], poblacion[i+1], poblacion[tam_pob/2 + i], poblacion[tam_pob/2 + i + 1], n, m);
            }
            // (barrera implicita que en este caso es necesaria ya que se mutan individuos del rango
            //  [tam_pob/4, tam_pob/2) que son padres en el cruce no se pueden
            //  mutar hasta que todos los cruces hayan terminado de leerlos)

            // Mutacion de 3/4 de la poblacion (a partir de tam_pob/4) y recalculo
            // del fitness fusionados en un unico bucle: el fitness de poblacion[i]
            // solo depende de la mutacion de ese mismo individuo, asi que no hace
            // falta esperar a que termine toda la mutacion por lo que nos ahorramos una
            // barrera y un reparto de iteraciones por generacion.
            // schedule(static) el coste lo domina fitness, igual para
            // todos los individuos, por lo que el reparto estatico equilibra bien y
            // no tiene la sobrecarga del dinamico.
            #pragma omp for schedule(static)
            for(int i = 0; i < tam_pob; i++) {
                // muta 3/4 partes de la poblacion
                if (i >= tam_pob/4) {
                    mutar(poblacion[i], n, m, m_rate);
                }
                // recalcula el fitness del individuo
                fitness(d, poblacion[i], n, m);
            }
            // (barrera implicita lo que implica que todos los fitness calculados antes de ordenar)

            // Ordenacion y criterio de convergencia es secuencial, lo hace un hilo.
            // La barrera implicita al final del single asegura que todos los hilos
            // leen el ngeneracion actualizado en la condicion del bucle.
            #pragma omp single
            {
                // ordena individuos segun la funcion de bondad (mayor "fitness" equivale a que son mas aptos)
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
                    ngeneracion = 0;
                }else{
                    ngeneracion++;
                }

                if (PRINT) {
                    printf("Generacion %d - ", g);
                    printf("Fitness = %.0lf\n", poblacion[0]->fitness);
                }
            }
        }

        // Copia de la solucion secuencial y muy barata realizada por un unico hilo.
        // La barrera del single impide liberar la memoria antes de haber copiado.
        #pragma omp single
        {
            // ordena el array solucion
            qsort(poblacion[0]->array_int, m, sizeof(int), comp_array_int);

            // y lo mueve a sol para escribirlo
            memmove(sol, poblacion[0]->array_int, m*sizeof(int));

            // almacena el mejor valor obtenido para el fitness
            value = poblacion[0]->fitness;
        }

        // se libera la memoria reservada. Cada iteracion libera un individuo
        // distinto por lo que son independientes. nowait: la barrera implicita al final de
        // la region paralela ya sincroniza, no hace falta otra.
        #pragma omp for schedule(static) nowait
        for (int i = 0; i < tam_pob; i++){
            free(poblacion[i]->array_int);
            free(poblacion[i]);
        }
    } // fin de la region paralela (barrera implicita)

    free(poblacion);
    // devuelve el valor obtenido para el fitness
    return value;
}

// cruzar, mutar, distancia_ij y fitness no tienen directivas internas porque 
// se llaman ya desde bucles paralelos sobre la poblacion (un individuo o pareja
// por iteracion). Paralelizar tambien su interior seria paralelismo anidado,
// con regiones cuya sobrecarga superaria la ganancia, ya que tam_pob es
// normalmente mucho mayor que el numero de hilos. Son seguras para ejecutarse
// en paralelo y solo usan variables locales, escriben unicamente en
// sus individuos y generan aleatorios con rand_r sobre la semilla del hilo.

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
    // La fórmula solo es válida con i < j; como la distancia es simétrica,
    // intercambiamos si hace falta.
    if (i > j) { int t = i; i = j; j = t; }
    int val = (((n * n)- n)/2)-((((n-i)*(n-i))-(n-i))/2)+j-i-1;
    return d[val];
}


void fitness(const double *d, Individuo *individuo, int n, int m)
{
    // Determina la calidad del individuo calculando la suma de la distancia entre cada par de enteros
    double fitness = 0.0;
    //Doble bucle para recorrer todas las posiciones del individuo para calcular su fitness
    for(int i = 0 ; i < m-1 ; i++){
        for(int j = i + 1; j < m ; j++){
            fitness += distancia_ij(d,individuo->array_int[i], individuo->array_int[j], n);
        }
    }
    individuo->fitness = fitness;
}










