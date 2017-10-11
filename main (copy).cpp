/**
 * Sean McGlincy
 * Parallel Systems
 * Assignment 2
 *
 * Dependencies: I am using CLION as an IDE which uses CMAKE 3.8, and GCC, C+11
 * gcc (GCC) 4.8.5 20150623 (Red Hat 4.8.5-11)
 * Running on Cento 7
 *
 *
 * Program:
 * Running the File:
 * I recommend putting these in the same folder
 * To run file execute the binary file as ./filename
 * arg 1: is the filepath
 * arg 2: is the number of intervals
 * Example: mpiCC -g -Wall -o mpi_program main.cpp -std=c++0x
 *          mpiexec -n 5  mpi_program  ./random.binary  10
 *
 *
 *
 * Description:
 * This program uses C++ and MPI libraries and must be compiled before execution.
 * The file will constantly open and close the file during program execution.  It is assumed that
 * the file may be on a sans/ nsf server and too large for memory.  It is also assumed that the file may
 * not be left open indefantly on a cluster enviroment.
 *
 * The Algorithm
 * 1: The file is read and file length is obtained.
 *
 * 2: The other nodes are sent their minimum file size and number of cycles.
 * This is the file size of the last transmition.
 * All other passes, a pre-agreed default is used.
 *
 * 3: The cluster cycles through the data.
 * The minimum is calculated.  Any remainder is give to
 * the root node to reduce transmition and complexity.
 * (Array size is default + number of processors)
 *
 * 4:  Values are reset
 *
 * 5: The cluser cycles through the data.
 * The interval is calculated and reduced.
 * */
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <cmath>
#include <assert.h>
#include <mpi.h>
#ifdef _OPENMP
#include <omp.h>
#endif


using namespace std;
using namespace chrono;

typedef struct {
    //////// MPI  INIT //////////////
    int comm_sz;  // Number of process
    int my_rank;


    //////// MPI  Variables //////////////
    const int root = 0;
    int maxMessage = 100;
    int *messageSize = NULL;
    int minMessage;
    int *local_buffer = NULL;
    int *local_data = NULL;
    int local_min;
    int local_max;
    int num_iterations;
    int intervalSize;
    bool keep_alive;
    bool alive;
    string local_file;


    //////// Root Variables //////////////
    int unit;
    string filePath;
    int bufferSize;
    int size;
    int bucketSize;
    int fileLength;
    int min;
    int max;
    int *readBuffer = NULL;
    int *data = NULL;
    int seek;
    int remainder;

} data;


void get_rank_thread_count(int *my_rank, int *thread_count) {
#ifdef _OPENMP
    *my_rank = omp_get_thread_num();
    *thread_count = omp_get_num_threads();
#else
    *my_rank = 1;
    *thread_count = 0;
#endif
}

// Input:  Takes in a pointer to the clock array and number of samples
// Output: Writes time to the array memory location
void clock(high_resolution_clock::time_point *array, int *time_samples) {
    for (int i = 0; i < *time_samples; i++) {
        array[i] = high_resolution_clock::now();
    }
}

// Input: Takes an array of time samples and number of samples
// Output: returns the average
double calculate_time(high_resolution_clock::time_point *start, high_resolution_clock::time_point *end, int *time_samples) {
    // Average time and convert to Micro Sec; 1 sec = 1,000,000 micro sec
    double total = 0;
    for (int i = 0; i < *time_samples; i++) {
        chrono::duration<double, std::milli> diff = end[i] - start[i];  // Time in Micro Sec
        total += diff.count();
    }
    return total / *time_samples;
}

// Input: Takes in the sample data and print
// Output: VOID
void print(int sample_size, int min, int max, int bucketSize, int *data, int myrank, int intervalSize) {
    cout << "Report from processor:  " << myrank << endl;
    cout << "Sample Size: " << sample_size << endl;
    cout << "Min Value: " << min << endl;
    cout << "Max Value: " << max << endl;
    cout << "bucket Range: " << bucketSize - 1 << endl;
    for (int i = 0; i < intervalSize; i++) {
        cout << "[" << min + (i * bucketSize) << ", " << min + ((i + 1) * bucketSize) - 1 << "] : " << data[i] << endl;
    }
}

// Input: Takes an array of char. to determine if they are integers
// Output: returns as integer
int check_user_number(char *argv) {
    char *endptr;
    int intervalSize = strtol(argv, &endptr, 10);
    if (!*argv || *endptr)
        cerr << "Invalid number " << argv << '\n';
    return intervalSize;
}

// Input: take an array ptr and size of array
// Output:  Zeros out the array
void init_array(int *a, int arr_size) {
    new int[arr_size];
    for (int i = 0; i < arr_size; i++)
        a[i] = 0;
}


// Input: takes a void pointer an pointer to open file
// Output:  Calculates filelength and min message
void io_init(void *ptr, ifstream *fileInput) {
    data *tdata = (data *) ptr;
    fileInput->seekg(0, ios::end);
    tdata->fileLength = fileInput->tellg();
    tdata->num_iterations = (int) ceil((double) tdata->fileLength / tdata->bufferSize);

    if (tdata->fileLength < tdata->bufferSize) {
        tdata->minMessage = (tdata->fileLength / tdata->unit) / tdata->comm_sz;
    } else {
        tdata->minMessage = ((tdata->fileLength - (tdata->num_iterations - 1) * tdata->bufferSize)) /
                            (tdata->unit * tdata->comm_sz);
        if (tdata->minMessage <= 0)
            tdata->minMessage = tdata->maxMessage;
    }
}

//// Input: takes a void pointer an pointer to open file
//// Output: Reads file to buffer
//void readFile(void *ptr, ifstream *fileInput, int* target_array) {
//    data *tdata = (data *) ptr;
//    fileInput->seekg(tdata->seek);
////    cout << "Reading File" << endl;
//
//    //  Check if buffer is less then remainder of file
//    if (tdata->fileLength - tdata->seek < tdata->size) {
//        tdata->size = tdata->fileLength - tdata->seek;
//        *tdata->messageSize = (tdata->size / tdata->unit) / tdata->comm_sz;
//        tdata->remainder = (tdata->size / tdata->unit) - (*tdata->messageSize * tdata->comm_sz);
//    }
//    fileInput->read((char *) target_array, tdata->size);
//    tdata->seek = fileInput->tellg();
//
////    cout << "done reading file" << endl;
////    for (int i= 0; i< tdata->size / tdata->unit; i++)
////        cout << *target_array << endl;
//
//}


// Input: takes a void pointer an pointer to open file
// Output: Reads file to buffer
void readFile(void *ptr, ifstream *fileInput) {
    data *tdata = (data *) ptr;
    fileInput->seekg(tdata->seek);

    //  Check if buffer is less then remainder of file
    if (tdata->fileLength - tdata->seek < tdata->size) {
        tdata->size = tdata->fileLength - tdata->seek;
        *tdata->messageSize = (tdata->size / tdata->unit) / tdata->comm_sz;
        tdata->remainder = (tdata->size / tdata->unit) - (*tdata->messageSize * tdata->comm_sz);
    }
    fileInput->read((char *) tdata->readBuffer, tdata->size);
    tdata->seek = fileInput->tellg();

}

void writeFile(void *ptr) {
    data *tdata = (data *) ptr;

    try
    {
        ofstream fileAppend;
        fileAppend.open(tdata->filePath, ios::binary);
        if(fileAppend.is_open()){
            fileAppend.seekp(0, ios::end);
            fileAppend.write((char*) tdata->local_buffer, *tdata->messageSize);

            fileAppend.close();

        } else {
            ofstream fileOut (tdata->local_file, ios::binary);
            fileOut.write((char*) tdata->local_buffer, *tdata->messageSize);
            fileOut.close();

        }
    }
    catch(std::ofstream::failure &writeErr)
    {
        cout << "Can Not open file..." << endl;
        tdata->keep_alive = false;
    }



    // All processes call and check for error
    MPI_Allreduce(&tdata->keep_alive, &tdata->alive, 1,MPI_C_BOOL, MPI_LAND, MPI_COMM_WORLD );
    if(!(tdata->alive)){
        printf("Abort process called.  Thread %d shutting down!\n", tdata->my_rank);
        MPI_Finalize();
        exit(1);
    }

}


// Input:  Takes a void pointer
// Output: Reads call back to read file or init file
void openFile(void(&f)(void *ptr, ifstream *fileInput), void *ptr) {
    data *tdata = (data *) ptr;
    if (tdata->my_rank == tdata->root) {

        ifstream fileInput;
        fileInput.open(tdata->filePath, ios::binary);
        if (fileInput.is_open()) {

            // Function CAll Back
            f(tdata, &fileInput);
            fileInput.close();
        } else {
            cout << "Can Not open file..." << endl;
            tdata->keep_alive = false;
        }
    }

    // All processes call and check for error
    MPI_Allreduce(&tdata->keep_alive, &tdata->alive, 1,MPI_C_BOOL, MPI_LAND, MPI_COMM_WORLD );
    if(!(tdata->alive)){
        printf("Abort process called.  Thread %d shutting down!\n", tdata->my_rank);
        MPI_Finalize();
        exit(1);
    }

//// Input:  Takes a void pointer
//// Output: Reads call back to read file or init file
//void openFile(void(&f)(void *ptr, ifstream *fileInput, int* target_array),  string file_path, int target_node, void *ptr,  int* target_array = NULL) {
//    data *tdata = (data *) ptr;
//    if (tdata->my_rank == target_node) {
//
//        ifstream fileInput;
//        fileInput.open(file_path, ios::binary);
//        if (fileInput.is_open()) {
//
//            // Function CAll Back
//            f(tdata, &fileInput, target_array);
//            fileInput.close();
//        } else {
//            cout << "Can Not open file..." << endl;
//            tdata->keep_alive = false;
//        }
//    }
//
//    // All processes call and check for error
//    MPI_Allreduce(&tdata->keep_alive, &tdata->alive, 1,MPI_C_BOOL, MPI_LAND, MPI_COMM_WORLD );
//    if(!(tdata->alive)){
//        printf("Abort process called.  Thread %d shutting down!\n", tdata->my_rank);
//        MPI_Finalize();
//        exit(1);
//    }

}



void append_remainder_data(void *ptr, int target_node){
    data *tdata = (data *) ptr;

    // Root gets any data not evenly split
    if (tdata->remainder > 0 && tdata->my_rank == target_node) {
        std::copy(tdata->readBuffer + ((tdata->size / tdata->unit) - tdata->remainder),
                  tdata->readBuffer + (tdata->size / tdata->unit), tdata->local_buffer + *tdata->messageSize);
        tdata->messageSize += tdata->remainder;
    }
}

// Input: 2 pointers to integer files and cluster root
// Groups values together and broadcast
void build_mpi_data_type(int *data_1, int *data_2, int root) {

    MPI_Datatype custom_type = NULL;

    MPI_Aint data_1_addr, data_2_addr;
    MPI_Get_address(data_1, &data_1_addr);
    MPI_Get_address(data_2, &data_2_addr);

    int array_of_blocklengths[2] = {1, 1};
    MPI_Datatype array_of_types[2] = {MPI_INT, MPI_INT};
    MPI_Aint array_of_displacements[2] = {0, data_2_addr - data_1_addr};
    MPI_Type_create_struct(2, array_of_blocklengths, array_of_displacements, array_of_types, &custom_type);
    MPI_Type_commit(&custom_type);

    MPI_Bcast(data_1, 1, custom_type, root, MPI_COMM_WORLD);
    MPI_Type_free(&custom_type);
}


//  Input: void pointer
//  Output: increments the interval array
void calculate_intervals(void *ptr) {
    data *tdata = (data *) ptr;
    for (int i = 0; i < *tdata->messageSize; i++) {
        tdata->local_data[(tdata->local_buffer[i] - tdata->min) / tdata->bucketSize]++;
    }
}

//  Input: void pointer
//  Output: finds local min and max
void find_min_max(void *ptr) {
    data *tdata = (data *) ptr;
    for (int i = 0; i < *tdata->messageSize; i++) {
        if (tdata->local_min > tdata->local_buffer[i])
            tdata->local_min = tdata->local_buffer[i];
        if (tdata->local_max < tdata->local_buffer[i])
            tdata->local_max = tdata->local_buffer[i];
    }
}


////  Input: void pointer
////  Output: Calculate min and max or interval size through call backs.
//void loop_read_send(void(&f)(void *ptr), void *ptr) {
//    data *tdata = (data *) ptr;
//    int last = tdata->num_iterations - 1;
//
//    // Cycle through and send data
//    for (int i = 0; i < tdata->num_iterations; i++) {
//        // The last cycle maybe shorter.  Resize message
//        if (i == last)
//            tdata->messageSize = &tdata->minMessage;
//
//
//
//
//        // Read in data and Send Data
//        openFile(tdata->local_file,  tdata->my_rank , readFile, tdata);
//        MPI_Scatter(tdata->readBuffer, *tdata->messageSize, MPI_INT, tdata->local_buffer, *tdata->messageSize, MPI_INT, tdata->root, MPI_COMM_WORLD);
//
//        // Root gets any data not evenly split
//        if (tdata->remainder > 0 && tdata->my_rank == tdata->root) {
//            std::copy(tdata->readBuffer + ((tdata->size / tdata->unit) - tdata->remainder), tdata->readBuffer + (tdata->size / tdata->unit), tdata->local_buffer + *tdata->messageSize);
//            tdata->messageSize += tdata->remainder;
//        }
//
//        // Calculate: min, max or number of partitions
//        f(tdata);
//    }
//
//}

int main(int argc, char *argv[]) {
    //////// Start Clock //////////////
    // Use Chrono for high grade clock
    int time_samples = 5;
    high_resolution_clock::time_point clock_start[time_samples];
    high_resolution_clock::time_point clock_end[time_samples];
    clock(clock_start, &time_samples);

    //////// Data Struct INIT  //////////////
    data tdata;

    //////// USER INPUT//////////////
    if (argc != 3) {
        cout << "Error Error" << endl;
        cout << "Please provide: binary data file and interval size" << endl;
        exit(1);
    }
    assert(argc == 3);
    tdata.filePath = argv[1];
    tdata.intervalSize = check_user_number(argv[2]);
    assert(tdata.intervalSize > 0);


    //////// MPI  INIT //////////////
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &tdata.comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &tdata.my_rank);


    //////// MPI  Variables //////////////
    tdata.messageSize = &tdata.maxMessage;
    tdata.minMessage = 0;
    tdata.local_buffer = new int[tdata.maxMessage + tdata.comm_sz];
    tdata.local_data = new int[tdata.intervalSize];
    tdata.local_min = numeric_limits<int>::max();
    tdata.local_max = numeric_limits<int>::min();
    tdata.keep_alive = true;
    tdata.alive = true;
    tdata.local_file = "/temp/mcglincy_mpi_binary_file_" + to_string(tdata.my_rank) + ".binary";


    //////// Root Variables //////////////
    tdata.unit = sizeof(int);
    tdata.bufferSize = tdata.unit * tdata.maxMessage * tdata.comm_sz;  // read in chuncks of file
    tdata.size = tdata.bufferSize;
    tdata.bucketSize = 0;
    tdata.fileLength = 0;
    tdata.min = tdata.local_min;
    tdata.max = tdata.local_max;
    tdata.readBuffer = new int[tdata.bufferSize];
    tdata.data = new int[tdata.intervalSize];
    tdata.seek = 0;
    tdata.remainder = 0;

    //////// INIT Arrays //////////////
    init_array(tdata.local_buffer, tdata.maxMessage + tdata.comm_sz);
    init_array(tdata.local_data, tdata.intervalSize);
    init_array(tdata.readBuffer, tdata.bufferSize);
    init_array(tdata.data, tdata.intervalSize);



        //////// OPEN FILE & INIT MSG   //////////////
    openFile(io_init, &tdata);
//    openFile( io_init, tdata.filePath, tdata.root, &tdata);
    build_mpi_data_type(&tdata.num_iterations, &tdata.minMessage, tdata.root);


    //////// Min Max   //////////////
    int last = tdata.num_iterations - 1;

    // Cycle through and send data
    for (int i = 0; i < tdata.num_iterations; i++) {
        // The last cycle maybe shorter.  Resize message
        if (i == last)
            tdata.messageSize = &tdata.minMessage;

        cout << "Opening file" << endl;
//        void openFile(void(&f)(void *ptr, ifstream *fileInput), void *ptr)
                openFile(readFile, &tdata);

//        openFile( readFile, tdata.filePath, tdata.root, &tdata, tdata.readBuffer);

        cout << "Scatter"  <<  endl;
        MPI_Scatter(tdata.readBuffer, *tdata.messageSize, MPI_INT, tdata.local_buffer, *tdata.messageSize, MPI_INT, tdata.root, MPI_COMM_WORLD);
        cout << " Remainder" << endl;
        append_remainder_data(&tdata, tdata.root);

//        if(tdata.my_rank == tdata.root){
//            for(int j = 0; j < *tdata.messageSize; j++)
//                cout << tdata.readBuffer[j] << endl;
////                cout << tdata.local_buffer[j] << endl;
//
////                cout < to_string( (int)tdata.readBuffer[0]) << endl;
//        }

        // Root gets any data not evenly split
//        if (tdata.remainder > 0 && tdata.my_rank == tdata.root) {
//            cout << "adding remainder " << tdata.my_rank << " Loop: " << i << endl;
//            std::copy(tdata.readBuffer + ((tdata.size / tdata.unit) - tdata.remainder),
//                      tdata.readBuffer + (tdata.size / tdata.unit), tdata.local_buffer + *tdata.messageSize);
//            tdata.messageSize += tdata.remainder;
//        }
        find_min_max(&tdata);

//        writeFile(&tdata);

    }


//    MPI_Allreduce(&tdata.local_min, &tdata.min, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
//    MPI_Allreduce(&tdata.local_max, &tdata.max, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

//    //////// Rest Values and Send Min and Max //////////////
//    tdata.messageSize = &tdata.maxMessage;
//    tdata.seek = 0;
//    tdata.remainder = 0;
//    tdata.size = tdata.bufferSize;


//    //////// FIND Buckets //////////////
//    int range = abs(tdata.max - tdata.min);
//    tdata.bucketSize = range / tdata.intervalSize;
//    tdata.bucketSize++;
//
//    //////// Send All Data and Reduce //////////////
//    loop_read_send(calculate_intervals, &tdata);
//    MPI_Reduce(tdata.local_data, tdata.data, tdata.intervalSize, MPI_INT, MPI_SUM, tdata.root, MPI_COMM_WORLD);


    ////////  END CLOCK //////////////
    //////// GET TIME //////////////
    if (tdata.my_rank == tdata.root) {
        print(tdata.fileLength / tdata.unit, tdata.min, tdata.max, tdata.bucketSize, tdata.data, tdata.my_rank,
              tdata.intervalSize);;
        clock(clock_end, &time_samples);
        double total_time = calculate_time(clock_start, clock_end, &time_samples);
        cout << "AVG Time: " << total_time << " Milli Seconds" << endl;
    }
    delete[] tdata.local_buffer;
    delete[] tdata.local_data;
    delete[] tdata.readBuffer;
    delete[] tdata.data;

    MPI_Finalize();
    return 0;
}