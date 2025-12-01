#include "ostraps.h"
#include "dlxos.h"
#include "traps.h"
#include "disk.h"
#include "dfs.h"

void RunOSTests() {
  // STUDENT: run any os-level tests here
  int ret;
  printf("[OSTests] Starting DFS driver tests...\n");

  // Ensure filesystem is open
  ret = DfsOpenFileSystem();
  if (ret == DFS_FAIL) {
    printf("[OSTests] DfsOpenFileSystem failed; tests cannot proceed.\n");
    return;
  }

  // Test 1: Non-block-aligned write and read
  {
    char fname1[] = "small.txt";
    uint32 h1 = DfsInodeOpen(fname1);
    if (h1 == DFS_FAIL) {
      printf("[OSTests] DfsInodeOpen('%s') failed.\n", fname1);
      return;
    }
    char wbuf[283];
    char rbuf[283];
    int i;
    for (i = 0; i < (int)sizeof(wbuf); i++) wbuf[i] = (char)(i * 7 + 3);
    int start = 20;
    ret = DfsInodeWriteBytes(h1, wbuf, start, sizeof(wbuf));
    if (ret == DFS_FAIL || ret != (int)sizeof(wbuf)) {
      printf("[OSTests] Non-block-aligned write failed: ret=%d\n", ret);
      return;
    }
    ret = DfsInodeReadBytes(h1, rbuf, start, sizeof(rbuf));
    if (ret == DFS_FAIL || ret != (int)sizeof(rbuf)) {
      printf("[OSTests] Non-block-aligned read failed: ret=%d\n", ret);
      return;
    }
    for (i = 0; i < (int)sizeof(wbuf); i++) {
      if (wbuf[i] != rbuf[i]) {
        printf("[OSTests] MISMATCH small.txt at offset %d: wrote=%d read=%d\n",
               i, (int)wbuf[i], (int)rbuf[i]);
        return;
      }
    }
    printf("[OSTests] Non-block-aligned write/read passed.\n");
  }

  // Test 2: Indirect addressing (write beyond 10 direct blocks)
  {
    char fname2[] = "indirect.bin";
    uint32 h2 = DfsInodeOpen(fname2);
    if (h2 == DFS_FAIL) {
      printf("[OSTests] DfsInodeOpen('%s') failed.\n", fname2);
      return;
    }
    // Write 12 * 1024 bytes (exceeds 10 direct blocks)
    int total = 12 * 1024;
    char *largew = (char *)MemoryAlloc(total);
    char *larger = (char *)MemoryAlloc(total);
    if (!largew || !larger) {
      printf("[OSTests] MemoryAlloc failed for indirect test.\n");
      return;
    }
    int i;
    for (i = 0; i < total; i++) largew[i] = (char)((i * 13) & 0xFF);
    ret = DfsInodeWriteBytes(h2, largew, 0, total);
    if (ret == DFS_FAIL || ret != total) {
      printf("[OSTests] Indirect write failed: ret=%d\n", ret);
      return;
    }
    ret = DfsInodeReadBytes(h2, larger, 0, total);
    if (ret == DFS_FAIL || ret != total) {
      printf("[OSTests] Indirect read failed: ret=%d\n", ret);
      return;
    }
    for (i = 0; i < total; i++) {
      if (largew[i] != larger[i]) {
        printf("[OSTests] MISMATCH indirect.bin at byte %d: w=%d r=%d\n",
               i, (int)largew[i], (int)larger[i]);
        return;
      }
    }
    printf("[OSTests] Indirect addressing write/read passed.\n");
    MemoryFree(largew);
    MemoryFree(larger);
  }

  // Test 3: Persistence across close/reopen
  {
    char fname3[] = "persist.bin";
    uint32 h3 = DfsInodeOpen(fname3);
    if (h3 == DFS_FAIL) {
      printf("[OSTests] DfsInodeOpen('%s') failed.\n", fname3);
      return;
    }
    char marker[64];
    int i;
    for (i = 0; i < (int)sizeof(marker); i++) marker[i] = (char)(0xA5 ^ i);
    ret = DfsInodeWriteBytes(h3, marker, 123, sizeof(marker));
    if (ret == DFS_FAIL || ret != (int)sizeof(marker)) {
      printf("[OSTests] Persistence write failed: ret=%d\n", ret);
      return;
    }
    // Close and reopen filesystem to force metadata/data to disk
    if (DfsCloseFileSystem() == DFS_FAIL) {
      printf("[OSTests] DfsCloseFileSystem failed.\n");
      return;
    }
    if (DfsOpenFileSystem() == DFS_FAIL) {
      printf("[OSTests] DfsOpenFileSystem after close failed.\n");
      return;
    }
    // Reopen inode by name and read back
    uint32 h3b = DfsInodeOpen(fname3);
    if (h3b == DFS_FAIL) {
      printf("[OSTests] DfsInodeOpen('%s') after reopen failed.\n", fname3);
      return;
    }
    char check[64];
    ret = DfsInodeReadBytes(h3b, check, 123, sizeof(check));
    if (ret == DFS_FAIL || ret != (int)sizeof(check)) {
      printf("[OSTests] Persistence read failed: ret=%d\n", ret);
      return;
    }
    for (i = 0; i < (int)sizeof(check); i++) {
      if (check[i] != (char)(0xA5 ^ i)) {
        printf("[OSTests] Persistence MISMATCH at byte %d: got=%d expected=%d\n",
               i, (int)check[i], (int)((char)(0xA5 ^ i)));
        return;
      }
    }
    printf("[OSTests] Persistence test passed.\n");
  }

  printf("[OSTests] DFS driver tests completed.\n");
}

