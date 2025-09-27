#include "lab2-api.h"
#include "usertraps.h"
#include "misc.h"
// #include <q2/include/spawn.h>
#include "spawn.h"
// #include <string.h>

void Consumer(mem_buffer *mc, int process_id, char *final_string);
void Producer(mem_buffer *mc, int process_id);
unsigned int my_pid;
char item;
char current_item;
int i;
int j;
int chars_produced = 0;
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

  // Producer and Consumer work together
  while (chars_produced >= strlen(MESSAGE) && strlen(final_string) >= strlen(MESSAGE)) {
    if (chars_produced  < strlen(MESSAGE)){
      Producer(mc, my_pid);
    }
    if (strlen(final_string) < strlen(MESSAGE)){
      Consumer(mc, my_pid, final_string);
    }
    // sleep(5);  // Small delay to allow other process to run
    Printf("spawn_me: PID %d intermediate final_string: %s\n", my_pid, final_string);
  }
  
  // Printf("spawn_me: PID %d transfer complete. Final string: %s\n", my_pid, final_string);
  Printf("spawn_me: PID %d Final string length: %d, MESSAGE length: %d\n", my_pid,
         strlen(final_string), strlen(MESSAGE));

  if(sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }
}

void Producer(mem_buffer *mc, int process_id) {
  int message_len = strlen(MESSAGE);
  

  // int inserted;
  if (chars_produced < message_len) {
    lock_acquire(mc->buffer_lock);
    
    // Check if the buffer is full
    if ((mc->end + 1) % BUFFER_SIZE == mc->start) {
      // Printf("Producer %d: Buffer full, waiting...\n", process_id);
      lock_release(mc->buffer_lock);
      return;
    }
    
    // Get character from MESSAGE
    item = MESSAGE[chars_produced];
    // Printf("Producer %d: Starting to produce %d characters "\n", process_id, message_len);
    // Add the character to the buffer
    mc->buffer[mc->end] = item;
    mc->end = (mc->end + 1) % BUFFER_SIZE;
    // inserted = 1;
    chars_produced++;
    
    Printf("Producer %d: Produced '%c' (%d/%d)\n", 
           process_id, item, chars_produced, message_len);
    
    lock_release(mc->buffer_lock);
    // Printf("Producer %d: Finished producing %c\n", process_id, item);
  }
  // else {
  //   Printf("Producer %d: All characters produced.\n", process_id);
  // }
}

void Consumer(mem_buffer *mc, int process_id, char *final_string) {
  int message_len = strlen(MESSAGE); 
  
  // int consumed;
  if (chars_consumed < message_len) {
    lock_acquire(mc->buffer_lock);
    
    // Check if the buffer is empty
    if (mc->start == mc->end) {
      // Printf("Consumer %d: Buffer empty, waiting for character : %c\n", process_id, expected_char);
      lock_release(mc->buffer_lock);
      // Small delay before retrying
      return;
    }
    
    current_item = mc->buffer[mc->start];
    
    // Sequential check
    if (current_item != expected_char) {
      // Printf("Consumer %d: ERROR - Sequential check failed!\n", process_id);
      // Printf("Consumer %d: Expected '%c' (ASCII %d), got '%c' (ASCII %d)\n", 
      //        process_id, expected_char, expected_char, current_item, current_item);
      // Printf("Consumer %d: Characters consumed so far: %s\n", process_id, final_string);
      lock_release(mc->buffer_lock);
      // print the entire buffer
      // Printf("Current Buffer State: ");
      // for (i = mc->start; i < BUFFER_SIZE; i++) {
      //   Printf("%c ", mc->buffer[i]);
      // }
      // if (mc->end < mc->start) {
      //   for (i = 0; i < mc->end; i++) {
      //     Printf("%c ", mc->buffer[i]);
      //   }
      // }
      // Printf("\n");
      // sleep(5);  // Allow time for debugging
      return;  // Stop consuming on sequential error
    }
    
    // Remove character from buffer
    mc->start = (mc->start + 1) % BUFFER_SIZE;

    // Printf("Consumer %d: Current Buffer State after consume: start: %d, end: %d, count: %d\n", 
    //        process_id, mc->start, mc->end, mc->count);
    // for (i = mc->start; i < BUFFER_SIZE; i++) {
    //     Printf("%c ", mc->buffer[i]);
    //   }
    //   if (mc->end < mc->start) {
    //     for (i = 0; i < mc->end; i++) {
    //       Printf("%c ", mc->buffer[i]);
    //     }
    //   }
    // Printf("\n");
    // Store in final string
    final_string[chars_consumed] = current_item;
    

    
    Printf("Consumer %d: Consumed '%c' (%d/%d) , expected %c - Sequential check: PASS\n", 
           process_id, current_item, chars_consumed + 1, message_len, expected_char);
    
    // Update expected character for next iteration
    expected_char = current_item + 1;
    // consumed = 1;
    chars_consumed++;
    
    lock_release(mc->buffer_lock);
  }
  
  // Final verification
//   if (strlen(final_string) == strlen(MESSAGE)) {
//     lock_acquire(mc->buffer_lock);
//     Printf("Consumer %d: SUCCESS - Transfer complete! Final string length matches MESSAGE length\n", process_id);
//     Printf("Consumer %d: Original MESSAGE: %s (length %d)\n", process_id, MESSAGE, strlen(MESSAGE));
//     // Printf("Consumer %d: Final string:    %s (length %d)\n", process_id, final_string, strlen(final_string));
    
//   //print the final string
//     Printf("The Final String is: ");
//     for (i = 0; i < strlen(final_string); i++) {
//       Printf("%c", final_string[i]);
//     }
//     Printf("\n");
//     lock_release(mc->buffer_lock);
// }
}