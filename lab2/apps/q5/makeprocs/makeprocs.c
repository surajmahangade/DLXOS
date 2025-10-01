#include "lab2-api.h"
#include "usertraps.h"
#include "misc.h"
#include "spawn.h"

void main (int argc, char *argv[])
{
  int num_N3 = 0;
  int num_H2O = 0;
  int num_N_atoms;
  int num_O2_molecules;
  int num_NO2_molecules;
  mem_buffer *mc;
  uint32 h_mem;
  sem_t s_procs_completed;
  char h_mem_str[10];
  char s_procs_completed_str[10];
  char num_N3_str[10];
  char num_H2O_str[10];
  char num_N_atoms_str[10];
  char num_O2_molecules_str[10];
  char num_NO2_molecules_str[10];

  if (argc != 3) {
    Printf("Usage: "); Printf(argv[0]); Printf(" <number of N3 injected> <number of H2O injected>\n");
    Exit();
  }

  num_N3 = dstrtol(argv[1], NULL, 10);
  Printf("Injected %d N3\n", num_N3);
  num_H2O = dstrtol(argv[2], NULL, 10);
  Printf("Injected %d H2O\n", num_H2O);

  // Calculate expected atoms/molecules for consumer processes
  num_N_atoms = num_N3 * 3;  // Each N3 produces 3 N atoms
  num_O2_molecules = num_H2O / 2;  // Each 2 H2O produces 1 O2
  num_NO2_molecules = (num_N_atoms < num_O2_molecules) ? num_N_atoms : num_O2_molecules;

  if ((h_mem = shmget()) == 0) {
    Printf("ERROR: could not allocate shared memory page in "); Printf(argv[0]); Printf(", exiting...\n");
    Exit();
  }

  if ((mc = (mem_buffer *)shmat(h_mem)) == NULL) {
    Printf("Could not map the shared page to virtual address in "); Printf(argv[0]); Printf(", exiting..\n");
    Exit();
  }

  mc->N3 = sem_create(0);
  mc->H2O = sem_create(0);
  mc->H2 = sem_create(0);
  mc->O2 = sem_create(0);
  mc->N = sem_create(0);
  mc->NO2 = sem_create(0);
  
  // 5 processes total (2 injectors + 3 reactions)
  if ((s_procs_completed = sem_create(-4)) == SYNC_FAIL) {
    Printf("Bad sem_create in "); Printf(argv[0]); Printf("\n");
    Exit();
  }

  ditoa(h_mem, h_mem_str);
  ditoa(s_procs_completed, s_procs_completed_str);
  ditoa(num_N3, num_N3_str);
  ditoa(num_H2O, num_H2O_str);
  ditoa(num_N_atoms, num_N_atoms_str);
  ditoa(num_O2_molecules, num_O2_molecules_str);
  ditoa(num_NO2_molecules, num_NO2_molecules_str);

  // Create 5 processes
  process_create(REACTION1, h_mem_str, s_procs_completed_str, num_N3_str, NULL);
  process_create(REACTION2, h_mem_str, s_procs_completed_str, num_H2O_str, NULL);
  process_create(REACTION3, h_mem_str, s_procs_completed_str, num_N3_str, NULL);
  process_create(REACTION4, h_mem_str, s_procs_completed_str, num_O2_molecules_str, NULL);
  process_create(REACTION5, h_mem_str, s_procs_completed_str, num_NO2_molecules_str, NULL);

  if (sem_wait(s_procs_completed) != SYNC_SUCCESS) {
    Printf("Bad semaphore s_procs_completed (%d) in ", s_procs_completed); Printf(argv[0]); Printf("\n");
    Exit();
  }
  Printf("All other processes completed, exiting main process.\n");
}