#include "lab2-api.h"
#include "usertraps.h"
#include "misc.h"
// #include <q2/include/spawn.h>
#include "spawn.h"
// #include <string.h>

void Consumer(mem_buffer *mc, int process_id, char *final_string);
unsigned int my_pid;
char current_item;
int i;
int j;
int chars_consumed = 0;
char expected_char = '0';  // Start expecting '0' for "0123456789"

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

// #define MESSAGE "0123456789"
void main (int argc, char *argv[])
{
  mem_buffer *mc;
  uint32 h_mem;
  sem_t s_procs_completed;
  char final_string[strlen(MESSAGE)];  // Store consumed results
  
  // Initialize final_string
  int i;
  for (i = 0; i < strlen(MESSAGE); i++) {
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
//   Printf("consumer: This is process with PID %d\n", my_pid);

  // Producer and Consumer work together
  while (strlen(final_string) < strlen(MESSAGE)) {
      Consumer(mc, my_pid, final_string);
  }
  
  Printf("consumer: PID %d Consumed : %d chars\n", my_pid, chars_consumed);

  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }
}


void Consumer(mem_buffer *mc, int process_id, char *final_string) {
  int message_len = strlen(MESSAGE); 
  // Printf("Consumer %d: Trying to consume. Chars consumed so far: %d, waiting for: %c\n", process_id, chars_consumed, expected_char);
  sem_wait(mc->full);  // Wait for available item
  // Printf("Consumer %d: Passed full semaphore. Chars consumed so far: %d\n", process_id, chars_consumed);
  if (chars_consumed < message_len) {
    lock_acquire(mc->buffer_lock);
    
    // Check if the buffer is empty
    if (mc->start == mc->end) {
      Printf("something wrong: buffer empty\n");
      lock_release(mc->buffer_lock);
      // Small delay before retrying
      return;
    }
    
    current_item = mc->buffer[mc->start];
    
    // Sequential check
    if (current_item != expected_char) {
      lock_release(mc->buffer_lock);
      sem_signal(mc->full);  // Put the item back for correct consumer
      return;  // Stop consuming on sequential error
    }
    
    // Remove character from buffer
    mc->start = (mc->start + 1) % BUFFER_SIZE;

    // Store in final string
    final_string[chars_consumed] = current_item;
    

    
    // Consumer pid removed: char
    Printf("Consumer %d removed: %c\n", process_id, current_item);
    
    // Update expected character for next iteration
    expected_char = current_item + 1;
    // consumed = 1;
    chars_consumed++;
    
    sem_signal(mc->empty);  // Signal that a slot is free
    lock_release(mc->buffer_lock);
  }

}