# Práctica IV - Multiplicación de Matrices con MPICH

Implementación de multiplicación de matrices cuadradas distribuida con MPI (MPICH) en C, ejecutada sobre un cluster Docker (master + workers).

## Qué incluye

- `project/mpi_matrix.c`: programa MPI distribuido.
- `Dockerfile`: compila la aplicación usando `nlknguyen/alpine-mpich:onbuild`.
- `docker-compose.yml`: levanta registry, nodo master y workers.
- `cluster.sh`: automatiza build, push, escalado y ejecución remota.
- `scripts/benchmark.sh`: ejecuta los escenarios de temporización pedidos en la práctica.

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
RESULT n=1024 procs=4 seconds=... gflops=... checksum=...
```

## Escenarios pedidos en la práctica

- 1 nodo: `size=1`, `-np 1 -ppn 1`
- 4 nodos: `size=4`, `-np 4 -ppn 1`
- 4 nodos con 2 procesos por nodo: `size=4`, `-np 8 -ppn 2`
- 8 nodos: `size=8`, `-np 8 -ppn 1`

Puedes ejecutar todo automáticamente:

```sh
SIZES="1024 2048" REPEATS=3 ./scripts/benchmark.sh ./results.csv
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
