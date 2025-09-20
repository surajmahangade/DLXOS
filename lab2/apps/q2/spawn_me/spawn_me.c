#include "lab2-api.h"
#include "usertraps.h"
#include "misc.h"

#include "spawn.h"
// #include <q2/include/spawn.h>

void main (int argc, char *argv[])
{
  mem_buffer *mc;        // Used to access missile codes in shared memory page
  uint32 h_mem;            // Handle to the shared memory page
  sem_t s_procs_completed; // Semaphore to signal the original process that we're done

  if (argc != 3) { 
    Printf("Usage: "); Printf(argv[0]); Printf(" <handle_to_shared_memory_page> <handle_to_page_mapped_semaphore>\n"); 
    Exit();
  } 

  // Convert the command-line strings into integers for use as handles
  h_mem = dstrtol(argv[1], NULL, 10); // The "10" means base 10
  s_procs_completed = dstrtol(argv[2], NULL, 10);

  // Map shared memory page into this process's memory space
  if ((mc = (mem_buffer *)shmat(h_mem)) == NULL) {
    Printf("Could not map the virtual address to the memory in "); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }

  //mc has start, end, count, buffer[10]
 
  // Now print a message to show that everything worked
  Printf("spawn_me: This is one of the %d count  ", mc->count);
  Printf("spawn_me: Missile code is: %c\n", mc->buffer_lock);
  Printf("spawn_me: My PID is %d\n", Getpid());

  // Signal the semaphore to tell the original process that we're done
  Printf("spawn_me: PID %d is complete.\n", Getpid());
  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }
}
