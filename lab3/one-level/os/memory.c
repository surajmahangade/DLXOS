//
//	memory.c
//
//	Routines for dealing with memory management.

//static char rcsid[] = "$Id: memory.c,v 1.1 2000/09/20 01:50:19 elm Exp elm $";

#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "memory.h"
#include "queue.h"

// num_pages = size_of_memory / size_of_one_page
static uint32 freemap[MEM_MAX_PAGES]; // Bitmap for free pages
static uint32 pagestart;
static int nfreepages;
static int freemapmax;


//----------------------------------------------------------------------
//
//	This silliness is required because the compiler believes that
//	it can invert a number by subtracting it from zero and subtracting
//	an additional 1.  This works unless you try to negate 0x80000000,
//	which causes an overflow when subtracted from 0.  Simply
//	trying to do an XOR with 0xffffffff results in the same code
//	being emitted.
//
//----------------------------------------------------------------------
static int negativeone = 0xFFFFFFFF;
static inline uint32 invert (uint32 n) {
  return (n ^ negativeone);
}

//----------------------------------------------------------------------
//
//	MemoryGetSize
//
//	Return the total size of memory in the simulator.  This is
//	available by reading a special location.
//
//----------------------------------------------------------------------
int MemoryGetSize() {
  return (*((int *)DLX_MEMSIZE_ADDRESS));
}


//----------------------------------------------------------------------
//
//	MemoryModuleInit
//
//	Initialize the memory module of the operating system.
//      Basically just need to setup the freemap for pages, and mark
//      the ones in use by the operating system as "VALID", and mark
//      all the rest as not in use.
//
//----------------------------------------------------------------------
void MemoryModuleInit() {
  int i;
  int memsize = MemoryGetSize();
  int os_pages;
  int total_pages;
  
  // Calculate where usable memory starts (after OS)
  pagestart = ((uint32)&lastosaddress + MEM_PAGESIZE - 1) & MEM_ADDRESS_OFFSET_MASK;
  
  // Calculate total available pages in the system
  total_pages = (memsize - pagestart) / MEM_PAGESIZE;
  
  // Can't have more pages than our maximum
  if (total_pages > MEM_MAX_PAGES) {
    total_pages = MEM_MAX_PAGES;
  }
  
  nfreepages = total_pages;
  freemapmax = total_pages;
  
  dbprintf('m', "MemoryModuleInit: memsize=0x%x, pagestart=0x%x, total_pages=%d\n",
      memsize, pagestart, total_pages);
  
  // Initialize freemap - all pages start as free
  for (i = 0; i < (MEM_MAX_PAGES / 32 + 1); i++) {
    freemap[i] = 0;
  }
  // Set bits for pages used by OS as "in use"
  os_pages = (pagestart) / MEM_PAGESIZE;
  for (i = 0; i < os_pages; i++) {
    freemap[i / 32] |= (1 << (i % 32));
    nfreepages--;
  }
  
  dbprintf('m', "MemoryModuleInit: initialized with %d free pages\n", nfreepages);
}

//----------------------------------------------------------------------
//
// MemoryTranslateUserToSystem
//
//	Translate a user address (in the process referenced by pcb)
//	into an OS (physical) address.  Return the physical address.
//
//----------------------------------------------------------------------
uint32 MemoryTranslateUserToSystem (PCB *pcb, uint32 addr) {
  uint32 l1index;
  uint32 pte;
  uint32 physaddr;
  uint32 offset;

  // Check that the address is in range
  if (addr > MEM_MAX_VIRTUAL_ADDRESS) {
    dbprintf('m', "MemoryTranslateUserToSystem: addr 0x%x out of range\n", addr);
    return 0;
  }

  // Extract the L1 page table index from the virtual address
  l1index = addr >> MEM_L1FIELD_FIRST_BITNUM;

  // Get the page table entry from the PCB's page table
  pte = pcb->pagetable[l1index];

  // Check that the page is valid
  if ((pte & MEM_PTE_VALID) == 0) {
    dbprintf('m', "MemoryTranslateUserToSystem: invalid PTE for vaddr 0x%x (index %d)\n", 
             addr, l1index);
    return 0;
  }

  // Extract offset from virtual address
  offset = addr & MEM_PAGE_OFFSET_MASK;
  
  // Construct the physical address: (physical page address) | offset
  physaddr = (pte & MEM_ADDRESS_OFFSET_MASK) | offset;

  dbprintf('m', "MemoryTranslateUserToSystem: vaddr 0x%x -> paddr 0x%x\n", addr, physaddr);
  return physaddr;
}


//----------------------------------------------------------------------
//
//	MemoryMoveBetweenSpaces
//
//	Copy data between user and system spaces.  This is done page by
//	page by:
//	* Translating the user address into system space.
//	* Copying all of the data in that page
//	* Repeating until all of the data is copied.
//	A positive direction means the copy goes from system to user
//	space; negative direction means the copy goes from user to system
//	space.
//
//	This routine returns the number of bytes copied.  Note that this
//	may be less than the number requested if there were unmapped pages
//	in the user range.  If this happens, the copy stops at the
//	first unmapped address.
//
//----------------------------------------------------------------------
int MemoryMoveBetweenSpaces (PCB *pcb, unsigned char *system, unsigned char *user, int n, int dir) {
  unsigned char *curUser;         // Holds current physical address representing user-space virtual address
  int		bytesCopied = 0;  // Running counter
  int		bytesToCopy;      // Used to compute number of bytes left in page to be copied

  while (n > 0) {
    // Translate current user page to system address.  If this fails, return
    // the number of bytes copied so far.
    curUser = (unsigned char *)MemoryTranslateUserToSystem (pcb, (uint32)user);

    // If we could not translate address, exit now
    if (curUser == (unsigned char *)0) break;

    // Calculate the number of bytes to copy this time.  If we have more bytes
    // to copy than there are left in the current page, we'll have to just copy to the
    // end of the page and then go through the loop again with the next page.
    // In other words, "bytesToCopy" is the minimum of the bytes left on this page 
    // and the total number of bytes left to copy ("n").

    // First, compute number of bytes left in this page.  This is just
    // the total size of a page minus the current offset part of the physical
    // address.  MEM_PAGESIZE should be the size (in bytes) of 1 page of memory.
    // MEM_ADDRESS_OFFSET_MASK should be the bit mask required to get just the
    // "offset" portion of an address.
    bytesToCopy = MEM_PAGESIZE - ((uint32)curUser & MEM_ADDRESS_OFFSET_MASK);
    
    // Now find minimum of bytes in this page vs. total bytes left to copy
    if (bytesToCopy > n) {
      bytesToCopy = n;
    }

    // Perform the copy.
    if (dir >= 0) {
      bcopy (system, curUser, bytesToCopy);
    } else {
      bcopy (curUser, system, bytesToCopy);
    }

    // Keep track of bytes copied and adjust addresses appropriately.
    n -= bytesToCopy;           // Total number of bytes left to copy
    bytesCopied += bytesToCopy; // Total number of bytes copied thus far
    system += bytesToCopy;      // Current address in system space to copy next bytes from/into
    user += bytesToCopy;        // Current virtual address in user space to copy next bytes from/into
  }
  return (bytesCopied);
}

//----------------------------------------------------------------------
//
//	These two routines copy data between user and system spaces.
//	They call a common routine to do the copying; the only difference
//	between the calls is the actual call to do the copying.  Everything
//	else is identical.
//
//----------------------------------------------------------------------
int MemoryCopySystemToUser (PCB *pcb, unsigned char *from,unsigned char *to, int n) {
  return (MemoryMoveBetweenSpaces (pcb, from, to, n, 1));
}

int MemoryCopyUserToSystem (PCB *pcb, unsigned char *from,unsigned char *to, int n) {
  return (MemoryMoveBetweenSpaces (pcb, to, from, n, -1));
}

//---------------------------------------------------------------------
// MemoryPageFaultHandler is called in traps.c whenever a page fault 
// (better known as a "seg fault" occurs.  If the address that was
// being accessed is on the stack, we need to allocate a new page 
// for the stack.  If it is not on the stack, then this is a legitimate
// seg fault and we should kill the process.  Returns MEM_SUCCESS
// on success, and kills the current process on failure.  Note that
// fault_address is the beginning of the page of the virtual address that 
// caused the page fault, i.e. it is the vaddr with the offset zero-ed
// out.
//
// Note: The existing code is incomplete and only for reference. 
// Feel free to edit.
//---------------------------------------------------------------------
int MemoryPageFaultHandler(PCB *pcb) {
  uint32 fault_address;
  uint32 user_stack_pointer;
  uint32 fault_page;
  uint32 stack_page;
  int page;
  
  // Get the faulting address (with offset zeroed out)
  fault_address = pcb->currentSavedFrame[PROCESS_STACK_FAULT];
  fault_page = fault_address >> MEM_L1FIELD_FIRST_BITNUM;
  
  // Get user stack pointer and its page
  user_stack_pointer = pcb->currentSavedFrame[PROCESS_STACK_USER_STACKPOINTER];
  stack_page = user_stack_pointer >> MEM_L1FIELD_FIRST_BITNUM;
  
  dbprintf('m', "MemoryPageFaultHandler (%d): fault_addr=0x%x (page %d), stack_ptr=0x%x (page %d)\n",
           GetCurrentPid(), fault_address, fault_page, user_stack_pointer, stack_page);
  
  // Stack grows downward. Check if fault is within valid stack range
  // Allow fault_page == stack_page - 1 or stack_page - 2 (for function calls)
  // The -8 bytes (2 words) accounts for return address and frame pointer
  if (fault_page >= (stack_page - 2) && fault_page <= stack_page) {
    // This is legitimate stack growth
    
    // Check if this page already exists (shouldn't happen, but be safe)
    if (pcb->pagetable[fault_page] & MEM_PTE_VALID) {
      dbprintf('m', "MemoryPageFaultHandler (%d): page %d already allocated?\n",
               GetCurrentPid(), fault_page);
      return MEM_SUCCESS;
    }
    
    // Allocate a new physical page
    page = MemoryAllocPage();
    if (page < 0) {
      printf("FATAL ERROR: Out of physical memory in MemoryPageFaultHandler for PID %d!\n",
             GetCurrentPid());
      ProcessKill();
      return MEM_FAIL;
    }
    
    // Install the new page in the page table
    pcb->pagetable[fault_page] = (page << MEM_L1FIELD_FIRST_BITNUM) | MEM_PTE_VALID;
    pcb->npages++;
    
    dbprintf('m', "MemoryPageFaultHandler (%d): allocated physical page %d for virtual page %d\n", 
             GetCurrentPid(), page, fault_page);
    return MEM_SUCCESS;
  }
  
  // Not a valid stack access - segmentation fault
  printf("SEGMENTATION FAULT: Process %d accessed invalid address 0x%x\n", 
         GetCurrentPid(), fault_address);
  ProcessKill();
  return MEM_FAIL;
}


//---------------------------------------------------------------------
// You may need to implement the following functions and access them from process.c
// Feel free to edit/remove them
//---------------------------------------------------------------------

int MemoryAllocPage(void) {
  int i, j;
  uint32 mask;
  
  for (i = 0; i < (freemapmax / 32 + 1); i++) {
    if (freemap[i] != 0xFFFFFFFF) {
      // Found a word with at least one free page
      for (j = 0; j < 32; j++) {
        mask = 1 << j;
        if ((freemap[i] & mask) == 0) {
          // Found a free page
          int page = i * 32 + j;
          if (page >= freemapmax) {
            return -1;
          }
          
          // Mark page as used
          freemap[i] |= mask;
          nfreepages--;
          
          dbprintf('m', "MemoryAllocPage: allocated page %d (phys addr 0x%x), %d pages remaining\n",
                   page, pagestart + (page << MEM_L1FIELD_FIRST_BITNUM), nfreepages);
          return page;
        }
      }
    }
  }
  
  dbprintf('m', "MemoryAllocPage: no free pages available!\n");
  return -1;
}

uint32 MemorySetupPte(uint32 page) {
  uint32 pte;
  uint32 physAddr;
  
  if (page >= freemapmax) {
    dbprintf('m', "MemorySetupPte: invalid page %d\n", page);
    return 0;
  }
  
  // Calculate physical address
  physAddr = pagestart + (page << MEM_L1FIELD_FIRST_BITNUM);
  
  // Create PTE with valid bit set
  pte = physAddr | MEM_PTE_VALID;
  
  dbprintf('m', "MemorySetupPte: page %d -> PTE 0x%x (phys addr 0x%x)\n",
           page, pte, physAddr);
  
  return pte;
}


void MemoryFreePage(uint32 page) {
  int word, bit;
  uint32 mask;
  
  if (page >= freemapmax) {
    dbprintf('m', "MemoryFreePage: invalid page %d\n", page);
    return;
  }
  
  word = page / 32;
  bit = page % 32;
  mask = 1 << bit;
  
  // Check if page is actually allocated
  if ((freemap[word] & mask) == 0) {
    dbprintf('m', "MemoryFreePage: warning - page %d was already free\n", page);
    return;
  }
  
  // Mark page as free
  freemap[word] &= ~mask;
  nfreepages++;
  
  dbprintf('m', "MemoryFreePage: freed page %d, %d pages now available\n",
           page, nfreepages);
}

// code for question 3
// void *malloc(PCB *pcb, int size) {}
// int mfree(PCB *pcb, void *ptr) {}
