# Práctica 2 MPP – Cuestión 2 (comunicaciones síncronas)

## Estructura

```
C2/
├── C_digo/derivados_mpi.c   función del tipo derivado (ya copiada en mh.c)
├── codigo/
│   ├── src/                 main.c, mh.c, io.c, Makefile, run.sh, prestaciones.sh, prestaciones_fijo.sh
│   ├── include/             mh.h, io.h
│   ├── igen/                generador de ficheros de entrada
│   └── output/              resultados (prestaciones.txt, prestaciones_fijo.txt)
└── memoria/                 MemoriaMPP.tex, image.png, sello.png
```

Hay dos versiones del programa, que salen del mismo código:

- **`mpi`**: versión normal, con criterio de convergencia.
- **`mpi_fijo`**: versión de trabajo fijo, que se compila con `-DSIN_CONVERGENCIA`, nunca converge y siempre hace `n_gen` generaciones. Se usa para medir el speed-up comparando siempre el mismo trabajo.

## 1. Requisitos

```bash
sudo apt install openmpi-bin libopenmpi-dev hwloc
```

## 2. Ficheros de entrada

La carpeta `input/` no viene en el zip porque ocupa mucho. Hay que copiarla dentro de `codigo/`, de forma que quede `codigo/input/input_2000_800_400_200_0.3.txt`, etc.

Si no la tienes, se genera con `igen`. Hay que meter la carpeta `igen` dentro de `input` para que las rutas cuadren:
```bash
cd codigo
mkdir -p input && cp -r igen input/
cd input/igen && ./run.sh && cd ../..
```
Genera las instancias que no están comentadas en `igen/in.txt`.

## 3. Compilar

Desde `codigo/src`:
```bash
make clean
make mpi        # versión normal      -> ./mpi
make mpi_fijo   # versión trabajo fijo -> ./mpi_fijo
```
Tiene que compilar sin avisos.

## 4. Ejecutar

```
mpirun -np <procesos> ./mpi <n> <m> <n_gen> <tam_pob> <m_rate> <NGM> <NEM> < fichero_entrada
```

- **NGM**: cada cuántas generaciones hay migración.
- **NEM**: cuántos individuos migran de cada isla.
- Los valores elegidos son **NGM = 10 y NEM = 3**.

Ejemplos (desde `codigo/src`):
```bash
mpirun -np 4 ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt
mpirun -np 4 ./mpi_fijo 1000 400 20 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt
```
Tiene que salir `Processes: 4  NGM: 10  NEM: 3`, el tiempo con 3 decimales (`Execution Time`) y la solución con su fitness (`Distance`).

Para lanzar más procesos que núcleos físicos, se añade `--oversubscribe`:
```bash
mpirun --oversubscribe -np 16 ./mpi 1000 400 200 100 0.3 10 3 < ../input/input_1000_400_200_100_0.3.txt
```

Comprobación de parámetros. Esta ejecución debe pararse con `Assertion 'NEM <= tam_pob / size' failed`, porque NEM no cabe en las islas:
```bash
mpirun -np 8 ./mpi 1000 400 200 100 0.3 10 20 < ../input/input_1000_400_200_100_0.3.txt
```

## 5. Medir prestaciones (tablas de la memoria)

Desde `codigo/src`. Los scripts compilan solos.
```bash
rm -f ../output/prestaciones.txt ../output/prestaciones_fijo.txt   # los scripts AÑADEN al final del fichero
./prestaciones.sh        # versión normal: 2000 800 400 200 0.3, np = 1 2 4 6 8, 5 repeticiones
./prestaciones_fijo.sh   # trabajo fijo:   2000 800 60 200 0.3,  np = 1 2 4 6 8, 3 repeticiones
```
Cada script escribe una línea por número de procesos, con el tiempo medio, el speed-up y la eficiencia. El speed-up se calcula respecto al tiempo con 1 proceso.

Recomendaciones para que los tiempos sean fiables:
- Portátil enchufado.
- Sin otros programas abiertos.
- No usar el ordenador mientras se ejecuta.

Para las pruebas de NGM y NEM (6 procesos, todas las combinaciones):
```bash
for ngm in 5 10 20 50; do for nem in 1 3 5; do echo "NGM=$ngm NEM=$nem"; mpirun -np 6 ./mpi 1000 400 200 100 0.3 $ngm $nem < ../input/input_1000_400_200_100_0.3.txt | grep -E "Time|Distance"; done; done
```

## 6. Actualizar la memoria con los resultados

En `memoria/MemoriaMPP.tex`, todos los sitios donde hay datos de las ejecuciones están marcados con el comentario:
```
% >>> DATO
```
Busca "DATO" en el editor. Los comentarios no salen en el PDF. Hay que actualizar:
- la tabla de NGM/NEM y el párrafo que la comenta;
- los datos de la máquina, en el apartado *Resultados*;
- la tabla de prestaciones, copiando de `output/prestaciones.txt` y `output/prestaciones_fijo.txt`;
- las `coordinates` de la gráfica, que son los speed-up de la tabla;
- las cifras del apartado *Análisis* y la recomendación final.

**Imagen del sistema**: `memoria/image.png` tiene que ser el `lstopo` de la máquina donde se mide. Se genera con:
```bash
lstopo -f -p --no-legend --no-io memoria/image.png
```
Después hay que poner en el título de la figura el procesador y el número de núcleos reales.

## 7. Pendiente antes de entregar


## Problemas típicos

| Problema | Solución |
|---|---|
| `Permission denied` al lanzar un `.sh` | `chmod +x *.sh` |
| `There are not enough slots available` | Añadir `--oversubscribe` al `mpirun` |
| `No encuentro el fichero de entrada` | Falta la carpeta `codigo/input/` (paso 2) |
| `mpicc: command not found` | Instalar MPI (paso 1) |
