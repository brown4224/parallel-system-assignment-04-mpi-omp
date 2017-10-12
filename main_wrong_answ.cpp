/**``
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
    string* local_file;
    int* local_filesize;
//    int local_fileLength;
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
//    int remainder_root;

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
void io_init(void *ptr, ifstream *fileInput, int* file_length, int* target_array = NULL) {
    data *tdata = (data *) ptr;
    fileInput->seekg(0, ios::end);
    *file_length = fileInput->tellg();
    tdata->num_iterations = (int) ceil((double) *file_length / tdata->bufferSize);



    if (*file_length < tdata->bufferSize) {
        tdata->minMessage = (*file_length / tdata->unit) / tdata->comm_sz;
    } else {
        tdata->minMessage = ((*file_length- (tdata->num_iterations - 1) * tdata->bufferSize)) /
                            (tdata->unit * tdata->comm_sz);
        if (tdata->minMessage <= 0)
            tdata->minMessage = tdata->maxMessage;
    }
}
void delete_file(void* ptr, string filepath){
    data *tdata = (data *) ptr;

    if(std::remove(filepath.c_str())  != 0){
        printf("Thread %d: Can Not delete file...\n", tdata->my_rank);
        tdata->keep_alive = false;

    }


}
void shutdown_threads(void *ptr){
    data *tdata = (data *) ptr;
    for(int i= 0; i < tdata->num_iterations; i++)
        delete_file(&tdata,tdata->local_file[i]);
    delete[] tdata->local_buffer;
    delete[] tdata->local_data;
    delete[] tdata->readBuffer;
    delete[] tdata->data;
    delete[] tdata->local_file;
    delete[] tdata->local_filesize;
    MPI_Finalize();
}

void io_error_handling(void *ptr){
    data *tdata = (data *) ptr;
    MPI_Allreduce(&tdata->keep_alive, &tdata->alive, 1,MPI_C_BOOL, MPI_LAND, MPI_COMM_WORLD );
    if(!(tdata->alive)){
        printf("Abort process called.  Thread %d shutting down!\n", tdata->my_rank);
        shutdown_threads(tdata);
        exit(1);
    }
}


// Input: takes a void pointer an pointer to open file
// Output: Reads file to buffer
void readFile(void *ptr, ifstream *fileInput, int* file_Length, int* target_array) {
    data *tdata = (data *) ptr;
    fileInput->seekg(tdata->seek);

//    if(tdata->my_rank != tdata->root){
//        printf("Reading data: %d\n", tdata->my_rank);
//    }

    //  Check if buffer is less then remainder of file
    if (*file_Length - tdata->seek < tdata->size) {
        tdata->size = tdata->fileLength - tdata->seek;
        *tdata->messageSize = (tdata->size / tdata->unit) / tdata->comm_sz;
        tdata->remainder = (tdata->size / tdata->unit) - (*tdata->messageSize * tdata->comm_sz);
    }
    fileInput->read((char *) target_array, tdata->size);
    tdata->seek = fileInput->tellg();

}


void read_local_file(void* ptr, int counter){
    data *tdata = (data *) ptr;



    ifstream fileInput;
    fileInput.open(tdata->local_file[counter], ios::binary);
    if (fileInput.is_open()) {

//        // Function CAll Back
//        fileInput.seekg(0, ios::end);
//        *tdata->messageSize = fileInput.tellg();
//        fileInput.seekg(0);

        fileInput.seekg(0);
        fileInput.read((char *) tdata->local_buffer, tdata->local_filesize[counter]);
        fileInput.close();
    } else {
        cout << "Can Not open file..." << endl;
        tdata->keep_alive = false;
    }

    io_error_handling(tdata);
    for(int i = 0; i< tdata->local_filesize[counter]; i++){
        cout << i << " : Read: " << tdata->local_buffer[i] << endl;
    }
}





void writeFile(void *ptr, int counter) {
    data *tdata = (data *) ptr;
    for(int i = 0; i< *tdata->messageSize; i++){
        cout << i << " : Write: " << tdata->local_buffer[i] << endl;
    }

    try {
        ofstream fileOut (tdata->local_file[counter], ios::binary);
        tdata->local_filesize[counter] = *tdata->messageSize;
        fileOut.write((char*) tdata->local_buffer, *tdata->messageSize);
        fileOut.close();



//    string temp_file = tdata->local_file + "_temp";
//    std::ifstream infile (tdata->local_file, std::ifstream::binary);
//    std::ofstream outfile (temp_file,std::ofstream::binary);
//
////
////    FILE *fp;
////    fp = fopen(tdata->local_file, "a+b");
//
//
//    try{
//        if(infile.is_open()){
//            // get size of file
//            infile.seekg (0,infile.end);
//            in_size = infile.tellg();
//            infile.seekg (0);
//
//            // Copy data
//            int buffer_size = tdata->messageSize;
//            char* buffer = new char[buffer_size];
//            for(int j=0; j < in_size; j += buffer_size){
//                if(buffer_size > in_size -j)
//                    buffer_size = in_size - j;
//                // read content of infile
//                infile.read (buffer,buffer_size);
//                outfile.write (buffer,buffer_size);
//
//            }
//            infile.close();
//            delete[] buffer;
//        }
//
//        outfile.write((char*) tdata->local_buffer, *tdata->messageSize * tdata->unit);
//        tdata->local_fileLength = in_size + *tdata->messageSize * tdata->unit;
//
//        if(std::remove(tdata->local_file.c_str())  != 0){
//            printf("Thread %d: Can Not open file...", tdata->my_rank);
//            tdata->keep_alive = false;
//            io_error_handling(tdata);
//        }
//        if(rename(temp_file, tdata->local_file)  != 0){
//            printf("Thread %d:, Can't rename file: %s", tdata->my_rank, temp_file);
//            tdata->keep_alive = false;
//            io_error_handling(tdata);
//        }
//        outfile.close();
////        fstream fileOut (tdata->local_file, ios::binary);
//////        ofstream fileOut (tdata->local_file, ios::binary | ios::app);
////        fileOut.seekg(0, ios::end);
////
////        fileOut.write((char*) tdata->local_buffer, *tdata->messageSize);
////        tdata->local_fileLength = fileOut.tellg();
////        fileOut.close();
////
////
//////        ofstream fileAppend;
//////        fileAppend.open(tdata->local_file, ios::binary);
//////        if(fileAppend.is_open()){
//////            fileAppend.seekp(SEEK_END);
////////            fileAppend.seekp(0, ios::end);
//////            fileAppend.write((char*) tdata->local_buffer, *tdata->messageSize);
//////            fileAppend.close();
//////
//////        } else {
//////            ofstream fileOut (tdata->local_file, ios::binary);
//////            fileOut.write((char*) tdata->local_buffer, *tdata->messageSize);
//////            fileOut.close();
//////
//////        }
////        // todo multiply by unit
//////        tdata->local_fileLength += (*tdata->messageSize );
//////        tdata->local_fileLength += (*tdata->messageSize * tdata->unit);

    }
    catch(std::ofstream::failure &writeErr)
    {
        printf("Thread %d: Can Not open file...", tdata->my_rank);
        tdata->keep_alive = false;
    }
    // All processes call and check for error
    io_error_handling(tdata);

}



// Input:  Takes a void pointer
// Output: Reads call back to read file or init file
void openFile(void(&f)(void *ptr, ifstream *fileInput, int* file_length,  int* target_array),  string file_path, int* file_length, int target_node, void *ptr,  int* target_array = NULL) {
    data *tdata = (data *) ptr;
    if (tdata->my_rank == target_node) {



        ifstream fileInput;
        fileInput.open(file_path, ios::binary);
        if (fileInput.is_open()) {

            // Function CAll Back
            f(tdata, &fileInput, file_length, target_array);
            fileInput.close();
        } else {
            cout << "Can Not open file..." << endl;
            tdata->keep_alive = false;
        }
    }

//    printf("My Rank: %d\n", tdata->my_rank);
//    for(int j=0; j< tdata->size; j++)
//            printf("Rank: %d  Data:  %d\n", tdata->my_rank, tdata->local_buffer[j]);


    // All processes call and check for error
    io_error_handling(tdata);

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
void calculate_intervals(void *ptr, int counter) {
    data *tdata = (data *) ptr;
    for (int i = 0; i < tdata->local_filesize[counter]; i++) {
        ++tdata->local_data[(tdata->local_buffer[i] - tdata->min) / tdata->bucketSize];
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
//    tdata.local_file = "/tmp/mcglincy_mpi_binary_file_" + to_string(tdata.my_rank) + ".binary";
//    tdata.local_fileLength = 0;
    tdata.messageSize = &tdata.maxMessage;
    tdata.minMessage = 0;
    tdata.local_buffer = new int[tdata.maxMessage + tdata.comm_sz];
    tdata.local_data = new int[tdata.intervalSize];
    tdata.local_min = numeric_limits<int>::max();
    tdata.local_max = numeric_limits<int>::min();
    tdata.keep_alive = true;
    tdata.alive = true;


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
//    tdata.remainder_root = 0;

    //////// INIT Arrays //////////////
    init_array(tdata.local_buffer, tdata.maxMessage + tdata.comm_sz);
    init_array(tdata.local_data, tdata.intervalSize);
    init_array(tdata.readBuffer, tdata.bufferSize);
    init_array(tdata.data, tdata.intervalSize);









        //////// OPEN FILE & INIT MSG   //////////////
    openFile( io_init, tdata.filePath, &tdata.fileLength, tdata.root, &tdata);
    build_mpi_data_type(&tdata.num_iterations, &tdata.minMessage, tdata.root);



    //////// Min Max   //////////////
    int last = tdata.num_iterations - 1;
    tdata.local_file = new string[tdata.num_iterations];
    tdata.local_filesize = new int[tdata.num_iterations];
    for(int i=0; i< tdata.num_iterations; i++){
        tdata.local_file[i] = "/tmp/mcglincy_mpi_" + to_string(tdata.my_rank) + "_" + to_string(i) + ".binary";
        tdata.local_filesize[i] = 0;
    }

    // Cycle through and send data
    for (int i = 0; i < tdata.num_iterations; i++) {
        // The last cycle maybe shorter.  Resize message
        if (i == last)
            tdata.messageSize = &tdata.minMessage;

        openFile( readFile, tdata.filePath, &tdata.fileLength, tdata.root, &tdata, tdata.readBuffer);
        MPI_Scatter(tdata.readBuffer, *tdata.messageSize, MPI_INT, tdata.local_buffer, *tdata.messageSize, MPI_INT, tdata.root, MPI_COMM_WORLD);

        append_remainder_data(&tdata, tdata.root);
        writeFile(&tdata, i);

        find_min_max(&tdata);

//        if(tdata.my_rank == 7){
//            printf("Thread %d:  Writing pass: %d:  Message Size %d\n", tdata.my_rank, i, *tdata.messageSize);
//        }
    }


    MPI_Allreduce(&tdata.local_min, &tdata.min, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&tdata.local_max, &tdata.max, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    //////// Rest Values and Send Min and Max //////////////
    tdata.messageSize = &tdata.maxMessage;
    tdata.size = tdata.maxMessage;
    tdata.seek = 0;
    tdata.remainder = 0;
    //////// FIND Buckets //////////////
    int range = abs(tdata.max - tdata.min);
    tdata.bucketSize = range / tdata.intervalSize;
    tdata.bucketSize++;


    cout << "Done writting files";
    // Cycle through and send data
    for (int i = 0; i < tdata.num_iterations; i++) {
        // The last cycle maybe shorter.  Resize message
//        if (i == last){
//            tdata.messageSize = &tdata.minMessage;
//            if(tdata.my_rank == tdata.root)
//                tdata.size += tdata.remainder;
//        }
        read_local_file(&tdata, i);
        cout << "Local Filesize: "  << tdata.local_filesize[i] << endl;

        for(int j = 0; j< tdata.local_filesize[i]; j++){
            cout << i << " : ADD: " << tdata.local_buffer[j] << endl;
        }
        for (int j = 0; j < tdata.local_filesize[i]; j++) {
            ++tdata.local_data[(tdata.local_buffer[j] - tdata.min) / tdata.bucketSize];
        }


//        printf("My Rank: %d\n", tdata.my_rank);

//        openFile( readFile, tdata.local_file, &tdata.local_fileLength, tdata.my_rank, &tdata, tdata.local_buffer);
//        for(int j=0; j< tdata.size; j++)
//            printf("Rank: %d  Data:  %d\n", tdata.my_rank, tdata.local_buffer[j]);
//

//        calculate_intervals(&tdata, i);
//        if(tdata.my_rank == 7){
//            printf("Thread %d:  Reading pass: %d:  Message Size %d\n", tdata.my_rank, i, *tdata.messageSize);
//        }

//        find_min_max(&tdata);
//        writeFile(&tdata);
    }






    //////// Send All Data and Reduce //////////////
//    loop_read_send(calculate_intervals, &tdata);
    MPI_Reduce(tdata.local_data, tdata.data, tdata.intervalSize, MPI_INT, MPI_SUM, tdata.root, MPI_COMM_WORLD);


    ////////  END CLOCK //////////////
    //////// GET TIME //////////////
    if (tdata.my_rank == tdata.root) {
        print(tdata.fileLength / tdata.unit, tdata.min, tdata.max, tdata.bucketSize, tdata.data, tdata.my_rank,
              tdata.intervalSize);;
        clock(clock_end, &time_samples);
        double total_time = calculate_time(clock_start, clock_end, &time_samples);
        cout << "AVG Time: " << total_time << " Milli Seconds" << endl;
    }
    shutdown_threads(&tdata);
    return 0;
}