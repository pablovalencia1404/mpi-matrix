FROM nlknguyen/alpine-mpich:onbuild

COPY project/ .

RUN mpicc -O3 -o mpi_matrix mpi_matrix.c
