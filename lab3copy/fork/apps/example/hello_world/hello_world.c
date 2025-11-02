#include "usertraps.h"
#include "misc.h"

// int reccursive_function(int count);


inline int recursive_function(int count) {
  if (count <= 0) {
    return 0;
  } else {
    return 1 + recursive_function(count - 1);
  }
}


void main (int argc, char *argv[])
{
  sem_t s_procs_completed; // Semaphore to signal the original process that we're done
  int *p;
  int x;
  int result;
  int pid;
  if (argc != 2) { 
    Printf("Usage: %s <handle_to_procs_completed_semaphore>\n"); 
    Exit();
  } 

  // Convert the command-line strings into integers for use as handles
  s_procs_completed = dstrtol(argv[1], NULL, 10);

  // Now print a message to show that everything worked
  Printf("hello_world (%d): Hello world!\n", getpid());

  // p = (int*)(0x003DFFFC); // out of range
  // Printf("hello_world (%d): accessing address %d, size of int %d\n", getpid(), p, sizeof(int));
  // x = *p;        // READ → should raise TRAP_ACCESS (range)
  // // print x
  // Printf("hello_world (%d): read value %d from address %d\n", getpid(), x, p);
  // make the stack to grow larger than 1 page.
  pid = fork();
  Printf("hello_world (%d): fork returned %d\n", getpid(), pid);
  pid = fork();
  Printf("hello_world (%d): fork returned %d\n", getpid(), pid);
  result = recursive_function(100);
  // Printf("hello_world (%d): recursive_function result %d\n", getpid(), result);
  // Signal the semaphore to tell the original process that we're done
  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("hello_world (%d): Bad semaphore s_procs_completed (%d)!\n", getpid(), s_procs_completed);
    Exit();
  }

  Printf("hello_world (%d): Done!\n", getpid());
}
