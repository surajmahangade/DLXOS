#include "lab2-api.h"
#include "usertraps.h"
#include "misc.h"
// #include <q2/include/spawn.h>
#include "spawn.h"
// #include <string.h>

void Producer(mem_buffer *mc, int process_id);
unsigned int my_pid;
char item;
int i;
int j;
int chars_produced = 0;

int strlen (const char *s)
{
  int		i = 0;

  while (*(s++) != '\0') {
    i++;
  }
  return (i);
}

void sleep(int seconds) {
  //simulate sleep
  // for (j = 0; j < seconds * 1000000; j++);
}

// #define MESSAGE "0123456789"
void main (int argc, char *argv[])
{
  mem_buffer *mc;
  uint32 h_mem;
  sem_t s_procs_completed;

  if (argc != 3) { 
    Printf("Usage: "); Printf(argv[0]); Printf(" <handle_to_shared_memory_page> <handle_to_page_mapped_semaphore>\n"); 
    Exit();
  } 

  // Convert the command-line strings into integers for use as handles
  h_mem = dstrtol(argv[1], NULL, 10);
  s_procs_completed = dstrtol(argv[2], NULL, 10);

  // Map shared memory page into this process's memory space
  if ((mc = (mem_buffer *)shmat(h_mem)) == NULL) {
    Printf("Could not map the virtual address to the memory in "); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }

  my_pid = Getpid();
//   Printf("Producer: This is process with PID %d\n", my_pid);

  // Producer and Consumer work together
  while (chars_produced  < strlen(MESSAGE)) {
    Producer(mc, my_pid);
    }

    // Printf("Producer: PID %d Produced : %d chars\n", my_pid, chars_produced);

  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }
}

void Producer(mem_buffer *mc, int process_id) {
  int message_len = strlen(MESSAGE);
  

  // int inserted;
  if (chars_produced < message_len) {
    lock_acquire(mc->buffer_lock);
    
    // Check if the buffer is full
    if ((mc->end + 1) % BUFFER_SIZE == mc->start) {
      lock_release(mc->buffer_lock);
      return;
    }
    
    // Get character from MESSAGE
    item = MESSAGE[chars_produced];
    // Add the character to the buffer
    mc->buffer[mc->end] = item;
    mc->end = (mc->end + 1) % BUFFER_SIZE;
    // inserted = 1;
    chars_produced++;
    Printf("Producer %d inserted: %c\n", process_id, item);
    
    lock_release(mc->buffer_lock);
  }
}
