#include "usertraps.h"
#include "misc.h"

// Local copies of needed file constants to avoid extra header dependencies
#define FILE_SEEK_SET 1
#define FILE_FAIL -1

void main (int argc, char *argv[]) {
  int handle;
  char writebuf[] = "hello dlx file api";
  char readbuf[64];
  int n;

  Printf("[filetest] starting test...\n");

  // Open in write mode, write, close
  handle = file_open("testfile.txt", "w");
  if (handle == FILE_FAIL) {
    Printf("[filetest] file_open(w) failed\n");
    Exit();
  }
  n = file_write(handle, writebuf, dstrlen(writebuf));
  if (n == FILE_FAIL) {
    Printf("[filetest] file_write failed\n");
    Exit();
  }
  Printf("[filetest] wrote %d bytes\n", n);
  if (file_close(handle) == FILE_FAIL) {
    Printf("[filetest] file_close failed\n");
    Exit();
  }

  // Reopen in read mode, read back, verify
  handle = file_open("testfile.txt", "r");
  if (handle == FILE_FAIL) {
    Printf("[filetest] file_open(r) failed\n");
    Exit();
  }
  bzero(readbuf, sizeof(readbuf));
  n = file_read(handle, readbuf, sizeof(readbuf)-1);
  if (n == FILE_FAIL) {
    Printf("[filetest] file_read failed\n");
    Exit();
  }
  Printf("[filetest] read %d bytes: '%s'\n", n, readbuf);

  // Seek to beginning and read again to ensure seek clears eof
  if (file_seek(handle, 0, FILE_SEEK_SET) == FILE_FAIL) {
    Printf("[filetest] file_seek failed\n");
    Exit();
  }
  bzero(readbuf, sizeof(readbuf));
  n = file_read(handle, readbuf, sizeof(readbuf)-1);
  if (n == FILE_FAIL) {
    Printf("[filetest] file_read after seek failed\n");
    Exit();
  }
  Printf("[filetest] read after seek %d bytes: '%s'\n", n, readbuf);

  if (file_close(handle) == FILE_FAIL) {
    Printf("[filetest] file_close (final) failed\n");
    Exit();
  }

  // Delete file
  if (file_delete("testfile.txt") == FILE_FAIL) {
    Printf("[filetest] file_delete failed\n");
    Exit();
  }

  Printf("[filetest] test completed successfully.\n");
  // Ensure graceful shutdown so DFS superblock is revalidated
  Exit();
}
