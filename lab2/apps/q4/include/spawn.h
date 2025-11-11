#ifndef __USERPROG__
#define __USERPROG__
#include "lab2-api.h"

typedef struct missile_code {
  int numprocs;
  char really_important_char;
} missile_code;

#define BUFFER_SIZE 32
#define MESSAGE "0123456789"

typedef struct mem_buffer {
  char buffer[BUFFER_SIZE];
  int start;
  int end;
  cond_t notfull; // condition variable to indicate buffer is not full
  cond_t notempty; // condition variable to indicate buffer is not empty
  lock_t buffer_lock; // lock to protect access to buffer
} mem_buffer;


#define CONSUMER_TO_RUN "consumer.dlx.obj"
#define PRODUCER_TO_RUN "producer.dlx.obj"
#endif
