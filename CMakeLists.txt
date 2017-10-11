cmake_minimum_required(VERSION 3.8)
project(parallel_system_assignment_04_mpi_omp)

set(CMAKE_C_COMPILER mpiCC)
set(CMAKE_CXX_STANDARD 11)
find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

set(CMAKE_CXX_COMPILER mpic++)


set(SOURCE_FILES main.cpp)
add_executable(parallel_system_assignment_04_mpi_omp ${SOURCE_FILES})