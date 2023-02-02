#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint32_t u32;
typedef int32_t i32;

struct process {
  u32 pid;
  u32 arrival_time;
  u32 burst_time;

  TAILQ_ENTRY(process) pointers;

  /* Additional fields here */
  u32 remaining_time;
  u32 remaining_quantum;
  u32 start_execute_time; 
  u32 end_time;
  bool inserted;

  /* End of "Additional fields here" */
};

TAILQ_HEAD(process_list, process);

u32 next_int(const char **data, const char *data_end) {
  u32 current = 0;
  bool started = false;
  while (*data != data_end) {
    char c = **data;

    if (c < 0x30 || c > 0x39) {
      if (started) {
	return current;
      }
    }
    else {
      if (!started) {
	current = (c - 0x30);
	started = true;
      }
      else {
	current *= 10;
	current += (c - 0x30);
      }
    }

    ++(*data);
  }

  printf("Reached end of file while looking for another integer\n");
  exit(EINVAL);
}

u32 next_int_from_c_str(const char *data) {
  char c;
  u32 i = 0;
  u32 current = 0;
  bool started = false;
  while ((c = data[i++])) {
    if (c < 0x30 || c > 0x39) {
      exit(EINVAL);
    }
    if (!started) {
      current = (c - 0x30);
      started = true;
    }
    else {
      current *= 10;
      current += (c - 0x30);
    }
  }
  return current;
}

void init_processes(const char *path,
                    struct process **process_data,
                    u32 *process_size)
{
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    int err = errno;
    perror("open");
    exit(err);
  }

  struct stat st;
  if (fstat(fd, &st) == -1) {
    int err = errno;
    perror("stat");
    exit(err);
  }

  u32 size = st.st_size;
  const char *data_start = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  if (data_start == MAP_FAILED) {
    int err = errno;
    perror("mmap");
    exit(err);
  }

  const char *data_end = data_start + size;
  const char *data = data_start;
  

  *process_size = next_int(&data, data_end);

  *process_data = calloc(sizeof(struct process), *process_size);
  if (*process_data == NULL) {
    int err = errno;
    perror("calloc");
    exit(err);
  }

  for (u32 i = 0; i < *process_size; ++i) {
    (*process_data)[i].pid = next_int(&data, data_end);
    (*process_data)[i].arrival_time = next_int(&data, data_end);
    (*process_data)[i].burst_time = next_int(&data, data_end);
  }
  
  munmap((void *)data, size);
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc != 3) {
    return EINVAL;
  }
  struct process *data;
  u32 size;
  init_processes(argv[1], &data, &size);

  u32 quantum_length = next_int_from_c_str(argv[2]);

  struct process_list list;
  TAILQ_INIT(&list);

  u32 total_waiting_time = 0;
  u32 total_response_time = 0;

  /* Your code here */

  struct process *p;
  // set the remaining/quantum time to the initial burst time for each process 
  // set the start_execute_time to an arbituary number so it is not updated everytime in the while loop
  for (u32 i = 0; i < size; i++) {
      p = &data[i];
      p->remaining_time = p->burst_time;
      p->remaining_quantum = quantum_length;
      p->start_execute_time = 34567;
      p->inserted = false;
  }
  u32 t = 0;
  bool finished = false;
  int process_count = size;
  while (!finished) {
      for (u32 i = 0; i < size; i++) {
          p = &data[i];
          // insert to the queue if it arrived
          if (p->arrival_time == t && p->inserted == false) {
              TAILQ_INSERT_TAIL(&list, p, pointers);
          }
      }

      if (!TAILQ_EMPTY(&list)) {
          // adjust remaining time and quantum and update start_execute_time if not already set
          struct process* head = TAILQ_FIRST(&list);
          head->remaining_time--;
          head->remaining_quantum--;
          if (head->start_execute_time == 34567)
              head->start_execute_time = t; 

          // CASE 1: head is finished (record waiting and response time, remove from list)
          if (head->remaining_quantum == 0 && head->remaining_time == 0) {
              head->end_time = t+1;
              total_waiting_time += head->end_time - head->arrival_time - head->burst_time;
              total_response_time += head->start_execute_time - head->arrival_time;
              TAILQ_REMOVE(&list, head, pointers);
              process_count--;
              // if queue is empty and all processes have been executed, while loop is done 
              if (TAILQ_EMPTY(&list) && process_count == 0) {
                  finished = true;
              }
          }

          // CASE 2: used up quantum, head needs to be pushed to the end of the queue 
          if (head->remaining_quantum == 0 && head->remaining_time > 0) {
              head->remaining_quantum = quantum_length;
              TAILQ_REMOVE(&list, head, pointers);
              for (u32 i = 0; i < size; i++) {
                p = &data[i];
                // insert to the queue if it arrived
                if (p->arrival_time == t+1) {
                TAILQ_INSERT_TAIL(&list, p, pointers);
                p->inserted = true;
                }
             }
              TAILQ_INSERT_TAIL(&list, head, pointers);
          }

          // CASE 3: head is finished (record waiting and response time, remove from list)
          if (head->remaining_quantum > 0 && head->remaining_time == 0) {
              head->end_time = t+1;
              total_waiting_time += head->end_time - head->arrival_time - head->burst_time;
              total_response_time += head->start_execute_time - head->arrival_time;
              TAILQ_REMOVE(&list, head, pointers);
              process_count--;
              // if queue is empty and all processes have been executed, while loop is done 
              if (TAILQ_EMPTY(&list) && process_count == 0) {
                  finished = true;
              }
          }
      }
      t++;
  }

  /* End of "Your code here" */

  printf("Average waiting time: %.2f\n", (float) total_waiting_time / (float) size);
  printf("Average response time: %.2f\n", (float) total_response_time / (float) size);

  free(data);
  return 0;
}
