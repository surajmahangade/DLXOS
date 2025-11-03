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
static uint32 freemap[MEM_MAX_PAGES / 32 + 1];
uint32 pagestart;
static int nfreepages;
static int freemapmax;

// Buddy tree node structure
typedef struct buddy_node {
  int order;                    // Order of this block (0 to MAX_ORDER)
  int in_use;                   // 1 if allocated, 0 if free
  uint32 addr;                  // Address offset from heap start
  struct buddy_node *left;      // Left child
  struct buddy_node *right;     // Right child
  struct buddy_node *parent;    // Parent node
} buddy_node_t;

// Storage for buddy nodes (statically allocated in kernel space)
#define MAX_BUDDY_NODES 256
static buddy_node_t buddy_node_pool[MAX_BUDDY_NODES];
static int buddy_node_pool_index = 0;

// Forward declarations
static int is_heap_address_allocated(PCB *pcb, uint32 vaddr);
static int init_heap(PCB *pcb);
void MemoryIncreaseRefcount(uint32 page);

static inline uint32 buddy_block_size(int order) {
  return MEM_BUDDY_MIN_SIZE << order;
}

static inline uint32 buddy_address(uint32 addr, int order) {
  uint32 block_size = buddy_block_size(order);
  return addr ^ block_size;
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
  int total_pages;
  
  // Calculate where usable memory starts (after OS)
  pagestart = ((uint32)&lastosaddress + MEM_PAGESIZE - 1) & ~MEM_ADDRESS_OFFSET_MASK;
  
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
  offset = addr & MEM_ADDRESS_OFFSET_MASK;
  
  // Construct the physical address: (physical page address) | offset
  physaddr = (pte & MEM_PTE_ADDR_MASK) | offset;

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
  uint32 heap_start_page, heap_end_page;
  int page;
  
  fault_address = pcb->currentSavedFrame[PROCESS_STACK_FAULT];
  fault_page = fault_address >> MEM_L1FIELD_FIRST_BITNUM;
  
  user_stack_pointer = pcb->currentSavedFrame[PROCESS_STACK_USER_STACKPOINTER];
  stack_page = user_stack_pointer >> MEM_L1FIELD_FIRST_BITNUM;
  
  dbprintf('m', "MemoryPageFaultHandler (%d): fault_addr=0x%x (page %d)\n",
           GetCurrentPid(), fault_address, fault_page);
  
  // Check if this is stack growth
  if (fault_page >= (stack_page - 2) && fault_page <= stack_page) {
    if (pcb->pagetable[fault_page] & MEM_PTE_VALID) {
      return MEM_SUCCESS;
    }
    
    page = MemoryAllocPage();
    if (page < 0) {
      printf("FATAL ERROR: Out of physical memory for stack in PID %d!\n", GetCurrentPid());
      ProcessKill();
      return MEM_FAIL;
    }
    
    pcb->pagetable[fault_page] = MemorySetupPte(page);
    pcb->npages++;
    MemoryIncreaseRefcount(page);
    
    dbprintf('m', "MemoryPageFaultHandler (%d): allocated stack page %d for virtual page %d\n", 
             GetCurrentPid(), page, fault_page);
    return MEM_SUCCESS;
  }
  
  // Check if this is heap growth (Q4)
  if (pcb->heap_vaddr_start > 0) {
    heap_start_page = pcb->heap_vaddr_start >> MEM_L1FIELD_FIRST_BITNUM;
    heap_end_page = heap_start_page + MEM_HEAP_PAGES - 1;
    
    dbprintf('m', "MemoryPageFaultHandler (%d): heap range is pages %d to %d\n",
             GetCurrentPid(), heap_start_page, heap_end_page);
    
    if (fault_page >= heap_start_page && fault_page <= heap_end_page) {
      // This is within the heap's virtual address range
      
      // Check if page already mapped
      if (pcb->pagetable[fault_page] & MEM_PTE_VALID) {
        dbprintf('m', "MemoryPageFaultHandler (%d): heap page %d already mapped\n",
                 GetCurrentPid(), fault_page);
        return MEM_SUCCESS;
      }
      
      // Check if this address was allocated via buddy system
      if (pcb->heap_root != NULL && !is_heap_address_allocated(pcb, fault_address)) {
        printf("SEGMENTATION FAULT: Process %d accessed unallocated heap address 0x%x\n", 
               GetCurrentPid(), fault_address);
        ProcessKill();
        return MEM_FAIL;
      }
      
      // Legitimate heap access - allocate physical page
      page = MemoryAllocPage();
      if (page < 0) {
        printf("FATAL ERROR: Out of physical memory for heap in PID %d!\n", GetCurrentPid());
        ProcessKill();
        return MEM_FAIL;
      }
      
      // Map the page
      pcb->pagetable[fault_page] = MemorySetupPte(page);
      pcb->npages++;
      pcb->heap_pages_mapped++;
      MemoryIncreaseRefcount(page);
      
      printf("Allocated a physical page at %d to back up virtual page %d in the heap\n",
             page, fault_page);
      
      dbprintf('m', "MemoryPageFaultHandler (%d): allocated heap page %d for virtual page %d\n", 
               GetCurrentPid(), page, fault_page);
      return MEM_SUCCESS;
    }
  }
  
  // Not a valid access - segmentation fault
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

// Placeholder for MemoryIncreaseRefcount - implement based on your OS design
void MemoryIncreaseRefcount(uint32 page) {
  // This function should increment the reference count for a physical page
  // Implementation depends on your reference counting mechanism
  dbprintf('m', "MemoryIncreaseRefcount: page %d\n", page);
}

// code for question 3
// void *malloc(PCB *pcb, int size) {}
// int mfree(PCB *pcb, void *ptr) {}
static buddy_node_t* alloc_buddy_node(int order, uint32 addr, buddy_node_t *parent) {
  buddy_node_t *node;
  
  if (buddy_node_pool_index >= MAX_BUDDY_NODES) {
    dbprintf('m', "alloc_buddy_node: out of buddy nodes\n");
    return NULL;
  }
  
  node = &buddy_node_pool[buddy_node_pool_index++];
  node->order = order;
  node->in_use = 0;
  node->addr = addr;
  node->left = NULL;
  node->right = NULL;
  node->parent = parent;
  
  return node;
}

static void reset_buddy_pool(void) {
  buddy_node_pool_index = 0;
}

//----------------------------------------------------------------------
// Check if a heap address has been allocated via malloc
//----------------------------------------------------------------------
static int is_allocated_in_subtree(buddy_node_t *node, uint32 offset) {
  uint32 block_start, block_end;
  
  if (node == NULL) {
    return 0;
  }
  
  block_start = node->addr;
  block_end = node->addr + buddy_block_size(node->order);
  
  // Check if offset falls within this block
  if (offset >= block_start && offset < block_end) {
    // If this is a leaf node that's in use, it's allocated
    if (node->in_use && node->left == NULL && node->right == NULL) {
      return 1;
    }
    
    // If this is an internal node, check children
    if (node->left != NULL || node->right != NULL) {
      return is_allocated_in_subtree(node->left, offset) || 
             is_allocated_in_subtree(node->right, offset);
    }
  }
  
  return 0;
}

static int is_heap_address_allocated(PCB *pcb, uint32 vaddr) {
  uint32 offset;
  
  if (pcb->heap_root == NULL) {
    return 0;
  }
  
  // Calculate offset from heap start
  offset = vaddr - pcb->heap_vaddr_start;
  
  return is_allocated_in_subtree((buddy_node_t *)pcb->heap_root, offset);
}

//----------------------------------------------------------------------
// Initialize heap for a process (Q4 - starts with 1 page, can grow)
//----------------------------------------------------------------------
static int init_heap(PCB *pcb) {
  int vpage;
  
  if (pcb->heap_vaddr_start == 0) {
    // This shouldn't happen if ProcessFork did its job
    printf("ERROR: init_heap called but heap_vaddr_start is 0!\n");
    return -1;
  }
  
  // Check if first page is already mapped (it should be)
  vpage = pcb->heap_vaddr_start >> MEM_L1FIELD_FIRST_BITNUM;
  if (!(pcb->pagetable[vpage] & MEM_PTE_VALID)) {
    printf("ERROR: init_heap called but first heap page not mapped!\n");
    return -1;
  }
  
  // Create root node for buddy tree - starts with max order (64KB = 16 pages)
  pcb->heap_root = (void *)alloc_buddy_node(MEM_BUDDY_MAX_ORDER, 0, NULL);
  
  if (pcb->heap_root == NULL) {
    return -1;
  }
  
  dbprintf('m', "init_heap (%d): initialized buddy tree with root at order %d\n",
           GetPidFromAddress(pcb), MEM_BUDDY_MAX_ORDER);
  dbprintf('m', "init_heap (%d): heap can grow to 64KB (%d pages)\n",
           GetPidFromAddress(pcb), MEM_HEAP_PAGES);
  
  return 0;
}

//----------------------------------------------------------------------
// Buddy Operations - Split and Coalesce
//----------------------------------------------------------------------
static int split_buddy_block(buddy_node_t *node) {
  uint32 left_addr, right_addr;
  uint32 block_size;
  
  if (node->order == 0) {
    return -1;
  }
  
  if (node->in_use) {
    return -1;
  }
  
  block_size = buddy_block_size(node->order - 1);
  left_addr = node->addr;
  right_addr = node->addr + block_size;
  
  node->left = alloc_buddy_node(node->order - 1, left_addr, node);
  node->right = alloc_buddy_node(node->order - 1, right_addr, node);
  
  if (node->left == NULL || node->right == NULL) {
    return -1;
  }
  
  printf("Created a left child node (order = %d, addr = %d, size = %d) of parent (order = %d, addr = %d, size = %d)\n",
         node->left->order, node->left->addr, buddy_block_size(node->left->order),
         node->order, node->addr, buddy_block_size(node->order));
  
  printf("Created a right child node (order = %d, addr = %d, size = %d) of parent (order = %d, addr = %d, size = %d)\n",
         node->right->order, node->right->addr, buddy_block_size(node->right->order),
         node->order, node->addr, buddy_block_size(node->order));
  
  return 0;
}

static buddy_node_t* find_and_alloc_block(buddy_node_t *node, int target_order) {
  buddy_node_t *result;
  
  if (node == NULL) {
    return NULL;
  }
  
  if (node->order == target_order && !node->in_use && node->left == NULL && node->right == NULL) {
    node->in_use = 1;
    return node;
  }
  
  if (node->order < target_order) {
    return NULL;
  }
  
  if (node->left == NULL && node->right == NULL && !node->in_use) {
    if (split_buddy_block(node) < 0) {
      return NULL;
    }
  }
  
  result = find_and_alloc_block(node->left, target_order);
  if (result != NULL) {
    return result;
  }
  
  return find_and_alloc_block(node->right, target_order);
}

static buddy_node_t* find_node_by_addr(buddy_node_t *node, uint32 addr) {
  buddy_node_t *result;
  
  if (node == NULL) {
    return NULL;
  }
  
  if (node->addr == addr && node->in_use && node->left == NULL && node->right == NULL) {
    return node;
  }
  
  result = find_node_by_addr(node->left, addr);
  if (result != NULL) {
    return result;
  }
  
  return find_node_by_addr(node->right, addr);
}

static void coalesce_buddies(buddy_node_t *node) {
  buddy_node_t *parent;
  buddy_node_t *left, *right;
  
  if (node == NULL || node->parent == NULL) {
    return;
  }
  
  parent = node->parent;
  left = parent->left;
  right = parent->right;
  
  if (left != NULL && right != NULL &&
      !left->in_use && !right->in_use &&
      left->left == NULL && left->right == NULL &&
      right->left == NULL && right->right == NULL) {
    
    printf("Coalesced buddy nodes (order = %d, addr = %d, size = %d) & (order = %d, addr = %d, size = %d)\n",
           left->order, left->addr, buddy_block_size(left->order),
           right->order, right->addr, buddy_block_size(right->order));
    printf("into the parent node (order = %d, addr = %d, size = %d)\n",
           parent->order, parent->addr, buddy_block_size(parent->order));
    
    parent->left = NULL;
    parent->right = NULL;
    parent->in_use = 0;
    
    coalesce_buddies(parent);
  }
}

//----------------------------------------------------------------------
// malloc - Q4 Version (multi-page, dynamic allocation)
//----------------------------------------------------------------------
void *malloc(PCB *pcb, int memsize) {
  int order;
  int size;
  buddy_node_t *node;
  uint32 vaddr;
  
  if (memsize <= 0) {
    return NULL;
  }
  
  if (memsize > MEM_HEAP_PAGES * MEM_PAGESIZE) {
    dbprintf('m', "malloc (%d): size %d exceeds max heap size\n", GetPidFromAddress(pcb), memsize);
    return NULL;
  }
  
  size = (memsize + 3) & ~0x3;
  
  if (pcb->heap_root == NULL) {
    pcb->heap_root = (void *)alloc_buddy_node(MEM_BUDDY_MAX_ORDER, 0, NULL);
    if (pcb->heap_root == NULL) {
      return NULL;
    }
    dbprintf('m', "malloc (%d): initialized buddy tree\n", GetPidFromAddress(pcb));
  }
  
  order = 0;
  while (buddy_block_size(order) < size) {
    order++;
    if (order > MEM_BUDDY_MAX_ORDER) {
      return NULL;
    }
  }
  
  node = find_and_alloc_block((buddy_node_t *)pcb->heap_root, order);
  
  if (node == NULL) {
    return NULL;
  }
  
  vaddr = pcb->heap_vaddr_start + node->addr;
  
  printf("Allocated the block: order = %d, addr = %d, requested mem size = %d, block size = %d\n",
         node->order, node->addr, memsize, buddy_block_size(node->order));
  
  dbprintf('m', "malloc (%d): allocated block at vaddr 0x%x (order %d)\n",
           GetPidFromAddress(pcb), vaddr, order);
  
  // Note: Physical pages will be allocated on-demand via page fault handler
  // when the user actually accesses the memory
  
  return (void *)vaddr;
}

//----------------------------------------------------------------------
// mfree - Q4 Version
//----------------------------------------------------------------------
int mfree(PCB *pcb, void *ptr) {
  uint32 vaddr;
  uint32 offset;
  buddy_node_t *node;
  int size;
  
  if (ptr == NULL) {
    return -1;
  }
  
  vaddr = (uint32)ptr;
  
  if (pcb->heap_root == NULL) {
    return -1;
  }
  
  if (vaddr < pcb->heap_vaddr_start || 
      vaddr >= pcb->heap_vaddr_start + (MEM_HEAP_PAGES * MEM_PAGESIZE)) {
    return -1;
  }
  
  offset = vaddr - pcb->heap_vaddr_start;
  node = find_node_by_addr((buddy_node_t *)pcb->heap_root, offset);
  
  if (node == NULL) {
    return -1;
  }
  
  if (!node->in_use) {
    return -1;
  }
  
  size = buddy_block_size(node->order);
  node->in_use = 0;
  
  printf("Freed the block: order = %d, addr = %d, size = %d\n",
         node->order, node->addr, size);
  
  coalesce_buddies(node);
  
  return size;
}