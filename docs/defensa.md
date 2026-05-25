# Guion de defensa

## 1. Presentacion

Esta practica implementa multiplicacion de matrices cuadradas con MPICH en C. El objetivo es repartir el calculo entre varios procesos MPI, comparar tiempos en distintos escenarios de ejecucion y razonar cuando la distribucion mejora o deja de compensar.

Las matrices usadas son de tamano multiplo de 8. Se inicializan con valores pseudoaleatorios entre 1 y 10. La temporizacion se limita al nucleo de multiplicacion, dejando fuera la inicializacion, el reparto inicial y la impresion de resultados.

## 2. Metodologia

Materiales utilizados:

- C y MPICH para el programa distribuido.
- Docker Compose para simular un cluster con nodo maestro y nodos worker.
- `mpicc`, `mpirun`, `MPI_Scatterv`, `MPI_Bcast`, `MPI_Gatherv`, `MPI_Reduce`, `MPI_Barrier` y `MPI_Wtime`.
- `scripts/benchmark.sh` para ejecutar los escenarios de prueba de forma repetible.
- `scripts/analyze_results.py` para obtener resumen, speedup, eficiencia y graficas SVG.

Organizacion del codigo:

- `project/mpi_matrix.c`: version principal, memoria dinamica y representacion lineal.
- `project/mpi_matrix_dynamic_2d.c`: memoria dinamica con indexacion 2D.
- `project/mpi_matrix_static_linear.c`: memoria estatica y representacion lineal.
- `project/mpi_matrix_static_2d.c`: memoria estatica con indexacion 2D.
- `cluster.sh`: automatiza construccion, despliegue, escalado y ejecucion dentro del master.
- `scripts/benchmark.sh`: lanza los escenarios pedidos por el enunciado.

## 3. Algoritmo implementado

El nodo maestro inicializa las matrices `A` y `B`. La matriz `A` se divide por bloques de filas; cada proceso recibe un subconjunto usando `MPI_Scatterv`. La matriz `B` se replica completa en todos los procesos con `MPI_Bcast`, porque cada bloque de filas de `A` necesita consultar todas las columnas de `B`.

Cada proceso calcula su bloque local de `C`. La medicion empieza justo antes del bucle triple de multiplicacion y termina justo despues. Se usa `MPI_Barrier` para sincronizar a los procesos antes y despues del tramo medido. El tiempo final es el maximo de los tiempos locales, obtenido con `MPI_Reduce`, porque la ejecucion global termina cuando acaba el proceso mas lento.

Finalmente, los bloques de `C` se reunen en el maestro con `MPI_Gatherv`. Tambien se calcula un checksum global con `MPI_Reduce` para comprobar que todos los escenarios producen el mismo resultado.

## 4. Funciones MPI que conviene explicar

- `MPI_Init` y `MPI_Finalize`: abren y cierran el entorno MPI.
- `MPI_Comm_rank`: identifica el proceso actual.
- `MPI_Comm_size`: obtiene el numero total de procesos.
- `MPI_Scatterv`: reparte filas de `A`, permitiendo bloques de tamano distinto si `N` no divide exactamente entre procesos.
- `MPI_Bcast`: envia la matriz `B` completa a todos los procesos.
- `MPI_Barrier`: sincroniza antes y despues del computo medido.
- `MPI_Wtime`: mide el tiempo de computo.
- `MPI_Reduce`: obtiene el maximo de tiempos y el checksum global.
- `MPI_Gatherv`: recoge los bloques locales de `C`.

## 5. Pruebas planteadas

Escenarios pedidos:

- 1 nodo: `size=1`, `-np 1`, `-ppn 1`.
- 4 nodos: `size=4`, `-np 4`, `-ppn 1`.
- 4 nodos con dos procesos por nodo: `size=4`, `-np 8`, `-ppn 2`.
- 8 nodos: `size=8`, `-np 8`, `-ppn 1`.

Tamanos recomendados:

- `1024x1024`
- `2048x2048`
- `4096x4096`
- `8192x8192`, solo si la maquina tiene memoria y tiempo suficientes.

Comandos:

```sh
SIZES="1024 2048 4096" REPEATS=3 ./scripts/benchmark.sh ./results.csv
python3 scripts/analyze_results.py ./results.csv
```

Para comparar variantes:

```sh
VARIANTS="mpi_matrix mpi_matrix_dynamic_2d mpi_matrix_static_linear mpi_matrix_static_2d" SIZES="1024 2048" REPEATS=3 ./scripts/benchmark.sh ./results_variants.csv
python3 scripts/analyze_results.py ./results_variants.csv --out docs/results_variants_summary.md --charts-dir docs/charts_variants
```

## 6. Resultados

El resumen generado queda en `docs/results_summary.md`. Las graficas SVG quedan en `docs/charts/`.

Aspectos que hay que comentar durante la defensa:

- Comparar el tiempo de 1 nodo frente a 4 y 8 nodos.
- Mirar si el speedup se acerca al numero de procesos.
- Comparar 4 nodos con 1 proceso por nodo frente a 4 nodos con 2 procesos por nodo.
- Confirmar que el checksum se mantiene constante para cada tamano.

## 7. Conclusiones esperadas

La multiplicacion de matrices tiene suficiente carga computacional para beneficiarse de distribuir filas entre procesos. Aun asi, la mejora no es perfectamente lineal por el coste de comunicacion, la sincronizacion y la replicacion de la matriz `B`.

En tamanos pequenos, el coste fijo de MPI pesa mas y la mejora puede ser limitada. En tamanos mayores, el computo domina mas y suele verse mejor escalado, siempre que la maquina anfitriona no sature CPU o memoria.

El escenario de 4 nodos con 2 procesos por nodo puede no mejorar frente a 4 nodos con 1 proceso por nodo si los contenedores compiten por los mismos nucleos fisicos o por ancho de banda de memoria.

## 8. Futuras mejoras

- Dividir tambien la matriz `B` por bloques para reducir memoria replicada.
- Usar una multiplicacion por bloques para mejorar localidad de cache.
- Medir por separado comunicacion y computo.
- Probar en un cluster fisico para comparar contra Docker.
- Ejecutar Valgrind o sanitizers sobre tamanos pequenos para revisar memoria.
