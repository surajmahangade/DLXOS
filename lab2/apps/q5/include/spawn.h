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
  sem_t N3; // number of nitrogen
  sem_t H2; // number of hydrogen
  sem_t H2O; // number of water
  sem_t O2; // number of oxygen 
  sem_t N; // number of nitrogen atom
  sem_t NO2; // number of nitrogen dioxide
} mem_buffer;


// #define CONSUMER_TO_RUN "consumer.dlx.obj"
// #define PRODUCER_TO_RUN "producer.dlx.obj"
#define REACTION1 "reaction1.dlx.obj" // injetion of N3
#define REACTION2 "reaction2.dlx.obj" // injetion of H2O
#define REACTION3 "reaction3.dlx.obj" // N3 -> N + N + N
#define REACTION4 "reaction4.dlx.obj" // 2 H2O -> 2 H2 + O2
#define REACTION5 "reaction5.dlx.obj" // N + O2 -> NO2

#endif
