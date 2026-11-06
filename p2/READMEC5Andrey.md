# Práctica 2 MPP – Cuestión 5 (comunicaciones colectivas)

## Qué cambia respecto a la cuestión 2

Esta cuestión parte de la **cuestión 2** (tipo derivado `Individuo`), igual que la 4, para comparar solo "punto a punto frente a colectivas". El algoritmo es el mismo; solo se sustituyen los bucles de envíos y recepciones por operaciones colectivas, en las que **todos** los procesos llaman a la misma función:

| Comunicación | Cuestión 2 | Cuestión 5 |
|---|---|---|
| Matriz de distancias (`main.c`) | bucle de `MPI_Ssend` | un `MPI_Bcast` |
| Mejores de cada isla → proceso 0 | `MPI_Ssend` + bucle de `MPI_Recv` con `ANY_SOURCE` | un `MPI_Gather` |
| Mejores globales y `continuar` → islas | 2 `MPI_Ssend` por isla | dos `MPI_Bcast` |
| Mejor de cada isla al final | `MPI_Ssend` + bucle de `MPI_Recv` | un `MPI_Gather` |

Ya no hay `MPI_ANY_SOURCE` ni `if/else` en la migración: `MPI_Gather` recoge de todos a la vez y deja los datos de la isla `k` en `mezcla[k*NEM]`.

Los cambios están en `codigo/src/main.c` y `codigo/src/mh.c`, marcados con `CUESTIÓN 5`. El `Makefile`, `io.c` y los scripts son los mismos que en la cuestión 2.

## Estructura

```
C5/
├── codigo/
│   ├── src/        main.c, mh.c, io.c, Makefile, run.sh, prestaciones.sh, prestaciones_fijo.sh
│   ├── include/    mh.h, io.h
│   ├── igen/       generador de ficheros de entrada
│   └── output/     prestaciones.txt, prestaciones_fijo.txt (resultados de la cuestión 5)
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
Las dos tienen que compilar sin avisos.

Para comprobar que ya no queda ninguna comunicación punto a punto:
```bash
grep -n "MPI_Ssend\|MPI_Recv\|ANY_SOURCE" main.c mh.c
```
Solo deben salir líneas del comentario de la cuestión 2 que hay encima de `aplicar_mh`, las que empiezan por ` *`.

## 3. Ejecutar

```
mpirun -np <procesos> ./mpi <n> <m> <n_gen> <tam_pob> <m_rate> <NGM> <NEM> < fichero_entrada
```
Ejemplo:
```bash
mpirun -np 4 ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt
```
Tiene que salir `Processes: 4  NGM: 10  NEM: 3`, el tiempo y la solución con su `Distance`, que debe estar alrededor de 4.01–4.02 millones.

## 4. Pruebas

```bash
# distintos números de procesos (3 y 7: la población no es divisible)
for np in 1 2 3 4 7 10; do echo "np=$np"; mpirun --oversubscribe -np $np ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"; done

# migración en cada generación con 1 individuo, y más procesos que núcleos
mpirun -np 5 ./mpi 1000 400 200 100 0.3 1 1 < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"
mpirun --oversubscribe -np 16 ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"

# trabajo fijo con un número de generaciones que no es múltiplo de NGM
mpirun -np 4 ./mpi_fijo 1000 400 37 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"
```
Todas tienen que terminar y dar un fitness parecido.

La primera ejecución después de encender el portátil puede salir mucho más lenta, porque el sistema está "en frío". En nuestra prueba, 1 proceso tardó 3,6 s en vez de unos 0,35 s. No es un error: basta con repetirla.

## 5. Medir prestaciones

Se usan los mismos scripts y parámetros que en las cuestiones 2, 3 y 4: `2000 800 400 200 0.3`, NGM = 10, NEM = 3 y 1, 2, 4, 6 y 8 procesos.

Desde `codigo/src`:
```bash
chmod +x *.sh                                                       # si da "Permiso denegado"
rm -f ../output/prestaciones.txt ../output/prestaciones_fijo.txt    # los scripts AÑADEN al final del fichero
./prestaciones.sh        # versión normal, 5 repeticiones
# esperar unos minutos a que se enfríe el portátil
./prestaciones_fijo.sh   # trabajo fijo (60 generaciones), 3 repeticiones
cat ../output/prestaciones.txt ../output/prestaciones_fijo.txt
```

Para que los tiempos sean fiables:
- el portátil **enchufado**;
- sin otros programas abiertos;
- sin usar el ordenador mientras se ejecuta.

Si algún valor se sale claramente de la tendencia, repite esa configuración varias veces para ver si fue algo puntual.

## 6. Actualizar la memoria

En la cuestión 5 de `MemoriaMPP.tex`, los datos están marcados con el comentario `% >>> DATO`. Búscalos con el buscador del editor. Hay que actualizar:

1. **Tabla de prestaciones de la cuestión 5**: copia los valores de `output/prestaciones.txt` y `output/prestaciones_fijo.txt`.
2. **Tabla de comparación**: el speed-up de **trabajo fijo** de las cuestiones 2, 3, 4 y 5 (columna de speed-up de la tabla de cada cuestión).
3. **Análisis**: revisa las diferencias que se citan con la cuestión 2 (con 1–2 procesos y con 4–8 procesos) y el rango de mejora del speed-up.

Los porcentajes negativos se escriben como `$-$3.4\,\%`, nunca `$-3.4\,\%$`, porque en modo matemático da error con el paquete de español.

## 7. Antes de entregar

- [ ] La carpeta se llama `C5`.
- [ ] Has hecho `make clean` en `codigo/src`.
- [ ] Has borrado `codigo/input/`.
- [ ] En `output/` solo están los resultados de la cuestión 5.
- [ ] La memoria compila sin errores.
