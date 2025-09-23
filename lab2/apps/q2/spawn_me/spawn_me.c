#include "lab2-api.h"
#include "usertraps.h"
#include "misc.h"
#include <q2/include/spawn.h>

void Consumer(mem_buffer *mc, int process_id, char *final_string);
void Producer(mem_buffer *mc, int process_id);

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

  int my_pid = Getpid();
  Printf("spawn_me: This is process with PID %d\n", my_pid);

  // Producer and Consumer work together
  while (strlen(final_string) < strlen(MESSAGE)) {
    Producer(mc, my_pid);
    Consumer(mc, my_pid, final_string);
  }
  
  Printf("spawn_me: PID %d transfer complete. Final string: %s\n", my_pid, final_string);
  Printf("spawn_me: Final string length: %d, MESSAGE length: %d\n", 
         strlen(final_string), strlen(MESSAGE));

  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }
}

void Producer(mem_buffer *mc, int process_id) {
  int message_len = strlen(MESSAGE);
  int chars_produced = 0;
  
  Printf("Producer %d: Starting to produce %d characters from \"0123456789\"\n", process_id, message_len);
  int inserted;
  if (chars_produced < message_len) {
    lock_acquire(mc->buffer_lock);
    
    // Check if the buffer is full
    if ((mc->end + 1) % BUFFER_SIZE == mc->start) {
      Printf("Producer %d: Buffer full, waiting...\n", process_id);
      lock_release(mc->buffer_lock);
    }
    
    // Get character from MESSAGE
    char item = MESSAGE[chars_produced];
    
    // Add the character to the buffer
    mc->buffer[mc->end] = item;
    mc->end = (mc->end + 1) % BUFFER_SIZE;
    inserted = 1;
    chars_produced++;
    
    Printf("Producer %d: Produced '%c' (%d/%d)\n", 
           process_id, item, chars_produced, message_len);
    
    lock_release(mc->buffer_lock);
  }
  
  Printf("Producer %d: Finished producing all characters\n", process_id);
}

void Consumer(mem_buffer *mc, int process_id, char *final_string) {
  int message_len = strlen(MESSAGE); 
  int chars_consumed = 0;
  char expected_char = '0';  // Start expecting '0' for "0123456789"
  
  int consumed;
  if (chars_consumed < message_len) {
    lock_acquire(mc->buffer_lock);
    
    // Check if the buffer is empty
    if (mc->start == mc->end) {
      Printf("Consumer %d: Buffer empty, waiting...\n", process_id);
      lock_release(mc->buffer_lock);
      // Small delay before retrying
    }
    
    char current_item = mc->buffer[mc->start];
    
    // Sequential check
    if (current_item != expected_char) {
      Printf("Consumer %d: ERROR - Sequential check failed!\n", process_id);
      Printf("Consumer %d: Expected '%c' (ASCII %d), got '%c' (ASCII %d)\n", 
             process_id, expected_char, expected_char, current_item, current_item);
      Printf("Consumer %d: Characters consumed so far: %s\n", process_id, final_string);
      lock_release(mc->buffer_lock);
      return;  // Stop consuming on sequential error
    }
    
    // Remove character from buffer
    mc->start = (mc->start + 1) % BUFFER_SIZE;
    
    // Store in final string
    final_string[chars_consumed] = current_item;
    
    Printf("Consumer %d: Consumed '%c' (%d/%d) - Sequential check: PASS\n", 
           process_id, current_item, chars_consumed + 1, message_len);
    
    // Update expected character for next iteration
    expected_char = current_item + 1;
    consumed = 1;
    chars_consumed++;
    
    lock_release(mc->buffer_lock);
  }
  
  // Final verification
  if (strlen(final_string) == strlen(MESSAGE)) {
    Printf("Consumer %d: SUCCESS - Transfer complete! Final string length matches MESSAGE length\n", process_id);
    Printf("Consumer %d: Original MESSAGE: %s (length %d)\n", process_id, MESSAGE, strlen(MESSAGE));
    Printf("Consumer %d: Final string:    %s (length %d)\n", process_id, final_string, strlen(final_string));
    
  //print the final string
    Printf("The Final String is: ");
    for (int i = 0; i < strlen(final_string); i++) {
      Printf("%c", final_string[i]);
    }
    Printf("\n");
}
}