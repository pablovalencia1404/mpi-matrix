#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>

static int parse_positive_int(const char *s) {
    char *end = NULL;
    long value = strtol(s, &end, 10);
    if (s == end || *end != '\0' || value <= 0 || value > 1000000L) {
        return -1;
    }
    return (int)value;
}

static unsigned int next_random_1_to_10(unsigned int *seed) {
    *seed = (*seed * 1103515245U) + 12345U;
    return ((*seed / 65536U) % 10U) + 1U;
}

static void build_row_distribution(int n, int world_size, int *rows, int *displs_rows) {
    int base = n / world_size;
    int rem = n % world_size;
    int offset = 0;

    for (int rank = 0; rank < world_size; ++rank) {
        rows[rank] = base + (rank < rem ? 1 : 0);
        displs_rows[rank] = offset;
        offset += rows[rank];
    }
}

static void fill_random_matrix(int n, double m[n][n], unsigned int *seed) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            m[i][j] = (double)next_random_1_to_10(seed);
        }
    }
}

static void multiply_block(int n,
                           int local_rows,
                           double a_local[local_rows][n],
                           double b[n][n],
                           double c_local[local_rows][n]) {
    for (int i = 0; i < local_rows; ++i) {
        for (int k = 0; k < n; ++k) {
            double a_ik = a_local[i][k];
            for (int j = 0; j < n; ++j) {
                c_local[i][j] += a_ik * b[k][j];
            }
        }
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int world_rank = 0;
    int world_size = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    if (argc < 2) {
        if (world_rank == 0) {
            fprintf(stderr, "Uso: %s <N_multiplo_de_8> [seed]\n", argv[0]);
        }
        MPI_Finalize();
        return 1;
    }

    int n = parse_positive_int(argv[1]);
    if (n <= 0 || n % 8 != 0) {
        if (world_rank == 0) {
            fprintf(stderr, "Error: N debe ser un entero positivo y multiplo de 8.\n");
        }
        MPI_Finalize();
        return 1;
    }

    unsigned int seed = 12345U;
    if (argc >= 3) {
        int parsed_seed = parse_positive_int(argv[2]);
        if (parsed_seed <= 0) {
            if (world_rank == 0) {
                fprintf(stderr, "Error: seed invalida.\n");
            }
            MPI_Finalize();
            return 1;
        }
        seed = (unsigned int)parsed_seed;
    }

    int *rows = (int *)malloc((size_t)world_size * sizeof(int));
    int *displs_rows = (int *)malloc((size_t)world_size * sizeof(int));
    int *counts = (int *)malloc((size_t)world_size * sizeof(int));
    int *displs = (int *)malloc((size_t)world_size * sizeof(int));
    if (!rows || !displs_rows || !counts || !displs) {
        fprintf(stderr, "Rank %d: memoria insuficiente para metadatos MPI.\n", world_rank);
        free(rows);
        free(displs_rows);
        free(counts);
        free(displs);
        MPI_Finalize();
        return 1;
    }

    build_row_distribution(n, world_size, rows, displs_rows);
    for (int i = 0; i < world_size; ++i) {
        counts[i] = rows[i] * n;
        displs[i] = displs_rows[i] * n;
    }

    int local_rows = rows[world_rank];
    size_t local_elems = (size_t)local_rows * (size_t)n;

    double(*a_global)[n] = NULL;
    double(*c_global)[n] = NULL;
    if (world_rank == 0) {
        a_global = (double(*)[n])malloc((size_t)n * sizeof(*a_global));
        c_global = (double(*)[n])malloc((size_t)n * sizeof(*c_global));
        if (!a_global || !c_global) {
            fprintf(stderr, "Rank 0: memoria insuficiente para matrices globales.\n");
            free(rows);
            free(displs_rows);
            free(counts);
            free(displs);
            free(a_global);
            free(c_global);
            MPI_Finalize();
            return 1;
        }
        fill_random_matrix(n, a_global, &seed);
    }

    double(*b)[n] = (double(*)[n])malloc((size_t)n * sizeof(*b));
    double(*a_local)[n] = (double(*)[n])malloc((size_t)local_rows * sizeof(*a_local));
    double(*c_local)[n] = (double(*)[n])calloc((size_t)local_rows, sizeof(*c_local));
    if (!b || !a_local || !c_local) {
        fprintf(stderr, "Rank %d: memoria insuficiente para matrices locales.\n", world_rank);
        free(rows);
        free(displs_rows);
        free(counts);
        free(displs);
        free(a_global);
        free(c_global);
        free(b);
        free(a_local);
        free(c_local);
        MPI_Finalize();
        return 1;
    }

    if (world_rank == 0) {
        unsigned int seed_b = seed ^ 0xA5A5A5A5U;
        fill_random_matrix(n, b, &seed_b);
    }

    MPI_Scatterv(world_rank == 0 ? &a_global[0][0] : NULL,
                 counts,
                 displs,
                 MPI_DOUBLE,
                 &a_local[0][0],
                 (int)local_elems,
                 MPI_DOUBLE,
                 0,
                 MPI_COMM_WORLD);
    MPI_Bcast(&b[0][0], n * n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    multiply_block(n, local_rows, a_local, b, c_local);

    MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();
    double local_compute_seconds = t1 - t0;

    double compute_seconds = 0.0;
    MPI_Reduce(&local_compute_seconds, &compute_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Gatherv(&c_local[0][0],
                (int)local_elems,
                MPI_DOUBLE,
                world_rank == 0 ? &c_global[0][0] : NULL,
                counts,
                displs,
                MPI_DOUBLE,
                0,
                MPI_COMM_WORLD);

    double local_checksum = 0.0;
    for (int i = 0; i < local_rows; ++i) {
        for (int j = 0; j < n; ++j) {
            local_checksum += c_local[i][j];
        }
    }

    double checksum = 0.0;
    MPI_Reduce(&local_checksum, &checksum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (world_rank == 0) {
        double gflops = (2.0 * (double)n * (double)n * (double)n) / (compute_seconds * 1e9);
        printf("RESULT variant=dynamic_2d n=%d procs=%d seconds=%.6f gflops=%.6f checksum=%.4f\n",
               n,
               world_size,
               compute_seconds,
               gflops,
               checksum);
    }

    free(rows);
    free(displs_rows);
    free(counts);
    free(displs);
    free(a_global);
    free(c_global);
    free(b);
    free(a_local);
    free(c_local);

    MPI_Finalize();
    return 0;
}
