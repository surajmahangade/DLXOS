#include "usertraps.h"
#include "misc.h"

// int reccursive_function(int count);


inline int recursive_function(int count) {
  if (count <= 0) {
    return 0;
  } else {
    return 2 + recursive_function(count - 1);
  }
}


void main (int argc, char *argv[])
{
  sem_t s_procs_completed; // Semaphore to signal the original process that we're done
  char *p;
  char *og;
  int x;
  int result;
  int i;
  int size;
  if (argc != 2) { 
    Printf("Usage: %s <handle_to_procs_completed_semaphore>\n"); 
    Exit();
  } 

  // Convert the command-line strings into integers for use as handles
  s_procs_completed = dstrtol(argv[1], NULL, 10);

  // Now print a message to show that everything worked
  Printf("hello_world (%d): Hello world!\n", getpid());
  size = 1024*1; // 4KB
  // call malloc
  og = p = (char *)malloc(size); // allocate 
  Printf("hello_world (%d): allocated size %d at address %d\n", getpid(), size, p);
  mfree(p);
  p = (char *)malloc(size); // allocate 
  Printf("hello_world (%d): allocated size %d at address %d\n", getpid(), size, p);
  mfree(p);
  p = (char *)malloc(size); // allocate 
  Printf("hello_world (%d): allocated size %d at address %d\n", getpid(), size, p);
  mfree(p);
  p = (char *)malloc(size); // allocate 
  Printf("hello_world (%d): allocated size %d at address %d\n", getpid(), size, p);
  mfree(p);
  // access the allocated memory
  for (i = 0; i < size/sizeof(char); i++) {
    // accesing addres
    // Printf("hello_world (%d): accessing address %d\n", getpid(), &p[i]);
    p[i] = i;
  }
  i = ((1024*4) - sizeof(char)); // last byte out of allocated range
  // acces last address 1 byte
  x = og[i];
  Printf("hello_world (%d): read value %c from address %d\n", getpid(), x, &og[i]);
  og = p = (char *)malloc(size);
  og = p = (char *)malloc(size);
  og = p = (char *)malloc(size);
  og = p = (char *)malloc(size);
  // free
  // Printf("hello_world (%d): freeing allocated memory at address %d\n", getpid(), p);
  // mfree(p);

  // p = (int*)(0x003DFFFC); // out of range
  // Printf("hello_world (%d): accessing address %d, size of int %d\n", getpid(), p, sizeof(int));
  // x = *p;        // READ → should raise TRAP_ACCESS (range)
  // // print x
  // Printf("hello_world (%d): read value %d from address %d\n", getpid(), x, p);
  // make the stack to grow larger than 1 page.
  result = recursive_function(1000);
  Printf("hello_world (%d): recursive_function result %d\n", getpid(), result);
  // Signal the semaphore to tell the original process that we're done
  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("hello_world (%d): Bad semaphore s_procs_completed (%d)!\n", getpid(), s_procs_completed);
    Exit();
  }

  Printf("hello_world (%d): Done!\n", getpid());
}
