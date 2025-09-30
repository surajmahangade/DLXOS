#include "lab2-api.h"
#include "usertraps.h"
#include "misc.h"
#include "spawn.h"

void Consumer(mem_buffer *mc, int process_id, char *final_string);
void Producer(mem_buffer *mc, int process_id);
unsigned int my_pid;
char item;
char current_item;
int i;
int j;

int strlen (const char *s)
{
  int		i = 0;

  while (*(s++) != '\0') {
    i++;
  }
  return (i);
}

void sleep(int seconds) {
  //simulate sleep
  // for (j = 0; j < seconds * 1000000; j++);
}

void main (int argc, char *argv[])
{
  mem_buffer *mc;
  uint32 h_mem;
  sem_t s_procs_completed;
  char final_string[BUFFER_SIZE];  // Store consumed results
  
  // Initialize final_string
  int i;
  for (i = 0; i < BUFFER_SIZE; i++) {
    final_string[i] = '\0';
  }

  if (argc != 3) { 
    Printf("Usage: "); Printf(argv[0]); Printf(" <handle_to_shared_memory_page> <handle_to_page_mapped_semaphore>\n"); 
    Exit();
  } 

  // Convert the command-line strings into integers for use as handles
  h_mem = dstrtol(argv[1], NULL, 10);
  s_procs_completed = dstrtol(argv[2], NULL, 10);

  // Map shared memory page into this process's memory space
  if ((mc = (mem_buffer *)shmat(h_mem)) == NULL) {
    Printf("Could not map the virtual address to the memory in "); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }

  my_pid = Getpid();
  Printf("spawn_me: This is process with PID %d\n", my_pid);

  // Each process contributes to collective production and consumption
  // Continue until the entire MESSAGE has been transferred by all processes collectively
  while (mc->chars_produced < strlen(MESSAGE) || mc->chars_consumed < strlen(MESSAGE)) {
    // Try to produce if there are still characters to produce
    if (mc->chars_produced < strlen(MESSAGE)) {
      Producer(mc, my_pid);
    }
    
    // Try to consume if there are characters available
    if (mc->chars_consumed < strlen(MESSAGE)) {
      Consumer(mc, my_pid, final_string);
    }
  }
  
  Printf("spawn_me: PID %d finished its work. Chars produced globally: %d, Chars consumed globally: %d\n", 
         my_pid, mc->chars_produced, mc->chars_consumed);
  Printf("spawn_me: Final string length: %d, MESSAGE length: %d\n", 
         strlen(final_string), strlen(MESSAGE));

  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }
}
void Producer(mem_buffer *mc, int process_id) {
  int message_len = strlen(MESSAGE);
  
  sem_wait(mc->empty);  // Wait for empty slot
  
  // Check if we still need to produce
  if (mc->chars_produced >= message_len) {
    sem_signal(mc->empty);  // Release the semaphore we just took
    return;
  }
  
  // Check if buffer is full
  if ((mc->end + 1) % BUFFER_SIZE == mc->start) {
    sem_signal(mc->empty);  // Release and try again later
    return;
  }
  
  // Produce
  item = MESSAGE[mc->chars_produced];
  mc->buffer[mc->end] = item;
  mc->end = (mc->end + 1) % BUFFER_SIZE;
  mc->chars_produced++;
  
  Printf("Producer %d: Produced '%c'\n", process_id, item);
  
  sem_signal(mc->full);  // Signal that there's a full slot
}

void Consumer(mem_buffer *mc, int process_id, char *final_string) {
  sem_wait(mc->full);  // Wait for full slot
  
  // Check if we're done consuming
  if (mc->chars_consumed >= strlen(MESSAGE)) {
    sem_signal(mc->full);  // Release the semaphore
    return;
  }
  
  // Check if buffer is empty
  if (mc->start == mc->end) {
    sem_signal(mc->full);  // Release and try again
    return;
  }
  
  // Consume
  current_item = mc->buffer[mc->start];
  mc->start = (mc->start + 1) % BUFFER_SIZE;
  mc->chars_consumed++;
  
  Printf("Consumer %d: Consumed '%c'\n", process_id, current_item);
  
  sem_signal(mc->empty);  // Signal that there's an empty slot
}