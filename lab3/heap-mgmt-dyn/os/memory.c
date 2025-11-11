//
//	memory.c
//
//	Routines for dealing with memory management.

//static char rcsid[] = "$Id: memory.c,v 1.1 2000/09/20 01:50:19 elm Exp elm $";

#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "memory.h"
#include "memory_constants.h"
#include "queue.h"

// num_pages = size_of_memory / size_of_one_page
#define FREEMAP_SIZE ((MEM_MAX_PAGES + 31) / 32)
static uint32 freemap[FREEMAP_SIZE]; // Bitmap for free pages
static uint32 pagestart;
static int nfreepages;
static int freemapmax;

void BuddyInit(BuddyNode *tree, uint32 vaddress) {
    int i;
    tree[0].order = MAX_ORDER;
    tree[0].addr = vaddress;
    tree[0].state = FREE;

    for (i = 1; i < NODE_COUNT; i++) {
        tree[i].state = FREE;
        tree[i].order = -1; // invalid
    }
}

uint32 BuddyAlloc(PCB *pcb, int idx, int needed_order) {
    BuddyNode *n = &pcb->tree[idx];
    int left, right;
    uint32 addr, half;
    int i;

    if (n->state == USED) return 0;

    if (n->order == -1) {
      printf("Error: trying to allocate from invalid node idx=%d\n", idx);
      ProcessKill();
    }
    
    if (n->order == needed_order) {
        if (n->state == FREE) {
            n->state = USED;
            return n->addr;
        }
        // means it is SPLIT hence we don't have a free block here 
        else return 0;
    }

    // need smaller block
    if (n->state == FREE) {
        n->state = SPLIT;
        left = 2*idx + 1;
        right = 2*idx + 2;
        half = (MIN_BLOCK_SIZE << (n->order - 1));

        pcb->tree[left].order = n->order - 1;
        pcb->tree[right].order = n->order - 1;
        pcb->tree[left].addr = n->addr;
        pcb->tree[right].addr = n->addr + half;
        pcb->tree[left].state = FREE;
        pcb->tree[right].state = FREE;

        printf("Created a left child node (order = %d, addr = %d, size = %d) of parent (order = %d, addr = %d, size = %d)\n",
            pcb->tree[left].order, pcb->tree[left].addr - pcb->heapstartaddr, half,
            n->order, n->addr - pcb->heapstartaddr, half*2);
        printf("Created a right child node (order = %d, addr = %d, size = %d) of parent (order = %d, addr = %d, size = %d)\n",
            pcb->tree[right].order, pcb->tree[right].addr - pcb->heapstartaddr, half,
            n->order, n->addr - pcb->heapstartaddr, half*2);
    }

    addr = BuddyAlloc(pcb, 2*idx + 1, needed_order);
    if (addr == 0)
        addr = BuddyAlloc(pcb, 2*idx + 2, needed_order);

    return addr;
}

void merge(PCB *pcb, int idx) {
    int parent;
    int left;
    int right;
    BuddyNode *L;
    BuddyNode *R;
    BuddyNode *P;
    
    // find parent
    if (idx == 0) {
        return;  // root has no parent
    }

    parent = (idx - 1) / 2;
    left = 2 * parent + 1;
    right = 2 * parent + 2;

    L = &pcb->tree[left];
    R = &pcb->tree[right];
    P = &pcb->tree[parent];

    // only coalesce if both children are free
    if (L->state == FREE && R->state == FREE) {
        P->state = FREE;
        // set order
        P->order = L->order + 1;
        printf("Coalesced buddy nodes (order = %d, addr = %d, size = %d) & (order = %d, addr = %d, size = %d)\n",
            L->order, L->addr - pcb->heapstartaddr, MIN_BLOCK_SIZE << L->order,
            R->order, R->addr - pcb->heapstartaddr, MIN_BLOCK_SIZE << R->order);
        printf("into the parent node (order = %d, addr = %d, size = %d)\n",
            P->order, P->addr - pcb->heapstartaddr, MIN_BLOCK_SIZE << P->order);

        // recursively try to merge upward
        merge(pcb, parent);
    }
}

void BuddyFree(PCB *pcb, int idx, uint32 addr) {
    int size;
    int i;  
    BuddyNode *n = &pcb->tree[idx];
    size = MIN_BLOCK_SIZE << n->order;

    if (n->state == USED) {
        if (n->addr != addr) {
            printf("Error: trying to free block at addr=%x but node addr=%x\n",
                    addr, n->addr);
            ProcessKill();
            return;
        }
        n->state = FREE;
        printf("Freed the block: order = %d, addr = %d, size = %d\n",
            n->order, n->addr - pcb->heapstartaddr, size);
        merge(pcb, idx);
        return;
    }
    if (n->state == SPLIT) {
        // asked to free a block but this node is split, error
        printf("Error: trying to free block at addr=%d but node (order=%d, addr=%d) is SPLIT\n",
                addr, n->order, n->addr);
        ProcessKill();
        return;
    }
    else {
        printf("Error: trying to free block at addr=%d but node (order=%d, addr=%d) is already FREE\n",
                addr, n->order, n->addr);
        return;
    }
}

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
//	MemorySetFreemap
//
//----------------------------------------------------------------------
inline
void
MemorySetFreemap (int p, int b)
{
  uint32	wd = p / 32;
  uint32	bitnum = p % 32;

  freemap[wd] = (freemap[wd] & invert(1 << bitnum)) | (b << bitnum);
  dbprintf ('m', "Set freemap entry %d to 0x%x.\n",
	    wd, freemap[wd]);
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
void
MemoryModuleInit ()
{
  int		i;
  int		maxpage = MemoryGetSize () / MEM_PAGESIZE;
  int		curpage;

  pagestart = (lastosaddress + MEM_PAGESIZE - 4) / MEM_PAGESIZE;
  freemapmax = (maxpage+31) / 32;
  dbprintf ('m', "Map has %d entries, memory size is 0x%x.\n",
	    freemapmax, maxpage);
  dbprintf ('m', "Free pages start with page # 0x%x.\n", pagestart);
  for (i = 0; i < freemapmax; i++) {
    // Initially, all pages are considered in use.  This is done to make
    // sure we don't have any partially initialized freemap entries.
    freemap[i] = 0;
  }
  nfreepages = 0;
  for (curpage = pagestart; curpage < maxpage; curpage++) {
    nfreepages += 1;
    MemorySetFreemap (curpage, 1);
  }
  dbprintf ('m', "Initialized %d free pages.\n", nfreepages);
}


//----------------------------------------------------------------------
//
// MemoryTranslateUserToSystem
//
//	Translate a user address (in the process referenced by pcb)
//	into an OS (physical) address.  Return the physical address.
//
//----------------------------------------------------------------------
uint32
MemoryTranslateUserToSystem (PCB *pcb, uint32 addr)
{ 
    int	page = addr / MEM_PAGESIZE;
    int offset = addr % MEM_PAGESIZE;
    dbprintf('m', "MemoryTranslateUserToSystem: translating vaddr 0x%x\n", addr);

    return ((pcb->pagetable[page] & MEM_PTE_ADDR_MASK) + offset);
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
  int heap_start_page = pcb->heapstartpage; // heap starts at virtual page 4
  int heap_end_page = heap_start_page + (HEAP_SIZE / MEM_PAGESIZE )- 1; // adjust as per your heap size
  
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
    if (page <= 0) {
      printf("FATAL ERROR: Out of physical memory in MemoryPageFaultHandler for PID %d!\n",
             GetCurrentPid());
      ProcessKill();
      return MEM_FAIL;
    }
    
    // Install the new page in the page table
    pcb->pagetable[fault_page] = (page << MEM_L1FIELD_FIRST_BITNUM) | MEM_PTE_VALID;
    pcb->npages++;
    
    dbprintf('m', "MemoryPageFaultHandler (%d): allocated physical page %d for virtual page %d, remaining pages %d\n",
             GetCurrentPid(), page, fault_page, nfreepages);
    return MEM_SUCCESS;
  }

  dbprintf('m', "MemoryPageFaultHandler (%d): heap_start_page=%d, heap_end_page=%d, fault_page=%d, address=0x%x\n",
           GetCurrentPid(), heap_start_page, heap_end_page, fault_page, fault_address);
  
  // Check if fault address is in heap region
  if (fault_page >= heap_start_page && fault_page <= heap_end_page) {
    // This is legitimate heap access
    if (pcb->pagetable[fault_page] & MEM_PTE_VALID) {
      dbprintf('m', "MemoryPageFaultHandler (%d): page %d already allocated?\n",
               GetCurrentPid(), fault_page);
      return MEM_SUCCESS;
    }
    // Allocate a new physical page
    page = MemoryAllocPage();
    if (page <= 0) {
      printf("FATAL ERROR: Out of physical memory in MemoryPageFaultHandler for PID %d!\n",
             GetCurrentPid());
      ProcessKill();
      return MEM_FAIL;  
    }
    // Install the new page in the page table
    pcb->pagetable[fault_page] = MemorySetupPte(page);
    pcb->npages++;
    dbprintf('m', "MemoryPageFaultHandler (%d): allocated physical page %d for virtual page %d, remaining pages %d\n",
             GetCurrentPid(), page, fault_page, nfreepages);
    printf("Allocated physical page %d to back up virtual page %d in the heap\n",
           page, fault_page - heap_start_page);
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

int MemoryAllocPage (void)
{
  static int	mapnum = 0;
  int		bitnum;
  uint32	v;

  if (nfreepages == 0) {
    dbprintf ('m', "MemoryAllocPage: no free pages!\n");
    return (0);
  }
  dbprintf ('m', "Allocating memory, starting with page %d\n", mapnum);
  while (freemap[mapnum] == 0) {
    mapnum += 1;
    if (mapnum >= freemapmax) {
      mapnum = 0;
    }
  }
  v = freemap[mapnum];
  for (bitnum = 0; (v & (1 << bitnum)) == 0; bitnum++) {
  }
  freemap[mapnum] &= invert(1 << bitnum);
  v = (mapnum * 32) + bitnum;
  dbprintf ('m', "Allocated memory, from map %d, page %d, map=0x%x.\n",
	    mapnum, v, freemap[mapnum]);
  nfreepages -= 1;
  return (v);
}


uint32 MemorySetupPte (uint32 page) {
  return ((page * MEM_PAGESIZE) | MEM_PTE_VALID);
}


void MemoryFreePage(uint32 page) {
  MemorySetFreemap (page, 1);
  nfreepages += 1;
  dbprintf ('m',"Freed page %d, %d remaining.\n", page, nfreepages);
}

int GetNeededOrder(int memsize) {
    int rounded_size, order;
    if (memsize <= 0) return -1;

    // 1. round up to multiple of MIN_BLOCK_SIZE
    rounded_size = ((memsize + MIN_BLOCK_SIZE - 1) / MIN_BLOCK_SIZE) * MIN_BLOCK_SIZE;

    // 2. find smallest order whose block fits
    order = 0;
    while ((MIN_BLOCK_SIZE << order) < rounded_size) {
        order++;
    }
    if (order > MAX_ORDER) {
        printf("Error: malloc size %d exceeds maximum allocatable block size, max order is %d, requested order is %d\n", 
            memsize, MAX_ORDER, order);
        return -1;
    }
    return order;
}

void *malloc(PCB *pcb, int size) {
    uint32 vaddress;
    int needed_order = GetNeededOrder(size);
    int i;
    
    if (needed_order == -1) {
        ProcessKill();
        return 0;
    }
    
    dbprintf('m', "malloc: requesting allocation of size %d, needed order %d, max order %d\n",
             size, needed_order, MAX_ORDER);
    
    vaddress = BuddyAlloc(pcb, 0, needed_order);
    if (vaddress == 0) {
        printf("Error: malloc failed to allocate %d bytes\n", size);
        ProcessKill();
        return 0;
    }
    
    // Store size in separate allocation table
    for (i = 0; i < MAX_ACTIVE_ALLOCATIONS; i++) {
        if (!pcb->alloc_table[i].in_use) {
            pcb->alloc_table[i].addr = vaddress;
            pcb->alloc_table[i].size = size;
            pcb->alloc_table[i].in_use = 1;
            break;
        }
    }
    
    if (i == MAX_ACTIVE_ALLOCATIONS) {
        printf("Warning: allocation table full, size tracking may be inaccurate\n");
    }
    
    {
        int block_size = MIN_BLOCK_SIZE << needed_order;
        printf("Allocated the block: order = %d, addr = %d, requested mem size = %d, block size = %d\n",
               needed_order, vaddress - pcb->heapstartaddr, size, block_size);
    }
    
    dbprintf('m', "malloc: allocated memory at address %d of order %d\n",
             vaddress, needed_order);
    
    return (void *)vaddress;
}

int find_index_by_addr(PCB *pcb, uint32 addr) {
    int i;
    for (i = 0; i < NODE_COUNT; i++) {
        if (pcb->tree[i].addr == addr && pcb->tree[i].state == USED) {
            return i;
        }
    }
    return -1;
}

void print_buddy_tree(PCB *pcb) {
    int i;
    printf("Buddy Tree State:\n");
    for (i = 0; i < NODE_COUNT; i++) {
        if (pcb->tree[i].state != FREE) {
            printf("Node idx=%d order=%d addr=%d state=%d\n",
                   i, pcb->tree[i].order, pcb->tree[i].addr, pcb->tree[i].state);
        }
    }
}

int mfree(PCB *pcb, void *ptr) {
    uint32 addr = (uint32)ptr;
    int index = -1, i;
    int actual_size = 0;

    if (ptr == NULL || ptr == 0) {
        printf("Error: mfree called with NULL pointer\n");
        return -1;
    }
    
    dbprintf('m', "mfree: freeing memory at address %d\n", addr);
    
    // Find and remove from allocation table
    for (i = 0; i < MAX_ACTIVE_ALLOCATIONS; i++) {
        if (pcb->alloc_table[i].in_use && pcb->alloc_table[i].addr == addr) {
            actual_size = pcb->alloc_table[i].size;
            pcb->alloc_table[i].in_use = 0;
            break;
        }
    }
    
    index = find_index_by_addr(pcb, addr);
    if (index == -1) {
        printf("Error: mfree failed to find allocation record for address %d\n", addr);
        return -1;
    }

    BuddyFree(pcb, index, addr);
    
    dbprintf('m', "mfree: freed memory at address 0x%x from node index %d\n",
             addr, index);
    
    return actual_size;
}