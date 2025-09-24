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

int strlen (const char *s)
{
  int		i = 0;

  while (*(s++) != '\0') {
    i++;
  }
  return (i);
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

  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }
}

void Producer(mem_buffer *mc, int process_id) {
  lock_acquire(mc->buffer_lock);
  
  // Check if we still need to produce more characters globally
  if (mc->chars_produced >= strlen(MESSAGE)) {
    lock_release(mc->buffer_lock);
    return;
  }
  
  // Check if the buffer is full
  if ((mc->end + 1) % BUFFER_SIZE == mc->start) {
    Printf("Producer %d: Buffer full, waiting...\n", process_id);
    lock_release(mc->buffer_lock);
    return;
  }
  
  // Get the next character to produce based on global counter
  item = MESSAGE[mc->chars_produced];
  
  // Add the character to the buffer
  mc->buffer[mc->end] = item;
  mc->end = (mc->end + 1) % BUFFER_SIZE;
  mc->chars_produced++;  // Increment global production counter
  
  Printf("Producer %d: Produced '%c' (global count: %d/%d)\n", 
         process_id, item, mc->chars_produced, strlen(MESSAGE));
  
  lock_release(mc->buffer_lock);
}

void Consumer(mem_buffer *mc, int process_id, char *final_string) {
  lock_acquire(mc->buffer_lock);
  
  // Check if we've consumed all characters globally
  if (mc->chars_consumed >= strlen(MESSAGE)) {
    lock_release(mc->buffer_lock);
    return;
  }
  
  // Check if the buffer is empty
  if (mc->start == mc->end) {
    Printf("Consumer %d: Buffer empty, waiting...\n", process_id);
    lock_release(mc->buffer_lock);
    return;
  }
  
  current_item = mc->buffer[mc->start];
  
  // Sequential check based on global consumption counter
  char expected_char = '0' + mc->chars_consumed;
  if (current_item != expected_char) {
    Printf("Consumer %d: ERROR - Sequential check failed!\n", process_id);
    Printf("Consumer %d: Expected '%c' (ASCII %d), got '%c' (ASCII %d)\n", 
           process_id, expected_char, expected_char, current_item, current_item);
    Printf("Consumer %d: Global chars consumed: %d\n", process_id, mc->chars_consumed);
    lock_release(mc->buffer_lock);
    return;  // Stop consuming on sequential error
  }
  
  // Remove character from buffer
  mc->start = (mc->start + 1) % BUFFER_SIZE;
  
  // Store in local final string (each process keeps its own copy of what it consumed)
  final_string[strlen(final_string)] = current_item;
  mc->chars_consumed++;  // Increment global consumption counter
  
  Printf("Consumer %d: Consumed '%c' (global count: %d/%d) - Sequential check: PASS\n", 
         process_id, current_item, mc->chars_consumed, strlen(MESSAGE));
  
  lock_release(mc->buffer_lock);
  
  // Check if transfer is complete globally
  if (mc->chars_consumed == strlen(MESSAGE)) {
    Printf("Consumer %d: SUCCESS - Global transfer complete!\n", process_id);
    Printf("Consumer %d: Original MESSAGE: %s (length %d)\n", process_id, MESSAGE, strlen(MESSAGE));
    Printf("Consumer %d: This process consumed: %s (length %d)\n", process_id, final_string, strlen(final_string));
    
    Printf("Consumer %d: Final result - All processes collectively transferred: ", process_id);
    for (i = 0; i < strlen(MESSAGE); i++) {
      Printf("%c", MESSAGE[i]);
    }
    Printf("\n");
  }
}