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
  void *a, *b, *c, *d, *x1, *x2, *big;
  void *blocks[16];
  int nb = 0;
  if (argc != 2) { 
    Printf("Usage: %s <handle_to_procs_completed_semaphore>\n"); 
    Exit();
  } 

  // Convert the command-line strings into integers for use as handles
  s_procs_completed = dstrtol(argv[1], NULL, 10);

  // Now print a message to show that everything worked
  Printf("hello_world (%d): Hello world!\n", getpid());
  // We'll run several in-depth tests exercising the buddy allocator.
  Printf("\n===== HEAP ALLOCATOR TESTS START (pid %d) =====\n", getpid());

  // Helper local variables
  

  // Test 1: basic allocations of various sizes (should produce different orders)
  Printf("\n-- Test1: Basic different-size allocations --\n");
  a = malloc(16);   // should allocate 32 bytes (order 0)
  Printf("hello_world (%d): malloc(16) -> %d\n", getpid(), (int)a);
  b = malloc(40);   // should allocate 64 bytes (order 1)
  Printf("hello_world (%d): malloc(40) -> %d\n", getpid(), (int)b);
  c = malloc(100);  // should allocate 128 bytes (order 2)
  Printf("hello_world (%d): malloc(100) -> %d\n", getpid(), (int)c);
  d = malloc(500);  // should allocate 512 bytes (order 4)
  Printf("hello_world (%d): malloc(500) -> %d\n", getpid(), (int)d);

  // Keep references to avoid reuse
  blocks[nb++] = a; blocks[nb++] = b; blocks[nb++] = c; blocks[nb++] = d;

  // Test 2: free one block and reallocate similar size -> should reuse freed block
  Printf("\n-- Test2: Free and reuse behavior --\n");
  Printf("hello_world (%d): freeing address %d (malloc(40) result)\n", getpid(), (int)b);
  mfree(b);
  b = malloc(60); // still fits in 64 bytes; should ideally reuse previous freed 64-byte block
  Printf("hello_world (%d): malloc(60) -> %d (should reuse freed 64-byte block)\n", getpid(), (int)b);
  blocks[nb++] = b;

  // Test 3: allocate two adjacent equal-size blocks and then free them to observe coalescing
  Printf("\n-- Test3: Buddy coalescing (allocate two equal blocks then free both) --\n");
  x1 = malloc(40); // 64
  x2 = malloc(40); // 64
  Printf("hello_world (%d): x1=%d x2=%d\n", getpid(), (int)x1, (int)x2);
  Printf("hello_world (%d): freeing x1=%d\n", getpid(), (int)x1);
  mfree(x1);
  Printf("hello_world (%d): freeing x2=%d (should trigger coalescing upward)\n", getpid(), (int)x2);
  mfree(x2);

  // Test 4: many small allocations to exercise splitting
  Printf("\n-- Test4: Multiple small allocations (32-byte blocks) and selective frees --\n");
  for (i = 0; i < 8; i++) {
    blocks[nb++] = malloc(16); // each should be 32 bytes
    Printf("hello_world (%d): small alloc %d -> %d\n", getpid(), i, (int)blocks[nb-1]);
  }
  // Free pairs to coalesce progressively
  Printf("hello_world (%d): freeing small blocks 0 and 1\n", getpid());
  mfree(blocks[4]); mfree(blocks[5]);
  Printf("hello_world (%d): freeing small blocks 2 and 3\n", getpid());
  mfree(blocks[6]); mfree(blocks[7]);

  // Test 5: large allocation occupying the rest or entire heap
  Printf("\n-- Test5: Large allocation (try full page 4096 bytes) --\n");
  big = malloc(4096); // may succeed if full-page allowed
  Printf("hello_world (%d): malloc(4096) -> %d\n", getpid(), (int)big);
  if (big) {
    Printf("hello_world (%d): freeing big allocation %d\n", getpid(), (int)big);
    mfree(big);
  }

  // Cleanup: free everything remaining (be conservative)
  Printf("\n-- Cleanup: freeing remaining allocations --\n");
  for (i = 0; i < nb; i++) {
    if (blocks[i]) {
      Printf("hello_world (%d): freeing blocks[%d] = %d\n", getpid(), i, (int)blocks[i]);
      mfree(blocks[i]);
    }
  }

  Printf("===== HEAP ALLOCATOR TESTS END =====\n\n");
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
