#ifndef	_memory_constants_h_
#define	_memory_constants_h_

//------------------------------------------------
// #define's that you are given:
//------------------------------------------------

// We can read this address in I/O space to figure out how much memory
// is available on the system.
#define	DLX_MEMSIZE_ADDRESS	0xffff0000

// Return values for success and failure of functions
#define MEM_SUCCESS 1
#define MEM_FAIL -1

//--------------------------------------------------------
// Put your constant definitions related to memory here.
// Be sure to prepend any constant names with "MEM_" so 
// that the grader knows they are defined in this file.

//--------------------------------------------------------

// Page offset uses 12 bits (2^12 = 4096 bytes per page)
#define MEM_L1FIELD_FIRST_BITNUM 12

// Maximum virtual address (4MB - 1)
// Max address = 4,194,303 = 0x3FFFFF
#define MEM_MAX_VIRTUAL_ADDRESS 0x003FFFFF

// Maximum physical memory (2MB)
#define MEM_MAX_PHYS_MEM (2 * 1024 * 1024)

// PTE status bits
#define MEM_PTE_READONLY 0x4
#define MEM_PTE_DIRTY    0x2
#define MEM_PTE_VALID    0x1

// Page size = 2^12 = 4096 bytes
#define MEM_PAGESIZE (0x1 << MEM_L1FIELD_FIRST_BITNUM)

// Number of entries in L1 page table
// (4MB) / (4KB) = 1024 entries
#define MEM_L1TABLE_SIZE ((MEM_MAX_VIRTUAL_ADDRESS + 1) >> MEM_L1FIELD_FIRST_BITNUM)

// Maximum number of physical pages
// (2MB) / (4KB) = 512 pages
#define MEM_MAX_PAGES (MEM_MAX_PHYS_MEM >> MEM_L1FIELD_FIRST_BITNUM)

// Mask to extract page offset from virtual address
#define MEM_PAGE_OFFSET_MASK (MEM_PAGESIZE - 1)

// Mask to convert PTE to physical page address
#define MEM_ADDRESS_OFFSET_MASK (~(MEM_PTE_READONLY | MEM_PTE_DIRTY | MEM_PTE_VALID))

// Maximum page number in virtual address space
#define MEM_MAX_VIRTUAL_PAGE (MEM_L1TABLE_SIZE - 1)

#endif	// _memory_constants_h_
