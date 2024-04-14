#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>

// This describes the reading policy for each process.
// If the first bit is set, it means the process should read the first line if
// it initially starts reading from the middle of the line. Otherwise, the
// process will seek to the next line and starting reading from there. Similarly
// with the last line and the second bit.
typedef enum {
  READ_POLICY_READ_MID_ONLY = 0,
  READ_POLICY_READ_FIRST = (1 << 0),
  READ_POLICY_READ_LAST = (1 << 1),
} ReadPolicyEnum;

typedef enum {
  RETURN_OK,
  RETURN_FAILED_ARG_PARSE,
  RETURN_FAILED_FILE_OPEN,
  RETURN_FALIED_N_PROC_PARSE,
  RETURN_FALIED_N_PROC_NEGATIVE,
  RETURN_FAILED_FSTAT,
  RETURN_FAILED_MAP_FILE,
  RETURN_FAILED_SHARED_MEM_ALLOC,
} ReturnValuesEnum;

int add(int x, int y) { return x + y; }

int parse_args(int argc, char *argv[], int *fd, int *n_proc) {
  if (argc < 3) {
    fprintf(stderr, "Wrong number of arguments.\nUsage: mmap_compute "
                    "[FILE_NAME] [NUMBER OF PROCESSES]\n");
    return RETURN_FAILED_ARG_PARSE;
  }

  *fd = open(argv[1], O_RDONLY);
  if (*fd == -1) {
    fprintf(stderr, "Failed to open the file \"%s\"\n", argv[1]);
    return RETURN_FAILED_FILE_OPEN;
  }

  *n_proc = atoi(argv[2]);
  if (*n_proc == 0) {
    fprintf(stderr, "Failed to parse \"%s\", it's either 0 or not a number\n",
            argv[2]);
    return RETURN_FAILED_ARG_PARSE;
  }
  if (*n_proc < 0) {
    fprintf(stderr, "Failed to parse \"%s\" as it's a negative number\n",
            argv[2]);
    return RETURN_FALIED_N_PROC_NEGATIVE;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  int fd, n_proc;

  int ret = parse_args(argc, argv, &fd, &n_proc);
  if (ret < 0) {
    return ret;
  }

  // Get file size
  struct stat file_stat;
  if (fstat(fd, &file_stat) == -1) {
    fprintf(stderr, "Couldn't get file size for file\n");
    return RETURN_FAILED_FSTAT;
  }

  // Map the file into memory for easier handling
  char *file_in_memory =
      mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (file_in_memory == MAP_FAILED) {
    fprintf(stderr, "Failed to map file into memory\n");
    return RETURN_FAILED_MAP_FILE;
  }

  // Initialize all shared data:
  // Outputs for each process
  uint64_t *proc_out =
      mmap(NULL, n_proc * sizeof(uint64_t), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, 0, 0);
  // Starting indices
  size_t *starting_pos =
      mmap(NULL, n_proc * sizeof(size_t), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, 0, 0);
  // Ending indices
  size_t *ending_pos =
      mmap(NULL, n_proc * sizeof(size_t), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, 0, 0);
  // First and last line reading policies
  uint8_t *read_policies =
      mmap(NULL, n_proc * sizeof(uint8_t), PROT_READ | PROT_WRITE,
           MAP_SHARED | MAP_ANONYMOUS, 0, 0);
  if (proc_out == MAP_FAILED || starting_pos == MAP_FAILED ||
      ending_pos == MAP_FAILED || read_policies == MAP_FAILED) {
    fprintf(stderr, "Failed to allocate shared memory\n");
    return RETURN_FAILED_SHARED_MEM_ALLOC;
  }

  // Initialize the shared data
  uint32_t data_per_proc = file_stat.st_size / n_proc;
  for (int i = 0; i < n_proc - 1; ++i) {
    proc_out[i] = 0;
    starting_pos[i] = i * data_per_proc;
    ending_pos[i] = (i + 1) * data_per_proc - 1;
    read_policies[i] = READ_POLICY_READ_FIRST;
  }
  proc_out[n_proc - 1] = 0;
  starting_pos[n_proc - 1] = (n_proc - 1) * data_per_proc;
  ending_pos[n_proc - 1] = file_stat.st_size - 1;
  read_policies[n_proc - 1] = READ_POLICY_READ_FIRST | READ_POLICY_READ_LAST;

  for (int i = 0; i < n_proc; ++i) {
  }

  return 0;
}
