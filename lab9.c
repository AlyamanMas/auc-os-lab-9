#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define FLUSH()                                                                \
  fflush(mmap_compute_file);                                                   \
  fflush(sequential_compute_file);                                             \
  fflush(threads_compute_file);                                                \
  fflush(parallel_compute_file);

#define ul unsigned long long int
#define ui unsigned int

pthread_mutex_t sum_mutex;
ui *arr;   // will hold the numbers from the file
ui arr_sz; // will hold the size of arr
ul result; // will hold the accumulative sum of the results of the function
           // pointer applied to the elements of the array

// add function that takes two integers and returns an unsigned long integer
// that is the sum of the two integers
ul add(int a, int b) { return (ul)a + (ul)b; }

// sequential_compute function that takes a start and end of a range of the
// array and applies the add function
//  to the elements of the array in the range
// however here we don't really use add function, we just sum the elements of
// the array in the range
ul sequential_compute(ui start, ui end) {
  ul sum = 0;
  for (ui i = start - 1; i < end; i++)
    sum += (ul)arr[i];
  return sum;
}

struct range // instances of this class will hold the start and end of the range
             // of the array assigned to a thread/process or whatever
{
  ui start, end;
};

// generate_random_integers function that takes a filename, number of integers
// to generate, start and end of the range and writes the generated integers to
// the file
int generate_random_integers(char *filename, int n, int start, int end) {
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    return 0;
  }
  for (int i = 0; i < n; i++) {
    fprintf(file, "%d\n", rand() % (end - start + 1) + start);
  }
  fclose(file);
  return 1;
}

// get_numbers_array_from_file function that takes a filename and reads the
// integers from the file and stores them in the arr array and updates the
// arr_sz variable
void get_numbers_array_from_file(char *fileName) {
  FILE *file = fopen(fileName, "r");
  if (file == NULL) {
    printf("Error: Cannot open file.\n");
    exit(1);
  }

  // Count the number of elements in the file
  int numElements = 0;
  while (fscanf(file, "%u", &numElements) == 1) {
    arr_sz++;
  }

  // Allocate memory for the arr array
  arr = (unsigned int *)malloc(arr_sz * sizeof(unsigned int));

  if (arr == NULL) {
    printf("Error: Cannot allocate memory.\n");
    exit(1);
  }

  // Read the arr from the file and fill the array
  rewind(file); // go back to the start of the file
  for (ui i = 0; i < arr_sz; i++) {
    fscanf(file, "%u", &arr[i]);
  }

  // Close the file
  fclose(file);
}

void *calculate_sum(void *ptr) {
  struct range *t = (struct range *)ptr;
  ul s = sequential_compute(t->start, t->end);
  pthread_mutex_lock(&sum_mutex);
  result += s;
  pthread_mutex_unlock(&sum_mutex);
  return NULL;
}

ul threads_compute(ui n_threads) {
  // result to hold the sum of the results of the function pointer applied to
  // the elements of the array
  result = 0;

  // calculate number of elements per thread
  ui n_elements_per_thread = arr_sz / n_threads;

  // create threads divide the work among the threads
  pthread_t threads_ids_list[n_threads];

  // ranges_list to hold the start and end of the ranges of the array assigned
  // to different threads
  struct range *ranges_list =
      (struct range *)malloc(n_threads * sizeof(struct range));

  for (ui i = 0; i < n_threads; i++) {
    if (i == n_threads - 1) {
      ranges_list[i].start = i * n_elements_per_thread + 1;
      ranges_list[i].end = arr_sz;
    } else {
      ranges_list[i].start = i * n_elements_per_thread + 1;
      ranges_list[i].end = (i + 1) * n_elements_per_thread;
    }
    int error = pthread_create(&(threads_ids_list[i]), NULL, &calculate_sum,
                               &(ranges_list[i]));
    if (error != 0)
      printf("\nThreads creation error: [%s]", strerror(error));
  }
  for (ui i = 0; i < n_threads; i++) {
    int error = pthread_join(threads_ids_list[i], NULL);
    if (error != 0)
      printf("\nThreads join error: [%s]", strerror(error));
  }
  return result;
}

ul mmap_compute(ui nproc) {
  // calculate number of elements per thread
  ui n_elements_per_proc = arr_sz / nproc;
  pid_t pid;
  ul sum;

  ul *ptr = mmap(NULL, sizeof(ul), (PROT_READ | PROT_WRITE),
                 (MAP_SHARED | MAP_ANONYMOUS), -1, 0);

  if (ptr == MAP_FAILED) {
    perror("Error in mapping");
    return 0;
  }

  // initialize the shared memory to 0
  *ptr = 0;

  for (ui i = 0; i < nproc; i++) {

    pid = fork();
    if (pid < 0) {
      perror("fork");
      exit(1);
    } else if (pid == 0) {
      if (i == nproc - 1) {
        pthread_mutex_lock(&sum_mutex);
        *ptr += sequential_compute(i * n_elements_per_proc + 1, arr_sz);
        pthread_mutex_unlock(&sum_mutex);
      } else {
        pthread_mutex_lock(&sum_mutex);
        *ptr += sequential_compute(i * n_elements_per_proc + 1,
                                   (i + 1) * n_elements_per_proc);
        pthread_mutex_unlock(&sum_mutex);
      }
      exit(0);
    }
  }

  // while (wait(NULL) != -1 || errno != ECHILD);
  while (wait(NULL) > 0)
    ;

  sum = *ptr;
  int err = munmap(ptr, sizeof(ul));
  if (err != 0) {
    perror("UnMapping Failed");
    return 1;
  }

  return sum;
}

// function to print the elements of arr
void print_numbers_array() {
  for (ui i = 0; i < arr_sz; i++) {
    printf("%u\n", arr[i]);
  }
}

// Modified to accept array and size directly
ul parallel_compute(ui nproc) {
  int n_elements_per_proc = arr_sz / nproc;
  int file_descriptor[nproc][2];
  for (ui i = 0; i < nproc; i++) {
    if (pipe(file_descriptor[i]) == -1) {
      fprintf(stderr, "Error, failure to creating pipe\n");
      return 1;
    } else {
      // fprintf(stderr, "Created successfully\n");
    }
    pid_t pid = fork();
    if (pid < 0) {
      perror("fork");
      exit(1);
    } else if (pid == 0) {
      ul s;
      if (i == nproc - 1)
        s = sequential_compute(i * n_elements_per_proc + 1, arr_sz);
      else
        s = sequential_compute(i * n_elements_per_proc + 1,
                               (i + 1) * n_elements_per_proc);
      close(file_descriptor[i][0]);
      write(file_descriptor[i][1], &s, sizeof(s));
      exit(0);
    }
  }
  while (wait(NULL) != -1 || errno != ECHILD)
    ;
  ul sum = 0, partial_sum = 0;
  for (ui i = 0; i < nproc; i++) {
    close(file_descriptor[i][1]);
    read(file_descriptor[i][0], &partial_sum, sizeof(partial_sum));
    sum += partial_sum;
  }
  for (ui i = 0; i < nproc; i++) {
    close(file_descriptor[i][0]);
    close(file_descriptor[i][1]);
  }
  return sum;
}

void Test(
    int argc,
    char *argv[]) { // this function is used to test the functions ensuring they
                    // work correctly and all return the same result
  if (argc != 4) {
    fprintf(stderr, "Usage: %s file_name nproc/threads n_iter\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // generate random integers and write them to a file
  generate_random_integers(argv[1], 10000, 1, 100000);

  // test sequential_compute
  get_numbers_array_from_file(argv[1]);
  ul res = sequential_compute(1, arr_sz);
  printf("The sum of the elements of the array using sequential_compute is: "
         "%llu\n",
         res);

  // test function threads_compute
  get_numbers_array_from_file(argv[1]);
  ui n_threads = atoi(argv[2]);
  res = threads_compute(n_threads);
  printf(
      "The sum of the elements of the array using threads_compute is: %llu\n",
      res);

  // test function mmap_compute
  get_numbers_array_from_file(argv[1]);
  ui nproc = atoi(argv[2]);
  res = mmap_compute(nproc);
  printf("The sum of the elements of the array using mmap_compute is: %llu\n",
         res);

  // test function parallel_compute
  get_numbers_array_from_file(argv[1]);
  nproc = atoi(argv[2]);
  res = parallel_compute(nproc);
  printf(
      "The sum of the elements of the array using parallel_compute is: %llu\n",
      res);
}

void populate_excel_files(int N, ui nproc, ui n_iter,
                          FILE *sequential_compute_file,
                          FILE *threads_compute_file, FILE *mmap_compute_file,
                          FILE *parallel_compute_file) {

  // for the current N value, run the functions n_iter times and write the time
  // taken to run the function to the file

  for (ui i = 0; i < n_iter; i++) {
    struct timeval start, end;
    ul elapsed_time;
    ul res;

    // sequential_compute
    gettimeofday(&start, NULL);
    res = sequential_compute(1, arr_sz);
    gettimeofday(&end, NULL);
    elapsed_time =
        (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
    if (i == 0)
      fprintf(sequential_compute_file, "%d,", N);
    fprintf(sequential_compute_file, "%llu,", elapsed_time);

    // threads_compute
    FLUSH();
    gettimeofday(&start, NULL);
    res = threads_compute(nproc);
    gettimeofday(&end, NULL);
    FLUSH();
    elapsed_time =
        (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
    if (i == 0)
      fprintf(threads_compute_file, "%d,", N);
    fprintf(threads_compute_file, "%llu,", elapsed_time);

    // //mmap_compute
    gettimeofday(&start, NULL);
    FLUSH();
    res = mmap_compute(
        nproc); // ensure that the following code is run by parent process only
    FLUSH();
    gettimeofday(&end, NULL);
    FLUSH();
    elapsed_time =
        (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
    if (i == 0)
      fprintf(mmap_compute_file, "%d,", N);
    FLUSH();
    fprintf(mmap_compute_file, "%llu,", elapsed_time);
    FLUSH();

    // parallel_compute
    gettimeofday(&start, NULL);
    FLUSH();
    res = parallel_compute(nproc);
    FLUSH();
    gettimeofday(&end, NULL);
    elapsed_time =
        (end.tv_sec - start.tv_sec) * 1000000 + end.tv_usec - start.tv_usec;
    if (i == 0)
      fprintf(parallel_compute_file, "%d,", N);
    fprintf(parallel_compute_file, "%llu,", elapsed_time);
  }

  fprintf(sequential_compute_file, "\n");
  fprintf(threads_compute_file, "\n");
  FLUSH();
  fprintf(mmap_compute_file, "\n");
  FLUSH();
  fprintf(parallel_compute_file, "\n");
  FLUSH();
}

void Test1(int argc, char *argv[]) {
  if (argc != 4) {
    fprintf(stderr, "Usage: %s file_name nproc/threads n_iter\n", argv[0]);
    exit(EXIT_FAILURE);
  }

  // delete files if they exist before creating them
  remove("sequential_compute.csv");
  remove("threads_compute.csv");
  remove("mmap_compute.csv");
  remove("parallel_compute.csv");

  // create 4 csv files to hold the time taken to run each of the 4 functions
  // for n_iter times for each N value
  FILE *sequential_compute_file = fopen("sequential_compute.csv", "a");
  FILE *threads_compute_file = fopen("threads_compute.csv", "a");
  FILE *mmap_compute_file = fopen("mmap_compute.csv", "a");
  FILE *parallel_compute_file = fopen("parallel_compute.csv", "a");

  // for each file the column headers will be N, then 50 colummns for each of
  // the n_iter times the function was run. the rows will be the time taken to
  // run the function for each N value for each of the n_iter times the function
  // was run
  fprintf(sequential_compute_file, "N,");
  fprintf(threads_compute_file, "N,");
  fprintf(mmap_compute_file, "N,");
  fprintf(parallel_compute_file, "N,");

  ul nproc = atoi(argv[2]);
  ui n_iter = atoi(argv[3]);

  for (ui i = 0; i < n_iter; i++) {
    fprintf(sequential_compute_file, "Run %d,", i + 1);
    fprintf(threads_compute_file, "Run %d,", i + 1);
    fprintf(mmap_compute_file, "Run %d,", i + 1);
    fprintf(parallel_compute_file, "Run %d,", i + 1);
  }

  fprintf(sequential_compute_file, "\n");
  fprintf(threads_compute_file, "\n");
  fprintf(mmap_compute_file, "\n");
  fprintf(parallel_compute_file, "\n");
  FLUSH();

  for (int N = 10000; N <= 500000; N += 10000) {

    generate_random_integers(
        argv[1], N, 1,
        100000); // generate random integers and write them to a file

    get_numbers_array_from_file(
        argv[1]); // now I have the arr and arr_sz variables updated with the
                  // numbers from the file generated

    // cheack if the files were created successfully
    if (sequential_compute_file == NULL || threads_compute_file == NULL ||
        mmap_compute_file == NULL || parallel_compute_file == NULL) {
      printf("Error: Cannot open file.\n");
      exit(1);
    } else {
      populate_excel_files(N, nproc, n_iter, sequential_compute_file,
                           threads_compute_file, mmap_compute_file,
                           parallel_compute_file);
    }
  }
}

int main(int argc, char *argv[]) {
  // Test(argc, argv);
  Test1(argc, argv);
  return 0;
}
