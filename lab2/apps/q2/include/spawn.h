#ifndef __USERPROG__
#define __USERPROG__

typedef struct missile_code {
  int numprocs;
  char really_important_char;
} missile_code;


typedef struct mem_buffer {
  char buffer[10];
  int start;
  int end;
  int count;
  lock_t buffer_lock;
} mem_buffer;

#define FILENAME_TO_RUN "spawn_me.dlx.obj"

#endif
