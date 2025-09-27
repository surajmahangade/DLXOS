#ifndef __USERPROG__
#define __USERPROG__
#include "lab2-api.h"

typedef struct missile_code {
  int numprocs;
  char really_important_char;
} missile_code;

#define BUFFER_SIZE 4
#define MESSAGE "0123456789"

typedef struct mem_buffer {
  char buffer[BUFFER_SIZE];
  int start;
  int end;
  int count;
  lock_t buffer_lock;
} mem_buffer;

#define CONSUMER_TO_RUN "consumer.dlx.obj"
#define PRODUCER_TO_RUN "producer.dlx.obj"
#endif
