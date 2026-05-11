#include <mpi.h>

#include <stdio.h>
#include <stdlib.h>

#ifndef MAX_N
#define MAX_N 4096
#endif

#ifndef MAX_PROCS
#define MAX_PROCS 128
#endif

static double a_global[(size_t)MAX_N * MAX_N];
static double b[(size_t)MAX_N * MAX_N];
static double c_global[(size_t)MAX_N * MAX_N];
static double a_local[(size_t)MAX_N * MAX_N];
static double c_local[(size_t)MAX_N * MAX_N];

static int rows[MAX_PROCS];
static int displs_rows[MAX_PROCS];
static int counts[MAX_PROCS];
static int displs[MAX_PROCS];

static int parse_positive_int(const char *s) {
    char *end = NULL;
    long value = strtol(s, &end, 10);
    if (s == end || *end != '\0' || value <= 0 || value > 1000000L) {
        return -1;
    }
    return (int)value;
}

static void build_row_distribution(int n, int world_size) {
    int base = n / world_size;
    int rem = n % world_size;
    int offset = 0;

    for (int rank = 0; rank < world_size; ++rank) {
        rows[rank] = base + (rank < rem ? 1 : 0);
        displs_rows[rank] = offset;
        counts[rank] = rows[rank] * n;
        displs[rank] = offset * n;
        offset += rows[rank];
    }
}

static void fill_random_matrix(double *m, int n, unsigned int *seed) {
    size_t total = (size_t)n * (size_t)n;
    for (size_t i = 0; i < total; ++i) {
        m[i] = (double)((rand_r(seed) % 10) + 1);
    }
}

static void zero_matrix(double *m, int rows_count, int n) {
    size_t total = (size_t)rows_count * (size_t)n;
    for (size_t i = 0; i < total; ++i) {
        m[i] = 0.0;
    }
}

static void multiply_block(const double *local_a,
                           const double *matrix_b,
                           double *local_c,
                           int local_rows,
                           int n) {
    for (int i = 0; i < local_rows; ++i) {
        double *c_row = local_c + (size_t)i * n;
        const double *a_row = local_a + (size_t)i * n;

        for (int k = 0; k < n; ++k) {
            double a_ik = a_row[k];
            const double *b_row = matrix_b + (size_t)k * n;
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
    if (n <= 0 || n % 8 != 0 || n > MAX_N) {
        if (world_rank == 0) {
            fprintf(stderr, "Error: N debe ser multiplo de 8 y <= MAX_N (%d).\n", MAX_N);
        }
        MPI_Finalize();
        return 1;
    }
    if (world_size > MAX_PROCS) {
        if (world_rank == 0) {
            fprintf(stderr, "Error: demasiados procesos, MAX_PROCS=%d.\n", MAX_PROCS);
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

    build_row_distribution(n, world_size);

    int local_rows = rows[world_rank];
    size_t local_elems = (size_t)local_rows * (size_t)n;

    if (world_rank == 0) {
        unsigned int seed_b = seed ^ 0xA5A5A5A5U;
        fill_random_matrix(a_global, n, &seed);
        fill_random_matrix(b, n, &seed_b);
    }
    zero_matrix(c_local, local_rows, n);

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
        printf("RESULT variant=static_linear n=%d procs=%d seconds=%.6f gflops=%.6f checksum=%.4f\n",
               n,
               world_size,
               compute_seconds,
               gflops,
               checksum);
    }

    MPI_Finalize();
    return 0;
}
