#include "usertraps.h"
#include "misc.h"

void main (int argc, char *argv[])
{
  sem_t s_procs_completed; // Semaphore to signal the original process that we're done
  int *p;
  int x;
  if (argc != 2) { 
    Printf("Usage: %s <handle_to_procs_completed_semaphore>\n"); 
    Exit();
  } 

  // Convert the command-line strings into integers for use as handles
  s_procs_completed = dstrtol(argv[1], NULL, 10);

  // Now print a message to show that everything worked
  Printf("TRAP_ACCESS (%d): Hello world! tmo\n", getpid());

  p = (int*)(5190100); // out of range
  Printf("TRAP_ACCESS (%d): accessing address %d, size of int %d\n", getpid(), p, sizeof(int));
  x = *p;        // READ → should raise TRAP_ACCESS (range)
  // print x
  Printf("TRAP_ACCESS (%d): read value %d from address %d\n", getpid(), x, p);

  // Signal the semaphore to tell the original process that we're done
  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("TRAP_ACCESS (%d): Bad semaphore s_procs_completed (%d)!\n", getpid(), s_procs_completed);
    Exit();
  }

  Printf("TRAP_ACCESS (%d): Done!\n", getpid());
}
