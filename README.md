# Práctica MPICH - Multiplicación de Matrices

Implementación de multiplicación de matrices cuadradas distribuida con MPI (MPICH) en C, ejecutada sobre un cluster Docker (master + workers).

## Qué incluye

- `project/mpi_matrix.c`: programa MPI distribuido.
- `Dockerfile`: compila la aplicación usando `nlknguyen/alpine-mpich:onbuild`.
- `docker-compose.yml`: levanta registry, nodo master y workers.
- `cluster.sh`: automatiza build, push, escalado y ejecución remota.
- `scripts/benchmark.sh`: ejecuta los escenarios de temporización pedidos en la práctica.
- `scripts/analyze_results.py`: genera resumen y gráficas SVG a partir del CSV.
- `docs/defensa.md`: guion de apoyo para la exposición oral.

## Requisitos

- Docker disponible como `docker` o con ruta explícita en `DOCKER_BIN`.
- `ssh-keygen` en el host.

En WSL con Docker Desktop, si `docker` no está en PATH:

```sh
export DOCKER_BIN="/mnt/c/Program Files/Docker/Docker/resources/bin/docker.exe"
```

## Levantar cluster

```sh
cd mpi-matrix
chmod +x cluster.sh scripts/benchmark.sh
./cluster.sh up size=4
```

## Ejecutar multiplicación (ejemplo)

```sh
./cluster.sh exec "mpirun -np 4 -ppn 1 ./mpi_matrix 1024"
```

Salida esperada:

```txt
RESULT variant=dynamic_linear n=1024 procs=4 seconds=... gflops=... checksum=...
```

## Versiones de memoria y representación

El `Dockerfile` compila cuatro combinaciones para poder compararlas:

- `mpi_matrix` / `mpi_matrix_dynamic_linear`: memoria dinámica + matrices lineales.
- `mpi_matrix_dynamic_2d`: memoria dinámica + indexación 2D.
- `mpi_matrix_static_linear`: memoria estática + matrices lineales.
- `mpi_matrix_static_2d`: memoria estática + matrices 2D.

Ejemplo:

```sh
./cluster.sh exec "mpirun -np 4 -ppn 1 ./mpi_matrix_static_2d 1024"
```

Las versiones estáticas usan `MAX_N=4096` por defecto, definido en compilación. Si se quiere
otro límite:

```sh
mpicc -O3 -std=c99 -DMAX_N=2048 -o mpi_matrix_static_2d mpi_matrix_static_2d.c
```

## Escenarios pedidos en la práctica

- 1 nodo: `size=1`, `-np 1 -ppn 1`
- 4 nodos: `size=4`, `-np 4 -ppn 1`
- 4 nodos con 2 procesos por nodo: `size=4`, `-np 8 -ppn 2`
- 8 nodos: `size=8`, `-np 8 -ppn 1`

Puedes ejecutar todo automáticamente:

```sh
SIZES="1024 2048 4096" REPEATS=3 ./scripts/benchmark.sh ./results.csv
python3 scripts/analyze_results.py ./results.csv
```

Para comparar todas las variantes:

```sh
VARIANTS="mpi_matrix mpi_matrix_dynamic_2d mpi_matrix_static_linear mpi_matrix_static_2d" \
SIZES="1024 2048" REPEATS=3 ./scripts/benchmark.sh ./results_variants.csv
python3 scripts/analyze_results.py ./results_variants.csv --out docs/results_variants_summary.md --charts-dir docs/charts_variants
```

## Apagar cluster

```sh
./cluster.sh down
```

## Notas de implementación

- Las matrices se inicializan con enteros aleatorios en `[1,10]`.
- Se distribuyen filas de `A` con `MPI_Scatterv`.
- `B` se replica en todos los procesos con `MPI_Bcast`.
- Cada proceso calcula su bloque de `C`.
- Se reúne `C` con `MPI_Gatherv`.
- El tiempo reportado (`seconds`) mide solo el bloque de cómputo de multiplicación, sincronizado con barreras MPI.
