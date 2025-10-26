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
typedef struct mem_block {
    uint32 size;           // Size of this block (including header)
    int in_use;            // 1 if allocated, 0 if free
    struct mem_block *next; // Pointer to next block
} mem_block_t;

#define BLOCK_HEADER_SIZE sizeof(mem_block_t)
#define MIN_BLOCK_SIZE (BLOCK_HEADER_SIZE + 16)

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
  pagestart = memsize - MEM_MAX_PHYS_MEM;
  os_pages = pagestart / MEM_PAGESIZE;
  nfreepages = MEM_MAX_PAGES;
  freemapmax = MEM_MAX_PAGES;
  for (i = 0; i < freemapmax; i++) {
    freemap[i] = 0; // Mark all pages as free
  }
  // Now mark pages used by OS as in use
  for (i = 0; i < os_pages; i++) {
    freemap[i] = 1; // Mark page as used
    nfreepages--;
  }
  dbprintf ('m', "MemoryModuleInit: memsize=0x%x, pagestart=0x%x, nfreepages=%d\n",
      memsize, pagestart, nfreepages);
    
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
  uint32 l1index;       // Index into L1 page table
  uint32 pte;           // Page table entry
  uint32 physaddr;      // Physical address to return

  // Check that the address is in range
  if (addr > MEM_MAX_VIRTUAL_ADDRESS) {
    return (0);
  }

  // Extract the L1 page table index from the virtual address
  l1index = addr >> MEM_L1FIELD_FIRST_BITNUM;

  // Get the page table entry from the PCB's page table
  pte = pcb->pagetable[l1index];

  // Check that the page is valid
  if ((pte & MEM_PTE_VALID) == 0) {
    return (0);
  }

  // Construct the physical address
  physaddr = (pte & MEM_ADDRESS_OFFSET_MASK) | (addr & MEM_PAGE_OFFSET_MASK);

  return (physaddr);
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
  uint32 user_stack_base;
  int i;
  // Get the faulting address from the saved frame in the PCB
  fault_address = pcb->currentSavedFrame[PROCESS_STACK_FAULT];
  // Compute the base of the user stack
  user_stack_base = MEM_MAX_VIRTUAL_ADDRESS + 1 - MEM_PAGESIZE;
  // Check if the fault address is within the user stack region
  if (fault_address >= user_stack_base) {
    // Allocate a new page for the stack
    int page = MemoryAllocPage();
    if (page < 0) {
      printf("FATAL ERROR: could not allocate page for stack in MemoryPageFaultHandler!\n");
      ProcessSetStatus (pcb, PROCESS_STATUS_ZOMBIE);
      }
    // Find the first free entry in the page table for the new stack page
    for (i = MEM_L1TABLE_SIZE - 1; i >= 0; i--) {
      if ((pcb->pagetable[i] & MEM_PTE_VALID) == 0) {
        pcb->pagetable[i] = (page << MEM_L1FIELD_FIRST_BITNUM) | MEM_PTE_VALID | MEM_PTE_DIRTY;
        pcb->npages++;
        dbprintf('m', "MemoryPageFaultHandler: allocated stack page %d at 0x%x for PCB 0x%x\n", page, page << MEM_L1FIELD_FIRST_BITNUM, (int)pcb);
        return MEM_SUCCESS;
      }
    }
  }
    // If we reach here, there was no free entry in the page table
    printf("FATAL ERROR: no free page table entry for stack page in MemoryPageFaultHandler!\n");
    ProcessSetStatus (pcb, PROCESS_STATUS_ZOMBIE);

  return MEM_FAIL;
}


//---------------------------------------------------------------------
// You may need to implement the following functions and access them from process.c
// Feel free to edit/remove them
//---------------------------------------------------------------------

int MemoryAllocPage(void) {
  int i;
  for (i = 0; i < freemapmax; i++) {
    if (freemap[i] == 0) {
      freemap[i] = 1; // Mark page as used
      nfreepages--;
      dbprintf('m', "MemoryAllocPage: allocated page %d, nfreepages=%d\n", i, nfreepages);
      return i;
    }
  }

  return -1;
}


uint32 MemorySetupPte (uint32 page) {
  if (page < freemapmax && freemap[page] == 1) {
    uint32 pte = (page * MEM_PAGESIZE) | MEM_PTE_VALID;
    dbprintf('m', "MemorySetupPte: page %d, pte=0x%x\n", page, pte);
    return pte;
  }

  return -1;
}


void MemoryFreePage(uint32 page) {
  if (page < freemapmax && freemap[page] == 1) {
    freemap[page] = 0; // Mark page as free
    nfreepages++;
    dbprintf('m', "MemoryFreePage: freed page %d, nfreepages=%d\n", page, nfreepages);
  }
}


void *malloc(PCB *pcb, int size) {
    mem_block_t *current, *prev, *new_block;
    uint32 total_size;
    uint32 heap_start, heap_end;
    int pages_needed, i;
    int page_num;
    uint32 vaddr_index;
    uint32 pte;
    uint32 phys_addr;
    uint32 page_to_free;
    uint32 vaddr;
    void *result;
    
    // Validate input
    if (size <= 0 || pcb == NULL) {
        dbprintf('m', "malloc: invalid parameters size=%d pcb=0x%x\n", size, (int)pcb);
        return NULL;
    }
    
    // Align size to 4-byte boundary and add header
    size = (size + 3) & ~0x3;
    total_size = size + BLOCK_HEADER_SIZE;
    
    dbprintf('m', "malloc: PID=%d requested size=%d, total_size=%d\n", 
             GetPidFromAddress(pcb), size, total_size);
    
    // Initialize heap if this is the first allocation
    // Heap starts after system stack area and grows upward
    // Leave room for user stack which grows down from MEM_MAX_VIRTUAL_ADDRESS
    heap_start = pcb->sysStackArea + MEM_PAGESIZE; // Start one page after sys stack
    heap_end = MEM_MAX_VIRTUAL_ADDRESS - (8 * MEM_PAGESIZE); // Reserve 8 pages for user stack
    
    if (pcb->heap_start == 0) {
        pcb->heap_start = heap_start;
        pcb->heap_end = heap_start;
        current = NULL;
        dbprintf('m', "malloc: initializing heap at vaddr 0x%x\n", heap_start);
    } else {
        // Search for a free block using first-fit algorithm
        current = (mem_block_t *)pcb->heap_start;
        prev = NULL;
        
        while (current != NULL && (uint32)current < pcb->heap_end) {
            if (!current->in_use && current->size >= total_size) {
                // Found a suitable free block
                dbprintf('m', "malloc: found free block at vaddr 0x%x, size=%d\n", 
                         (uint32)current, current->size);
                
                if (current->size >= total_size + MIN_BLOCK_SIZE) {
                    // Split the block if there's enough space left
                    new_block = (mem_block_t *)((char *)current + total_size);
                    new_block->size = current->size - total_size;
                    new_block->in_use = 0;
                    new_block->next = current->next;
                    
                    current->size = total_size;
                    current->next = new_block;
                    dbprintf('m', "malloc: split block, new free block at vaddr 0x%x\n", 
                             (uint32)new_block);
                }
                
                current->in_use = 1;
                result = (void *)((char *)current + BLOCK_HEADER_SIZE);
                dbprintf('m', "malloc: returning vaddr 0x%x\n", (uint32)result);
                return result;
            }
            prev = current;
            current = current->next;
        }
    }
    
    // No suitable free block found, need to expand heap
    pages_needed = (total_size + MEM_PAGESIZE - 1) / MEM_PAGESIZE;
    
    dbprintf('m', "malloc: need to expand heap by %d pages\n", pages_needed);
    
    // Check if we have space in virtual address space
    if (pcb->heap_end + (pages_needed * MEM_PAGESIZE) > heap_end) {
        dbprintf('m', "malloc: out of virtual address space\n");
        return NULL;
    }
    
    // Allocate physical pages and map them
    for (i = 0; i < pages_needed; i++) {
        page_num = MemoryAllocPage();
        if (page_num < 0) {
            dbprintf('m', "malloc: out of physical memory at page %d\n", i);
            // Out of physical memory, free previously allocated pages
            while (i > 0) {
                i--;
                vaddr_index = (pcb->heap_end + (i * MEM_PAGESIZE)) >> MEM_L1FIELD_FIRST_BITNUM;
                pte = pcb->pagetable[vaddr_index];
                if (pte & MEM_PTE_VALID) {
                    // Extract page number from PTE
                    phys_addr = pte & MEM_ADDRESS_OFFSET_MASK;
                    page_to_free = phys_addr / MEM_PAGESIZE;
                    MemoryFreePage(page_to_free);
                    pcb->pagetable[vaddr_index] = 0;
                    pcb->npages--;
                }
            }
            return NULL;
        }
        
        // Map the page into process's page table
        vaddr = pcb->heap_end + (i * MEM_PAGESIZE);
        vaddr_index = vaddr >> MEM_L1FIELD_FIRST_BITNUM;
        pcb->pagetable[vaddr_index] = MemorySetupPte(page_num);
        pcb->npages++;
        
        dbprintf('m', "malloc: mapped physical page %d to vaddr 0x%x (index %d)\n", 
                 page_num, vaddr, vaddr_index);
    }
    
    // Create new block at the end of the heap
    current = (mem_block_t *)pcb->heap_end;
    current->size = pages_needed * MEM_PAGESIZE;
    current->in_use = 1;
    current->next = NULL;
    
    dbprintf('m', "malloc: created new block at vaddr 0x%x, size=%d\n", 
             (uint32)current, current->size);
    
    // Update heap end
    pcb->heap_end += pages_needed * MEM_PAGESIZE;
    
    // If there's leftover space, create a free block
    if (current->size > total_size + MIN_BLOCK_SIZE) {
        new_block = (mem_block_t *)((char *)current + total_size);
        new_block->size = current->size - total_size;
        new_block->in_use = 0;
        new_block->next = NULL;
        
        current->size = total_size;
        current->next = new_block;
        
        dbprintf('m', "malloc: created leftover free block at vaddr 0x%x, size=%d\n", 
                 (uint32)new_block, new_block->size);
    }
    
    result = (void *)((char *)current + BLOCK_HEADER_SIZE);
    dbprintf('m', "malloc: returning vaddr 0x%x\n", (uint32)result);
    return result;
}

//----------------------------------------------------------------------
// mfree - Free previously allocated memory
//
// Parameters:
//   pcb - pointer to the process control block
//   ptr - pointer to memory to free
//
// Returns:
//   0 on success, -1 on failure
//----------------------------------------------------------------------
int mfree(PCB *pcb, void *ptr) {
    mem_block_t *block, *current, *prev;
    
    // Validate input
    if (ptr == NULL || pcb == NULL) {
        dbprintf('m', "mfree: invalid parameters ptr=0x%x pcb=0x%x\n", (int)ptr, (int)pcb);
        return -1;
    }
    
    // Get block header
    block = (mem_block_t *)((char *)ptr - BLOCK_HEADER_SIZE);
    
    dbprintf('m', "mfree: PID=%d freeing block at vaddr 0x%x (user ptr was 0x%x)\n", 
             GetPidFromAddress(pcb), (uint32)block, (uint32)ptr);
    
    // Verify heap is initialized
    if (pcb->heap_start == 0) {
        dbprintf('m', "mfree: heap not initialized\n");
        return -1;
    }
    
    // Verify this is within the heap
    if ((uint32)block < pcb->heap_start || (uint32)block >= pcb->heap_end) {
        dbprintf('m', "mfree: address 0x%x out of heap bounds [0x%x, 0x%x)\n",
                 (uint32)block, pcb->heap_start, pcb->heap_end);
        return -1;
    }
    
    // Check if block is already free (double-free detection)
    if (!block->in_use) {
        dbprintf('m', "mfree: double-free detected at vaddr 0x%x\n", (uint32)block);
        return -1;
    }
    
    // Mark block as free
    block->in_use = 0;
    dbprintf('m', "mfree: marked block as free, size=%d\n", block->size);
    
    // Coalesce with next block if it's free
    if (block->next != NULL && !block->next->in_use) {
        dbprintf('m', "mfree: coalescing with next block at vaddr 0x%x\n", (uint32)block->next);
        block->size += block->next->size;
        block->next = block->next->next;
    }
    
    // Coalesce with previous block if it's free
    current = (mem_block_t *)pcb->heap_start;
    prev = NULL;
    
    while (current != NULL && current != block) {
        if (!current->in_use && current->next == block) {
            // Previous block is free, merge with it
            dbprintf('m', "mfree: coalescing with previous block at vaddr 0x%x\n", (uint32)current);
            current->size += block->size;
            current->next = block->next;
            break;
        }
        prev = current;
        current = current->next;
    }
    
    dbprintf('m', "mfree: completed successfully\n");
    return 0;
}