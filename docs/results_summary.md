# Resumen de resultados MPICH

CSV analizado: `results.csv`.

La tabla usa la media de las repeticiones. El speedup se calcula frente a la ejecucion con 1 nodo, 1 proceso y el mismo tamano de matriz.

| Variante | Tamano | Escenario | Reps | Tiempo medio (s) | GFLOPS | Speedup | Eficiencia | Checksum |
|---|---:|---|---:|---:|---:|---:|---:|---|
| mpi_matrix | 1024 | 1 nodo | 3 | 0.242723 | 8.872 | 1.00 | 1.00 | ok |
| mpi_matrix | 1024 | 4 nodos | 3 | 0.155728 | 13.828 | 1.56 | 0.39 | ok |
| mpi_matrix | 1024 | 4 nodos, 2 ppn | 3 | 0.172572 | 12.572 | 1.41 | 0.18 | ok |
| mpi_matrix | 1024 | 8 nodos | 3 | 0.192695 | 11.227 | 1.26 | 0.16 | ok |
| mpi_matrix | 2048 | 1 nodo | 3 | 3.784788 | 4.544 | 1.00 | 1.00 | ok |
| mpi_matrix | 2048 | 4 nodos | 3 | 1.786957 | 9.622 | 2.12 | 0.53 | ok |
| mpi_matrix | 2048 | 4 nodos, 2 ppn | 3 | 1.504700 | 11.419 | 2.52 | 0.31 | ok |
| mpi_matrix | 2048 | 8 nodos | 3 | 1.532380 | 11.226 | 2.47 | 0.31 | ok |
| mpi_matrix | 4096 | 1 nodo | 1 | 32.301954 | 4.255 | 1.00 | 1.00 | ok |
| mpi_matrix | 4096 | 4 nodos | 1 | 14.553244 | 9.444 | 2.22 | 0.55 | ok |
| mpi_matrix | 4096 | 4 nodos, 2 ppn | 1 | 12.218874 | 11.248 | 2.64 | 0.33 | ok |
| mpi_matrix | 4096 | 8 nodos | 1 | 12.355171 | 11.124 | 2.61 | 0.33 | ok |

## Graficas

- [gflops_mpi_matrix_1024](charts/gflops_mpi_matrix_1024.svg)
- [gflops_mpi_matrix_2048](charts/gflops_mpi_matrix_2048.svg)
- [gflops_mpi_matrix_4096](charts/gflops_mpi_matrix_4096.svg)

## Lectura rapida

- Si el speedup crece menos que el numero de procesos, el coste de comunicacion y sincronizacion esta limitando la mejora.
- Si 4 nodos con 2 procesos por nodo no mejora frente a 4 nodos con 1 proceso por nodo, la maquina anfitriona ya esta saturando CPU, memoria o red virtual.
- Si el checksum cambia entre escenarios para la misma variante y tamano, hay que revisar reparto, recogida o inicializacion.
