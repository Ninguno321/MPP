# Práctica 2 MPP – Cuestión 3 (envío con MPI_Pack / MPI_Unpack)

## Qué cambia respecto a la cuestión 2

Solo cambia **cómo se envían los individuos**. El algoritmo, las comunicaciones síncronas (`MPI_Ssend` / `MPI_Recv`) y el uso de `MPI_ANY_SOURCE` son los mismos.

- En lugar del tipo derivado, cada individuo (sus `m` genes y su fitness) se empaqueta con `MPI_Pack` en un buffer de bytes. El buffer se envía en un único mensaje `MPI_PACKED` y se desempaqueta con `MPI_Unpack` en el mismo orden.
- El tamaño del buffer se calcula con `MPI_Pack_size`.
- El proceso 0 envía los mejores globales y la variable `continuar` en **un solo paquete**. En la cuestión 2 eran dos mensajes por isla.
- Se han eliminado `crear_tipo_datos` y el buffer `emigrantes`. Cada isla empaqueta directamente desde su población.

Todos los cambios están en `codigo/src/mh.c` y van marcados con `CUESTIÓN 3`. `main.c`, el `Makefile` y los scripts son los mismos que en la cuestión 2.

## Estructura

```
C3/
├── codigo/
│   ├── src/        main.c, mh.c, io.c, Makefile, run.sh, prestaciones.sh, prestaciones_fijo.sh
│   ├── include/    mh.h, io.h
│   ├── igen/       generador de ficheros de entrada
│   └── output/     prestaciones.txt, prestaciones_fijo.txt (resultados de la cuestión 3)
└── memoria/
```

## 1. Ficheros de entrada

Copia la carpeta `input/` (la misma de la cuestión 2) dentro de `codigo/`. Tiene que quedar así: `codigo/input/input_1000_400_200_100_0.3.txt` y `codigo/input/input_2000_800_400_200_0.3.txt`.

## 2. Compilar

Desde `codigo/src`:
```bash
make clean
make mpi        # versión normal (con convergencia)
make mpi_fijo   # versión de trabajo fijo (siempre hace n_gen generaciones)
```
Las dos versiones tienen que compilar sin avisos.

## 3. Ejecutar

```
mpirun -np <procesos> ./mpi <n> <m> <n_gen> <tam_pob> <m_rate> <NGM> <NEM> < fichero_entrada
```
Ejemplo:
```bash
mpirun -np 4 ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt
```
Tiene que salir `Processes: 4  NGM: 10  NEM: 3`, el tiempo y la solución con su `Distance`, que debe estar alrededor de 4.01–4.02 millones, como en la cuestión 2.

## 4. Pruebas

```bash
# distintos números de procesos (3 y 7 prueban que la población no sea divisible)
for np in 1 2 3 4 7 10; do echo "np=$np"; mpirun --oversubscribe -np $np ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"; done

# versión de trabajo fijo
mpirun -np 4 ./mpi_fijo 1000 400 20 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt

# más procesos que núcleos (islas de solo 6 individuos)
mpirun --oversubscribe -np 16 ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"
```
Todas tienen que terminar y dar un fitness parecido.

## 5. Medir prestaciones

Se usan los mismos scripts y parámetros que en la cuestión 2, para poder comparar: `2000 800 400 200 0.3`, NGM = 10, NEM = 3 y 1, 2, 4, 6 y 8 procesos.

Desde `codigo/src`:
```bash
rm -f ../output/prestaciones.txt ../output/prestaciones_fijo.txt   # los scripts AÑADEN al final del fichero
./prestaciones.sh        # versión normal, 5 repeticiones
./prestaciones_fijo.sh   # trabajo fijo (60 generaciones), 3 repeticiones
cat ../output/prestaciones.txt ../output/prestaciones_fijo.txt
```
Tardan unos 5 minutos. Para que los tiempos sean fiables y comparables con los de la cuestión 2:
- el portátil, enchufado;
- sin otros programas abiertos;
- sin usar el ordenador mientras se ejecuta.

Si aparece `Permission denied`, ejecuta `chmod +x *.sh`.

## 6. Actualizar la memoria

En la cuestión 3 de `MemoriaMPP.tex`, los datos están marcados con el comentario `% >>> DATO`. Búscalos con el buscador del editor. Hay que actualizar:

1. **Tabla de prestaciones de la cuestión 3**: copiar los valores de `output/prestaciones.txt` y `output/prestaciones_fijo.txt`.
2. **Tabla de comparación**:
   - columna *Tipo derivado*: los tiempos de la tabla de la cuestión 2;
   - columna *MPI_Pack*: los tiempos nuevos;
   - columna *Dif.*: (C3 − C2) / C2 en %. Si es negativa, se escribe `$-$3.4\,\%`, y nunca `$-3.4\,\%$`, porque en modo matemático da error con el paquete de español.
3. **Análisis**: revisar el rango de diferencias que se cita y el de la fila de 1 proceso.

## 7. Antes de entregar

- [ ] La carpeta se llama `C3`, no `C2`.
- [ ] `make clean` en `codigo/src`.
- [ ] Borrar `codigo/input/`.
- [ ] En `output/` solo deben estar los resultados de la cuestión 3.
- [ ] La memoria compila sin errores. Si da `Invalid UTF-8 byte`, falta la opción `literate` en `\lstdefinestyle{EstiloC}`. Si da `sello.png not found`, copia las imágenes a la carpeta de la memoria.
