#include "usertraps.h"
#include "misc.h"

#define PAGE_SZ     4096
#define TOUCH_BYTE  0x7a

static inline void touch_pages(char *p, int bytes) {
  int off;
  for (off = 0; off < bytes; off += PAGE_SZ) p[off] = TOUCH_BYTE;
  if (bytes > 0) p[bytes - 1] = TOUCH_BYTE; // boundary touch
}

static inline int recursive_function(int n) { return (n <= 0) ? 0 : 2 + recursive_function(n - 1); }

void main (int argc, char *argv[]) {
  sem_t s_procs_completed;
  void *p4k=NULL, *p8k=NULL, *p12k=NULL, *p16k=NULL, *p64k=NULL;
  void *a,*b,*c,*d,*x1,*x2;
  void *small[8]; int ns=0;
  int i;

  if (argc != 2) { Printf("Usage: %s <sem>\n", argv[0]); Exit(); }
  s_procs_completed = dstrtol(argv[1], NULL, 10);

  Printf("\n===== DYNAMIC HEAP TESTS (64KB cap) START pid=%d =====\n", getpid());

  // D0: allocate 1–4 pages and touch each page to trigger backing
  Printf("\n-- D0: Force per-page backing with touches (1–4 pages) --\n");
  p4k  = malloc( 4*1024); Printf("hello_world (%d): malloc(4KB)  -> %d\n", getpid(), (int)p4k);
  p8k  = malloc( 8*1024); Printf("hello_world (%d): malloc(8KB)  -> %d\n", getpid(), (int)p8k);
  p12k = malloc(12*1024); Printf("hello_world (%d): malloc(12KB) -> %d\n", getpid(), (int)p12k);
  p16k = malloc(16*1024); Printf("hello_world (%d): malloc(16KB) -> %d\n", getpid(), (int)p16k);

  if (p4k)  touch_pages((char*)p4k,  4*1024);
  if (p8k)  touch_pages((char*)p8k,  8*1024);
  if (p12k) touch_pages((char*)p12k, 12*1024);
  if (p16k) touch_pages((char*)p16k, 16*1024); // up to 4 pages so far

  // Free in reverse to test coalescing
  if (p16k) { Printf("hello_world (%d): free 16KB %d\n", getpid(), (int)p16k); mfree(p16k); }
  if (p12k) { Printf("hello_world (%d): free 12KB %d\n", getpid(), (int)p12k); mfree(p12k); }
  if (p8k)  { Printf("hello_world (%d): free 8KB  %d\n", getpid(), (int)p8k);  mfree(p8k); }
  if (p4k)  { Printf("hello_world (%d): free 4KB  %d\n", getpid(), (int)p4k);  mfree(p4k); }

  // D0b: allocate full 64KB and touch all 16 pages
  Printf("\n-- D0b: Full-heap allocation and touches (16 pages) --\n");
  p64k = malloc(64*1024); Printf("hello_world (%d): malloc(64KB) -> %d\n", getpid(), (int)p64k);
  if (p64k) touch_pages((char*)p64k, 64*1024);
  if (p64k) { Printf("hello_world (%d): free 64KB %d\n", getpid(), (int)p64k); mfree(p64k); }

  // D1: classic small/medium tests to verify buddy logic unchanged
  Printf("\n-- D1: Buddy behavior unchanged under dynamic heap --\n");
  a = malloc(16);   Printf("hello_world (%d): malloc(16)  -> %d\n", getpid(), (int)a);   // order 0
  b = malloc(40);   Printf("hello_world (%d): malloc(40)  -> %d\n", getpid(), (int)b);   // order 1
  c = malloc(100);  Printf("hello_world (%d): malloc(100) -> %d\n", getpid(), (int)c);   // order 2
  d = malloc(500);  Printf("hello_world (%d): malloc(500) -> %d\n", getpid(), (int)d);   // order 4

  Printf("\n-- D2: Free and reuse 64B --\n");
  mfree(b);
  b = malloc(60);   Printf("hello_world (%d): malloc(60)  -> %d (expect reuse)\n", getpid(), (int)b);

  Printf("\n-- D3: Coalescing check --\n");
  x1 = malloc(40); x2 = malloc(40);
  Printf("hello_world (%d): x1=%d x2=%d\n", getpid(), (int)x1, (int)x2);
  mfree(x1); mfree(x2);

  Printf("\n-- D4: Eight small 32B blocks; free in pairs --\n");
  ns = 0;
  for (i = 0; i < 8; i++) {
    small[ns] = malloc(16);
    Printf("hello_world (%d): small[%d] = %d\n", getpid(), ns, (int)small[ns]);
    ns++;
  }
  mfree(small[0]); mfree(small[1]);
  mfree(small[2]); mfree(small[3]);

  Printf("\n===== DYNAMIC HEAP TESTS (64KB cap) END =====\n", getpid());

  Printf("hello_world (%d): recursive_function result %d\n", getpid(), recursive_function(1000));
  if (sem_signal(s_procs_completed) != SYNC_SUCCESS) { Printf("hello_world (%d): Bad semaphore %d\n", getpid(), s_procs_completed); Exit(); }
  Printf("hello_world (%d): Done.\n", getpid());
}
