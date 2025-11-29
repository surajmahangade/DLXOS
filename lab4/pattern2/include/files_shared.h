#ifndef __FILES_SHARED__
#define __FILES_SHARED__

#define FILE_SEEK_SET 1
#define FILE_SEEK_END 2
#define FILE_SEEK_CUR 3

#define FILE_MAX_FILENAME_LENGTH 72 //76

#define FILE_MAX_READWRITE_BYTES 4096

static lock_t file_lock;

typedef struct file_descriptor {
  // STUDENT: put file descriptor info here
  char filename[FILE_MAX_FILENAME_LENGTH];
  int inuse;
  uint32 inode; // inode handle
  int eof;
  char mode[2]; // 'r', 'w', 'a'
  uint32 current_pos;
  int pid;
} file_descriptor;

#define FILE_FAIL -1
#define FILE_EOF -1
#define FILE_SUCCESS 1

#endif
