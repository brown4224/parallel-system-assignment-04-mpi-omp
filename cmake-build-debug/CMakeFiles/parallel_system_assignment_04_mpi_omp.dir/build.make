# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.8

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /home/sean/Desktop/clion/bin/cmake/bin/cmake

# The command to remove a file.
RM = /home/sean/Desktop/clion/bin/cmake/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp/cmake-build-debug

# Include any dependencies generated for this target.
include CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/depend.make

# Include the progress variables for this target.
include CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/flags.make

CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o: CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/flags.make
CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o: ../main.cpp
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --progress-dir=/home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o"
	mpic++  $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -o CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o -c /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp/main.cpp

CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Preprocessing CXX source to CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.i"
	mpic++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp/main.cpp > CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.i

CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green "Compiling CXX source to assembly CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.s"
	mpic++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp/main.cpp -o CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.s

CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o.requires:

.PHONY : CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o.requires

CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o.provides: CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o.requires
	$(MAKE) -f CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/build.make CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o.provides.build
.PHONY : CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o.provides

CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o.provides.build: CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o


# Object files for target parallel_system_assignment_04_mpi_omp
parallel_system_assignment_04_mpi_omp_OBJECTS = \
"CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o"

# External object files for target parallel_system_assignment_04_mpi_omp
parallel_system_assignment_04_mpi_omp_EXTERNAL_OBJECTS =

parallel_system_assignment_04_mpi_omp: CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o
parallel_system_assignment_04_mpi_omp: CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/build.make
parallel_system_assignment_04_mpi_omp: CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color --switch=$(COLOR) --green --bold --progress-dir=/home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp/cmake-build-debug/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Linking CXX executable parallel_system_assignment_04_mpi_omp"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/build: parallel_system_assignment_04_mpi_omp

.PHONY : CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/build

CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/requires: CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/main.cpp.o.requires

.PHONY : CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/requires

CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/cmake_clean.cmake
.PHONY : CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/clean

CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/depend:
	cd /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp/cmake-build-debug && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp/cmake-build-debug /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp/cmake-build-debug /home/sean/Desktop/school/parralel-systems/parallel-system-assignment-04-mpi-omp/cmake-build-debug/CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/parallel_system_assignment_04_mpi_omp.dir/depend

