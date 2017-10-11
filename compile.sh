#!bin/bash
rm ./mpi_program -f
mpiCC -g -Wall -fopenmp  -o mpi_program main.cpp -std=c++0x 
mpiexec -n 8  mpi_program  ./random.binary  5



#scp '/home/sean/Desktop/School/parallel-systems/Assignment-02-MPI-Histogram/mpi_program' smcglin1@cloudland.kennesaw.edu:~/


