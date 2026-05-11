FROM nlknguyen/alpine-mpich:onbuild

COPY project/ .

RUN mpicc -O3 -std=c99 -o mpi_matrix mpi_matrix.c && \
    mpicc -O3 -std=c99 -o mpi_matrix_dynamic_linear mpi_matrix.c && \
    mpicc -O3 -std=c99 -o mpi_matrix_dynamic_2d mpi_matrix_dynamic_2d.c && \
    mpicc -O3 -std=c99 -o mpi_matrix_static_linear mpi_matrix_static_linear.c && \
    mpicc -O3 -std=c99 -o mpi_matrix_static_2d mpi_matrix_static_2d.c
