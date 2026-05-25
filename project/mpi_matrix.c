#include <mpi.h>

#include <stdint.h>
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

static void fill_random_matrix(double *m, int n, unsigned int *seed) {
    int total = n * n;
    for (int i = 0; i < total; ++i) {
        m[i] = (double)next_random_1_to_10(seed);
    }
}

static void multiply_block(const double *a_local,
                           const double *b,
                           double *c_local,
                           int local_rows,
                           int n) {
    for (int i = 0; i < local_rows; ++i) {
        double *c_row = c_local + (size_t)i * n;
        const double *a_row = a_local + (size_t)i * n;

        for (int k = 0; k < n; ++k) {
            double a_ik = a_row[k];
            const double *b_row = b + (size_t)k * n;
            for (int j = 0; j < n; ++j) {
                c_row[j] += a_ik * b_row[j];
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

    double *a_global = NULL;
    double *c_global = NULL;
    if (world_rank == 0) {
        a_global = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
        c_global = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
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
        fill_random_matrix(a_global, n, &seed);
    }

    double *b = (double *)malloc((size_t)n * (size_t)n * sizeof(double));
    double *a_local = (double *)malloc(local_elems * sizeof(double));
    double *c_local = (double *)calloc(local_elems, sizeof(double));
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
        fill_random_matrix(b, n, &seed_b);
    }

    MPI_Scatterv(a_global, counts, displs, MPI_DOUBLE, a_local, (int)local_elems, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(b, n * n, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    MPI_Barrier(MPI_COMM_WORLD);
    double t0 = MPI_Wtime();

    multiply_block(a_local, b, c_local, local_rows, n);

    MPI_Barrier(MPI_COMM_WORLD);
    double t1 = MPI_Wtime();
    double local_compute_seconds = t1 - t0;

    double compute_seconds = 0.0;
    MPI_Reduce(&local_compute_seconds, &compute_seconds, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    MPI_Gatherv(c_local,
                (int)local_elems,
                MPI_DOUBLE,
                c_global,
                counts,
                displs,
                MPI_DOUBLE,
                0,
                MPI_COMM_WORLD);

    double local_checksum = 0.0;
    for (size_t i = 0; i < local_elems; ++i) {
        local_checksum += c_local[i];
    }

    double checksum = 0.0;
    MPI_Reduce(&local_checksum, &checksum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    if (world_rank == 0) {
        double gflops = (2.0 * (double)n * (double)n * (double)n) / (compute_seconds * 1e9);
        printf("RESULT variant=dynamic_linear n=%d procs=%d seconds=%.6f gflops=%.6f checksum=%.4f\n",
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
