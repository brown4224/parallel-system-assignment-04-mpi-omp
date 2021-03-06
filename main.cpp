/**
 * Sean McGlincy
 * Parallel Systems
 * Assignment 4
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
 * arg 3: is the number of threads
 * Example: mpiCC -g -Wall -fopenmp -o mpi_program main.cpp -std=c++0x
 *          mpiexec -n 4  mpi_program  ./random.binary  10 4
 *
 *
 *
 * Description:
 * This program uses C++, OMP and MPI libraries and must be compiled before execution.
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
 * UPDATE:  The cluster creates a distributed file structure for each cluster node.
 * The filenames are hashed to prevent collisions.  Each node stores what was sent in a small file or (shard).
 * These shards match the bit length that were sent and will fit into memory, rather then storing in one large folder.
 * The cluster then runs min & max using omp's multi threading library.  'ts_' indecates thread safe while 'local' indicates
 * a variable shared by the threads.
 *
 * 4:  Values are reset
 *
 * 5: The cluser cycles through the data.
 * The interval is calculated and reduced.
 * UPDATE:  The cluster now does not re-send data but reads the shard files.  The program then exicutes 'interval calculations'
 * using omp's threading library.
 *
 * Error handling:  a custom error handing protocal was written to shut down the cluster if any IO errors were handled by
 * any of the node.  I tried to reduce the 'mpi ALL_Reduce' as little as possible.  All cluster nodes need to call this after
 * root makes an IO call incase an error ocured.  However, when all nodes are operating independently, this can be called
 * after the end of the loop.  Calling at the end of the loop reduces the number of mpi transmitions because the io error function
 * is written to block the cluster node that has errored out.
 * */
#include <dirent.h>
#include <errno.h>
#include <sstream>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <cmath>
#include <assert.h>
#include <sys/stat.h>
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
    int root;
    string* local_file;
    int maxMessage;
    int *messageSize;
    int minMessage;
    int *local_buffer;
    int *local_data;
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
    int *readBuffer;
    int *data;
    int seek;
    int remainder;

} data;

// IN: takes pointer to the rank and thread count
// OUt:  produces the number of threads being used and the current threads rank
//       If OPENMP is not installed.  One thread is used
void get_rank_thread_count(int *ts_rank, int *ts_count) {

#ifdef _OPENMP
    *ts_rank = omp_get_thread_num();
    *ts_count = omp_get_num_threads();
#else
    *ts_rank= 0;
    *ts_count = 1;
#endif
}

// Input:  Takes in a pointer to the clock array and number of samples
// Output: Writes time to the array memory location
void clock(high_resolution_clock::time_point *array, int *time_samples) {
    for (int i = 0; i < *time_samples; i++) {
        array[i] = high_resolution_clock::now();
    }
}
// Input: Takes a pointer and full file path
//  Deletes the file.  If it can't cluster node is marked for shutdown
void delete_file(void* ptr, string filepath){
    data *tdata = (data *) ptr;

    if(filepath == ""){
        printf("Thread %d: Filename not initialized, can't delete file\n", tdata->my_rank);
        cout << "File: " << filepath << endl;
        tdata->keep_alive = false;
        return;
    }

    if(std::remove(filepath.c_str())  != 0){
        printf("Thread %d: Can Not delete file...\n", tdata->my_rank);
        cout << "File: " << filepath << endl;
        tdata->keep_alive = false;
    }
}

// Input:  takes a void pointer to data structure
//  Clears all files, then all arrays on the heap, then starts shutdown with mpi finalize
void shutdown_threads(void *ptr){
    data *tdata = (data *) ptr;
    for(int i= 0; i < tdata->num_iterations; i++)
        delete_file(&tdata, tdata->local_file[i]);
    delete[] tdata->local_buffer;
    delete[] tdata->local_data;
    delete[] tdata->readBuffer;
    delete[] tdata->data;
    delete[] tdata->local_file;
    MPI_Finalize();
}
// Input:  takes a void pointer
// Checks to see if a any threads have had errors.  If error exist, shut down cluster and clear files and memory
void io_error_handling(void *ptr){
    data *tdata = (data *) ptr;
    MPI_Allreduce(&tdata->keep_alive, &tdata->alive, 1,MPI_C_BOOL, MPI_LAND, MPI_COMM_WORLD );
    if(!(tdata->alive)){
        printf("Abort process called.  Thread %d shutting down!\n", tdata->my_rank);
        shutdown_threads(tdata);
        exit(1);
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

// Input:  takes a number
// Returns the number as a string
template <typename T>
std::string NumberToString ( T Number )
{
    // From: https://stackoverflow.com/questions/5590381/easiest-way-to-convert-int-to-string-in-c
    std::ostringstream ss;
    ss << Number;
    return ss.str();
}
/**
 * Creates a folder in /tmp for our distributed data structure.  If something went wrong and files exist from
 * last time, clear only our files.   Otherwise, create a folder and set permissions.  Use chmod 777 for dev.
 *
 * Creates an array of files paths that are hashed to each cluster node to prevent possible collisions if more
 * then one node is using shared storage.  The array is used later by 'write' and 'read local' functions.
 */
// Input:  takes a pointer, number of files to create and node rank
// Returns an array of file paths to be used locally during the program
string* create_file_structure(void* ptr, int num_files, int rank){
    data *tdata = (data *) ptr;

    string root_dir = "/tmp/mcglincy_mpi/";  // ONLY USE TMP FILE
    string root_file_name = "mcglincy_mpi_";

    char isFile = 0x8;
    struct dirent *DirEntry;
    const char* path = root_dir.c_str();
    DIR* dir = opendir(path);
    if(dir){
        // If directory exist, clear old files
        while((DirEntry = readdir(dir))){
            if(DirEntry->d_type == isFile){
                string file = DirEntry->d_name;
                if(file.substr(0, root_file_name.length()).compare(root_file_name))  // compare, only delete my files
                    delete_file(tdata, root_dir + DirEntry->d_name );
            }
        }
        closedir(dir);
    } else if(ENOENT == errno){
        // Create folder:  Permissions 777
//        mkdir(root_dir.c_str(),  S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
        mkdir(root_dir.c_str(),  0777);
    } else{
        cout << "Can Not create file structure..." << endl;
        tdata->keep_alive = false;
        io_error_handling(tdata);

    }
    io_error_handling(tdata);

    string* file_paths = new string[num_files];
    for(int i=0; i< num_files; i++){
        file_paths[i] = root_dir + root_file_name + NumberToString(rank) + "_" + NumberToString(i) + ".binary";
    }
    return file_paths;
}


// Input: takes a void pointer an pointer to open file
// Output:  Calculates filelength and min message
void io_init(void *ptr) {
    data *tdata = (data *) ptr;
    if (tdata->my_rank == tdata->root) {

        ifstream fileInput;
        fileInput.open(tdata->filePath, ios::binary);
        if (fileInput.is_open()) {

            fileInput.seekg(0, ios::end);
            tdata->fileLength = fileInput.tellg();
            tdata->num_iterations = (int) ceil((double) tdata->fileLength / tdata->bufferSize);

            if (tdata->fileLength < tdata->bufferSize) {
                tdata->minMessage = (tdata->fileLength / tdata->unit) / tdata->comm_sz;
            } else {
                tdata->minMessage = ((tdata->fileLength - (tdata->num_iterations - 1) * tdata->bufferSize)) / (tdata->unit * tdata->comm_sz);
                if (tdata->minMessage <= 0)
                    tdata->minMessage = tdata->maxMessage;
            }
            fileInput.close();
        } else {
            cout << "Can Not open file..." << endl;
            tdata->keep_alive = false;
        }
    }
    io_error_handling(tdata);
}

// Input: takes a void pointer an pointer to open file
// Output: Reads file to buffer
void read_master_file(void *ptr, string filePath) {

    data *tdata = (data *) ptr;
    if (tdata->my_rank == tdata->root) {

        ifstream fileInput;
        fileInput.open(filePath, ios::binary);
        if (fileInput.is_open()) {
            fileInput.seekg(tdata->seek);

            //  Check if buffer is less then remainder of file
            if (tdata->fileLength - tdata->seek < tdata->size) {
                tdata->size = tdata->fileLength - tdata->seek;
                *tdata->messageSize = (tdata->size / tdata->unit) / tdata->comm_sz;
                tdata->remainder = (tdata->size / tdata->unit) - (*tdata->messageSize * tdata->comm_sz);
            }
            fileInput.read((char *) tdata->readBuffer, tdata->size);
            tdata->seek = fileInput.tellg();

            fileInput.close();
        } else {
            cout << "Can Not open file..." << endl;
            tdata->keep_alive = false;
            io_error_handling(tdata);
        }
    }
    io_error_handling(tdata);  // All check if root failed

}

// Input:  takes a pointer and the counter for file path that will be accessed.
//  Gets the length of the file and reads the file into memory.  Because we are working with
// many small files, we know that this will fit into memory.
// io_error_handling moved to end of loop of main program
void read_local_file(void* ptr, int counter){
    data *tdata = (data *) ptr;

    ifstream fileInput;
    fileInput.open(tdata->local_file[counter], ios::binary);
    if (fileInput.is_open()) {

        fileInput.seekg(0, ios::end);
        long fileLength = fileInput.tellg();
        *tdata->messageSize = fileLength / tdata->unit;
        fileInput.seekg(0);
        fileInput.read((char *) tdata->local_buffer, fileLength);
        fileInput.close();
    } else {
        cout << "Can Not open file..." << endl;
        tdata->keep_alive = false;
        io_error_handling(tdata);
    }

//    io_error_handling(tdata);
}

// Input:  Void pointer, Counter to position in file array.  What file to read in.
// Writes the file that was sent via MPI to memory.  Uses a hashed filename.
// Store many small file instead of one big file
// Set file permisions to 777 for Dev
void writeFile(void* ptr, int counter) {
    data *tdata = (data *) ptr;
    if(tdata->local_file[counter] == "" || tdata->local_buffer == NULL){
        printf("Thread %d: Filename not initialized\n", tdata->my_rank);
        tdata->keep_alive = false;
        io_error_handling(tdata);
    }

    try {
        ofstream fileOut (tdata->local_file[counter], ios::binary);
        fileOut.write((char*) tdata->local_buffer, *tdata->messageSize * tdata->unit);
        fileOut.close();
//        chmod(  tdata->local_file[counter].c_str(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH);
        chmod(  tdata->local_file[counter].c_str(), 0777);

    }
    catch(std::ofstream::failure &writeErr)
    {
        printf("Thread %d: Can Not write file...", tdata->my_rank);
        tdata->keep_alive = false;
        io_error_handling(tdata);
    }
//    io_error_handling(tdata);

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
void calculate_intervals(void *ptr, int numThreads) {
    data *tdata = (data *) ptr;


    int j;
# pragma omp parallel num_threads(numThreads) default(none) private(j)  shared(tdata)
    {
        // Process Data
        int ts_rank;
        int ts_count;
        vector<int> ts_data(tdata->intervalSize, 0);
        get_rank_thread_count(&ts_rank, &ts_count);

# pragma omp for schedule(static, ( *tdata->messageSize ) / ts_count )
        for (j = 0; j < *tdata->messageSize; j++) {
            ts_data[  (tdata->local_buffer[j] - tdata->min) / tdata->bucketSize]++;
        }

        for (j = 0; j < tdata->intervalSize; j++) {
# pragma omp atomic
            tdata->local_data[j] += ts_data[j];
        }

    }
}

//  Input: void pointer
//  Output: finds local min and max
void find_min_max(void *ptr, int numThreads) {
    data *tdata = (data *) ptr;

    // ts_ denotes Thread Safe
    int j;
# pragma omp parallel num_threads(numThreads) default(none) private(j) shared(tdata)
    {
        int ts_rank;
        int ts_count;
        int ts_min = numeric_limits<int>::max();
        int ts_max = numeric_limits<int>::min();
        get_rank_thread_count(&ts_rank, &ts_count);

// Find max and min
# pragma omp for  schedule(static, *tdata->messageSize / ts_count )
        for (j = 0; j < *tdata->messageSize; ++j) {
            if (ts_min > tdata->local_buffer[j])
                ts_min = tdata->local_buffer[j];
            if (ts_max < tdata->local_buffer[j])
                ts_max = tdata->local_buffer[j];
        }
        // One Critical section is faster then two named
# pragma omp critical
        {
            if (tdata->local_min > ts_min)
                tdata->local_min = ts_min;

            if (tdata->local_max < ts_max)
                tdata->local_max = ts_max;
        }
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
    tdata.messageSize = NULL;
    tdata.local_buffer = NULL;
    tdata.local_data = NULL;
    tdata.readBuffer = NULL;
    tdata.data = NULL;
    tdata.local_file= NULL;

    //////// USER INPUT//////////////
    if (argc != 4) {
        cout << "Error Error" << endl;
        cout << "Please provide: binary data file and interval size" << endl;
        exit(1);
    }

    tdata.filePath = argv[1]; //Filename

    tdata.intervalSize =  check_user_number(argv[2]);  // Interval Size
    assert(tdata.intervalSize > 0 );

    int numThreads =  check_user_number(argv[3]);  // NumThreads
    assert(numThreads > 0 );


    //////// MPI  INIT //////////////
    MPI_Init(NULL, NULL);
    MPI_Comm_size(MPI_COMM_WORLD, &tdata.comm_sz);
    MPI_Comm_rank(MPI_COMM_WORLD, &tdata.my_rank);



    //////// MPI  Variables //////////////
    tdata.root = 0;
    tdata.maxMessage = 10000;
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

    //////// INIT Arrays //////////////
    init_array(tdata.local_buffer, tdata.maxMessage + tdata.comm_sz);
    init_array(tdata.local_data, tdata.intervalSize);
    init_array(tdata.readBuffer, tdata.bufferSize);
    init_array(tdata.data, tdata.intervalSize);

    //////// OPEN FILE //////////////
    omp_set_dynamic(0);  // Turn off dynamic threads
    io_init(&tdata);


    //////// Send INIT MESG, Then Send all data and Reduce //////////////
    build_mpi_data_type(&tdata.num_iterations, &tdata.minMessage, tdata.root);
    tdata.local_file = create_file_structure(&tdata, tdata.num_iterations, tdata.my_rank); // return an array of filenames

    // Cycle through and send data
    int last = tdata.num_iterations - 1;
    for (int i = 0; i < tdata.num_iterations; i++) {
        if (i == last)        // The last cycle maybe shorter.  Resize message
            tdata.messageSize = &tdata.minMessage;


        // Read in data and Send Data
        read_master_file(&tdata, tdata.filePath);
        MPI_Scatter(tdata.readBuffer, *tdata.messageSize, MPI_INT, tdata.local_buffer, *tdata.messageSize, MPI_INT, tdata.root, MPI_COMM_WORLD);

        // Root gets any data not evenly split
        if (tdata.remainder > 0 && tdata.my_rank == tdata.root) {
            std::copy(tdata.readBuffer + ((tdata.size / tdata.unit) - tdata.remainder), tdata.readBuffer + (tdata.size / tdata.unit), tdata.local_buffer + *tdata.messageSize);
            tdata.messageSize += tdata.remainder;
        }
        writeFile(&tdata, i);
        find_min_max(&tdata, numThreads);
    }

    io_error_handling(&tdata);  // All check for errors
    MPI_Allreduce(&tdata.local_min, &tdata.min, 1, MPI_INT, MPI_MIN, MPI_COMM_WORLD);
    MPI_Allreduce(&tdata.local_max, &tdata.max, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    //////// FIND Buckets  & re-setvariables//////////////
    int range = abs(tdata.max - tdata.min);
    tdata.bucketSize = range / tdata.intervalSize;
    tdata.bucketSize++;


    //////// Send All Data and Reduce //////////////
    for(int j = 0; j< tdata.num_iterations; j++){
        read_local_file(&tdata, j);
        calculate_intervals(&tdata, numThreads);
    }

    io_error_handling(&tdata); // All check for errors
    MPI_Reduce(tdata.local_data, tdata.data, tdata.intervalSize, MPI_INT, MPI_SUM, tdata.root, MPI_COMM_WORLD);


    ////////  END CLOCK //////////////
    //////// GET TIME //////////////
    if (tdata.my_rank == tdata.root) {
        print(tdata.fileLength / tdata.unit, tdata.min, tdata.max, tdata.bucketSize, tdata.data, tdata.my_rank, tdata.intervalSize);
        clock(clock_end, &time_samples);
        double total_time = calculate_time(clock_start, clock_end, &time_samples);
        cout << "AVG Time: " << total_time << " Milli Seconds" << endl;
    }
    shutdown_threads(&tdata);
    return 0;
}