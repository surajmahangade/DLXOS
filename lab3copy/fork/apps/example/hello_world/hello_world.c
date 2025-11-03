#include "usertraps.h"
#include "misc.h"

/*
 * ==========================================================================
 * Copy-On-Write (COW) Test Program
 * ==========================================================================
 * This program verifies correct Copy-on-Write behavior in the OS by:
 *   1. Ensuring page table entries are marked read-only after fork()
 *   2. Triggering TRAP_ROP_ACCESS on writes to shared pages
 *   3. Confirming parent and child get independent memory after writing
 * ==========================================================================
 */

#define PAGE_SIZE   4096
#define PAGE_SHIFT  12

void main(int argc, char *argv[])
{
  sem_t s_procs_completed;
  int pid;
  int i;

  // Variables in different memory regions
  static int data_var1 = 11111111;   // Data segment
  static int data_var2 = 22222222;   // Data segment
  int stack_var1 = 33333333;         // Stack
  int stack_var2 = 44444444;         // Stack

  if (argc != 2) {
    Printf("Usage: %s <handle_to_procs_completed_semaphore>\n", argv[0]);
    Exit();
  }

  s_procs_completed = dstrtol(argv[1], NULL, 10);

  Printf("\n========================================\n");
  Printf("COW Test - PID %d\n", getpid());
  Printf("========================================\n\n");

  Printf("BEFORE FORK:\n");
  Printf("  data_var1 (page %d) = %d\n", ((unsigned int)&data_var1) >> PAGE_SHIFT, data_var1);
  Printf("  data_var2 (page %d) = %d\n", ((unsigned int)&data_var2) >> PAGE_SHIFT, data_var2);
  Printf("  stack_var1 (page %d) = %d\n", ((unsigned int)&stack_var1) >> PAGE_SHIFT, stack_var1);
  Printf("  stack_var2 (page %d) = %d\n\n", ((unsigned int)&stack_var2) >> PAGE_SHIFT, stack_var2);

  Printf("Calling fork()...\n\n");
  pid = fork();

  if (pid < 0) {
    Printf("Fork failed!\n");
    Exit();
  }

  // -------------------- CHILD PROCESS --------------------
  if (pid == 0) {
    Printf("========================================\n");
    Printf("CHILD PROCESS (PID %d)\n", getpid());
    Printf("========================================\n\n");

    // Simple delay to separate output timing
    for (i = 0; i < 5000000; i++);

    Printf("CHILD: Values after fork (before writes):\n");
    Printf("  data_var1 = %d\n", data_var1);
    Printf("  data_var2 = %d\n", data_var2);
    Printf("  stack_var1 = %d\n", stack_var1);
    Printf("  stack_var2 = %d\n\n", stack_var2);

    Printf("CHILD: Writing to shared pages...\n");
    Printf("(Expect TRAP_ROP_ACCESS before each copy)\n\n");

    data_var1 = -111111111;
    Printf("CHILD: Wrote data_var1 (page %d) = %d\n\n", ((unsigned int)&data_var1) >> PAGE_SHIFT, data_var1);

    data_var2 = -222222222;
    Printf("CHILD: Wrote data_var2 (page %d) = %d\n\n", ((unsigned int)&data_var2) >> PAGE_SHIFT, data_var2);

    stack_var1 = -333333333;
    Printf("CHILD: Wrote stack_var1 (page %d) = %d\n\n", ((unsigned int)&stack_var1) >> PAGE_SHIFT, stack_var1);

    stack_var2 = -444444444;
    Printf("CHILD: Wrote stack_var2 (page %d) = %d\n\n", ((unsigned int)&stack_var2) >> PAGE_SHIFT, stack_var2);

    Printf("CHILD: Final values (should all be negative):\n");
    Printf("  data_var1 = %d\n", data_var1);
    Printf("  data_var2 = %d\n", data_var2);
    Printf("  stack_var1 = %d\n", stack_var1);
    Printf("  stack_var2 = %d\n\n", stack_var2);

  // -------------------- PARENT PROCESS --------------------
  } else {
    Printf("========================================\n");
    Printf("PARENT PROCESS (PID %d, Child PID %d)\n", getpid(), pid);
    Printf("========================================\n\n");

    Printf("PARENT: Values after fork (before writes):\n");
    Printf("  data_var1 = %d\n", data_var1);
    Printf("  data_var2 = %d\n", data_var2);
    Printf("  stack_var1 = %d\n", stack_var1);
    Printf("  stack_var2 = %d\n\n", stack_var2);

    Printf("PARENT: Writing to shared pages...\n");
    Printf("(Expect TRAP_ROP_ACCESS before each copy)\n\n");

    data_var1 = 999999999;
    Printf("PARENT: Wrote data_var1 (page %d) = %d\n\n", ((unsigned int)&data_var1) >> PAGE_SHIFT, data_var1);

    data_var2 = 888888888;
    Printf("PARENT: Wrote data_var2 (page %d) = %d\n\n", ((unsigned int)&data_var2) >> PAGE_SHIFT, data_var2);

    stack_var1 = 777777777;
    Printf("PARENT: Wrote stack_var1 (page %d) = %d\n\n", ((unsigned int)&stack_var1) >> PAGE_SHIFT, stack_var1);

    stack_var2 = 666666666;
    Printf("PARENT: Wrote stack_var2 (page %d) = %d\n\n", ((unsigned int)&stack_var2) >> PAGE_SHIFT, stack_var2);

    Printf("PARENT: Final values (should all be positive):\n");
    Printf("  data_var1 = %d\n", data_var1);
    Printf("  data_var2 = %d\n", data_var2);
    Printf("  stack_var1 = %d\n", stack_var1);
    Printf("  stack_var2 = %d\n\n", stack_var2);

    // Wait a bit to let child finish printing
    for (i = 0; i < 10000000; i++);
  }

  // -------------------- COMMON END --------------------
  Printf("========================================\n");
  Printf("Process %d: COW Test Complete!\n", getpid());
  Printf("========================================\n\n");

  Printf("SUMMARY:\n");
  Printf(" - TRAP_ROP_ACCESS should have been triggered\n");
  Printf(" - Parent holds positive values\n");
  Printf(" - Child holds negative values\n");
  Printf(" - Independent pages confirm COW success\n\n");

  if (sem_signal(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Error: Failed to signal semaphore!\n");
    Exit();
  }

  Printf("Process %d: Exiting cleanly.\n", getpid());
}
