#ifndef __USERPROG__
#define __USERPROG__
#include "lab2-api.h"

typedef struct missile_code {
  int numprocs;
  char really_important_char;
} missile_code;

#define BUFFER_SIZE 10
#define MESSAGE "0123456789"

typedef struct mem_buffer {
  char buffer[BUFFER_SIZE];
  int start;
  int end;
  int count;
  lock_t buffer_lock;
} mem_buffer;

#define FILENAME_TO_RUN "spawn_me.dlx.obj"
#endif
