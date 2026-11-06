# Práctica 2 MPP – Cuestión 4 (comunicación asíncrona)

## Qué cambia respecto a la cuestión 2

Esta cuestión parte de la **cuestión 2** (tipo derivado `Individuo`), no de la 3. Así, al comparar las dos, solo se mide el efecto de pasar de comunicación síncrona a asíncrona.

Todos los `MPI_Ssend` / `MPI_Recv` pasan a ser `MPI_Isend` / `MPI_Irecv`. Se completan con `MPI_Wait` (espera a que la operación termine) o `MPI_Test` (comprueba si ha terminado sin bloquearse).

- **Islas**: envían sus mejores al proceso 0 y **siguen calculando** sin esperar la respuesta. En cada generación comprueban con `MPI_Test` si ha llegado la respuesta; si ha llegado, incorporan los inmigrantes en ese momento y, si el 0 ha decidido parar, paran.
- **Proceso 0**: deja preparadas las recepciones de todas las islas antes de calcular su propia isla, y envía las respuestas sin esperar a que terminen.
- **`progresar()`**: mientras se calcula el fitness, cada proceso llama a `MPI_Test` sobre sus operaciones pendientes. Sin esto, MPI no transmite los mensajes de individuos (son de varios KB) mientras el otro proceso calcula, y con NGM = 1 el programa tardaba el doble.
- **Matriz de distancias** (`main.c`): el proceso 0 lanza todos los envíos a la vez y después espera.
- **Final**: antes de terminar se completan todas las operaciones pendientes, para que no quede ningún mensaje sin recibir.

Los cambios están en `codigo/src/main.c` y `codigo/src/mh.c`, marcados con `CUESTIÓN 4`. El `Makefile`, `io.c` y los scripts son los mismos que en la cuestión 2.

## Estructura

```
C4/
├── codigo/
│   ├── src/        main.c, mh.c, io.c, Makefile, run.sh, prestaciones.sh, prestaciones_fijo.sh
│   ├── include/    mh.h, io.h
│   ├── igen/       generador de ficheros de entrada
│   └── output/     prestaciones.txt, prestaciones_fijo.txt (resultados de la cuestión 4)
└── memoria/
```

## 1. Ficheros de entrada

Copia la carpeta `input/` (la misma de las cuestiones anteriores) dentro de `codigo/`. Tiene que quedar así: `codigo/input/input_1000_400_200_100_0.3.txt` y `codigo/input/input_2000_800_400_200_0.3.txt`.

## 2. Compilar

Desde `codigo/src`:
```bash
make clean
make mpi        # versión normal (con convergencia)
make mpi_fijo   # versión de trabajo fijo (siempre hace n_gen generaciones)
```
Las dos tienen que compilar **sin ningún aviso**.

## 3. Ejecutar

```
mpirun -np <procesos> ./mpi <n> <m> <n_gen> <tam_pob> <m_rate> <NGM> <NEM> < fichero_entrada
```
Ejemplo:
```bash
mpirun -np 4 ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt
```
Tiene que salir `Processes: 4  NGM: 10  NEM: 3`, el tiempo y la solución con su `Distance`, que debe rondar los 4.01–4.02 millones.

## 4. Pruebas

Con comunicación asíncrona, los errores suelen aparecer como bloqueos de vez en cuando. Por eso, además de las pruebas normales, se repite la ejecución muchas veces.

```bash
# distintos números de procesos (3 y 7: la población no es divisible)
for np in 1 2 3 4 7 10; do echo "np=$np"; mpirun --oversubscribe -np $np ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"; done

# migración en cada generación con 1 individuo, y más procesos que núcleos
mpirun -np 5 ./mpi 1000 400 200 100 0.3 1 1 < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"
mpirun --oversubscribe -np 16 ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"

# trabajo fijo con un número de generaciones que no es múltiplo de NGM
mpirun -np 4 ./mpi_fijo 1000 400 37 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"

# bloqueos: tiene que salir 20/20
ok=0; for r in $(seq 20); do timeout 60 mpirun -np 6 ./mpi 1000 400 200 100 0.3 5 3 < ../input/input_1000_400_200_100_0.3.txt > /dev/null 2>&1 && ok=$((ok+1)); done; echo "terminaron bien: $ok/20"
```

## 5. Medir prestaciones

Se usan los mismos scripts y parámetros que en las cuestiones 2 y 3: `2000 800 400 200 0.3`, NGM = 10, NEM = 3 y 1, 2, 4, 6 y 8 procesos.

Desde `codigo/src`:
```bash
rm -f ../output/prestaciones.txt ../output/prestaciones_fijo.txt   # los scripts AÑADEN al final del fichero
./prestaciones.sh        # versión normal, 5 repeticiones
# esperar unos minutos a que se enfríe el portátil
./prestaciones_fijo.sh   # trabajo fijo (60 generaciones), 3 repeticiones
cat ../output/prestaciones.txt ../output/prestaciones_fijo.txt
```

Condiciones para que los tiempos sean fiables:
- portátil **enchufado**;
- sin otros programas abiertos;
- sin usar el ordenador mientras se ejecuta.

Hemos comprobado que, al encadenar muchas ejecuciones, el portátil se calienta y cada tanda va algo más lenta (de 3,76 s a 4,24 s en 25 ejecuciones seguidas). Por eso conviene dejarlo enfriar entre los dos scripts.

Si un valor se sale claramente de la tendencia (en nuestra medición, 4 procesos con convergencia dio 5,341 s, más que con 1 proceso), repite esa configuración varias veces para ver si fue algo puntual:
```bash
for r in 1 2 3 4 5; do mpirun -np 4 ./mpi 2000 800 400 200 0.3 10 3 < ../input/input_2000_800_400_200_0.3.txt | grep Time; done
```

Si aparece `Permission denied`, ejecuta `chmod +x *.sh`.

## 6. Actualizar la memoria

En la cuestión 4 de `MemoriaMPP.tex`, los datos están marcados con el comentario `% >>> DATO`. Búscalos con el buscador del editor. Hay que actualizar:

1. **Tabla de prestaciones de la cuestión 4**: copiar los valores de `output/prestaciones.txt` y `output/prestaciones_fijo.txt`.
2. **Tabla de comparación**: tiempos y speed-up de **trabajo fijo** de la cuestión 2 (síncrona) y de la cuestión 4 (asíncrona).
3. **Análisis**:
   - el rango de diferencias entre la cuestión 2 y la 4, y la diferencia con 1 proceso;
   - el párrafo de los 4 procesos con convergencia. Si al volver a medir ya no sale un valor anómalo, ese párrafo se puede quitar, pero conviene dejar la frase sobre el calentamiento del portátil.

Si escribes porcentajes negativos, hazlo como `$-$3.4\,\%`, nunca `$-3.4\,\%$`: en modo matemático da error con el paquete de español.

## 7. Antes de entregar

- [ ] La carpeta se llama `C4`.
- [ ] `make clean` en `codigo/src`.
- [ ] Borrar `codigo/input/`.
- [ ] En `output/` solo están los resultados de la cuestión 4.
- [ ] La memoria compila sin errores.
