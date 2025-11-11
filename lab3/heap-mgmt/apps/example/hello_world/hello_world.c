#include "usertraps.h"
#include "misc.h"

static inline int recursive_function(int n) { return (n <= 0) ? 0 : 2 + recursive_function(n - 1); }

void main (int argc, char *argv[]) {
  sem_t s_procs_completed;
  void *a,*b,*c,*d,*x1,*x2,*big;
  void *gen[16]; int ng = 0;
  void *small[8]; int ns = 0;
  int i;
  int result;

  if (argc != 2) { Printf("Usage: %s <sem>\n", argv[0]); Exit(); }
  s_procs_completed = dstrtol(argv[1], NULL, 10);

  Printf("\n===== HEAP ALLOCATOR TESTS START (pid %d) =====\n", getpid());

  // Test S0: exactly one page
  Printf("\n-- S0: malloc(4096) then free --\n");
  big = malloc(4096);
  Printf("hello_world (%d): malloc(4096) -> %d\n", getpid(), (int)big);
  if (big) { ((char*)big)[0] = 0x5a; Printf("hello_world (%d): freeing %d\n", getpid(), (int)big); mfree(big); }

  // Test S1: basic different-size allocations
  Printf("\n-- S1: Basic different-size allocations --\n");
  a = malloc(16);   Printf("hello_world (%d): malloc(16)  -> %d\n",  getpid(), (int)a); gen[ng++] = a; // order 0
  b = malloc(40);   Printf("hello_world (%d): malloc(40)  -> %d\n",  getpid(), (int)b); gen[ng++] = b; // order 1
  c = malloc(100);  Printf("hello_world (%d): malloc(100) -> %d\n",  getpid(), (int)c); gen[ng++] = c; // order 2
  d = malloc(500);  Printf("hello_world (%d): malloc(500) -> %d\n",  getpid(), (int)d); gen[ng++] = d; // order 4

  // Test S2: free and reuse
  Printf("\n-- S2: Free and reuse behavior --\n");
  Printf("hello_world (%d): freeing %d (malloc(40))\n", getpid(), (int)b);
  mfree(b);
  b = malloc(60);   Printf("hello_world (%d): malloc(60)  -> %d (expect reuse of 64B)\n", getpid(), (int)b);

  // Test S3: buddy coalescing
  Printf("\n-- S3: Buddy coalescing --\n");
  x1 = malloc(40); x2 = malloc(40);
  Printf("hello_world (%d): x1=%d x2=%d\n", getpid(), (int)x1, (int)x2);
  mfree(x1);
  Printf("hello_world (%d): freeing x2=%d (should coalesce)\n", getpid(), (int)x2);
  mfree(x2);

  // Test S4: 8 small 32B blocks and pairwise coalesce; keep separate array
  Printf("\n-- S4: Multiple small allocations and selective frees --\n");
  ns = 0;
  for (i = 0; i < 8; i++) {
    small[ns] = malloc(16);
    Printf("hello_world (%d): small[%d] = %d\n", getpid(), ns, (int)small[ns]);
    ns++;
  }
  Printf("hello_world (%d): free small[0] and small[1]\n", getpid());
  mfree(small[0]); mfree(small[1]);
  Printf("hello_world (%d): free small[2] and small[3]\n", getpid());
  mfree(small[2]); mfree(small[3]);

  // Test S5: null pointer free
  Printf("\n-- S5: Free NULL pointer --\n");
  result = mfree(NULL);
  Printf("hello_world (%d): mfree(NULL) returned %d (expect -1)\n", getpid(), result);


  Printf("\n===== HEAP ALLOCATOR TESTS END =====\n", getpid());

  // Stack growth noise to ensure nothing crashes
  Printf("hello_world (%d): recursive_function result %d\n", getpid(), recursive_function(1000));

  if (sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("hello_world (%d): Bad semaphore %d\n", getpid(), s_procs_completed);
    Exit();
  }
  Printf("hello_world (%d): Done.\n", getpid());
}
