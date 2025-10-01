#include "lab2-api.h"
#include "usertraps.h"
#include "misc.h"
#include "spawn.h"

void main (int argc, char *argv[])
{
  mem_buffer *mc;
  int num_N3;
  int i;
  uint32 h_mem;
  sem_t s_procs_completed;

  if (argc != 4) { 
    Printf("Usage: "); Printf(argv[0]); Printf(" <handle_to_shared_memory_page> <handle_to_page_mapped_semaphore> <number of N3>\n"); 
    Exit();
  } 

  h_mem = dstrtol(argv[1], NULL, 10);
  s_procs_completed = dstrtol(argv[2], NULL, 10);
  num_N3 = dstrtol(argv[3], NULL, 10);

  if ((mc = (mem_buffer *)shmat(h_mem)) == NULL) {
    Printf("Could not map the virtual address to the memory in "); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }

  for (i = 0; i < num_N3; i++) {
    Printf("An N3 molecule is created\n");
    sem_signal(mc->N3);
  }

  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }
}