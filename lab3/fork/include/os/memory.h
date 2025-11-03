#ifndef	_memory_h_
#define	_memory_h_

// Put all your #define's in memory_constants.h
#include "memory_constants.h"

extern int lastosaddress; // Defined in an assembly file
extern uint32 pagestart;  // Defined in memory.c

//--------------------------------------------------------
// Existing function prototypes:
//--------------------------------------------------------

int MemoryGetSize();
void MemoryModuleInit();
uint32 MemoryTranslateUserToSystem (PCB *pcb, uint32 addr);
int MemoryMoveBetweenSpaces (PCB *pcb, unsigned char *system, unsigned char *user, int n, int dir);
int MemoryCopySystemToUser (PCB *pcb, unsigned char *from, unsigned char *to, int n);
int MemoryCopyUserToSystem (PCB *pcb, unsigned char *from, unsigned char *to, int n);
int MemoryPageFaultHandler(PCB *pcb);

//---------------------------------------------------------
// Put your function prototypes here
//---------------------------------------------------------
// All function prototypes including the malloc and mfree functions go here
int MemoryAllocPage(void);
uint32 MemorySetupPte (uint32 page);
void MemoryFreePage(uint32 page);
void MemoryIncreaseRefcount(uint32 page);
void MemoryDecreaseRefcount(uint32 page);
uint32 MemoryGetRefcount(uint32 page);
int MemoryROPAccessHandler(PCB *pcb);

void *malloc(PCB *pcb, int size);
int mfree(PCB *pcb, void *ptr);

#endif	// _memory_h_
