#include "ostraps.h"
#include "dlxos.h"
#include "traps.h"
#include "queue.h"
#include "disk.h"
#include "dfs.h"
#include "synch.h"

static dfs_inode inodes[DFS_INODE_MAX_NUM];
static dfs_superblock sb;
static uint32 fbv[DFS_FBV_MAX_NUM_WORDS];

static uint32 negativeone = 0xFFFFFFFF;
static inline uint32 invert(uint32 n) { return n ^ negativeone; }

// You have already been told about the most likely places where you should use locks. You may use 
// additional locks if it is really necessary.
static int dfs_open = 0;  // Flag to track if filesystem is open
static lock_t fbv_lock;   // Lock for free block vector operations
// STUDENT: put your file system level functions below.
// Some skeletons are provided. You can implement additional functions.

///////////////////////////////////////////////////////////////////
// Non-inode functions first
///////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------
// DfsModuleInit is called at boot time to initialize things and
// open the file system for use.
//-----------------------------------------------------------------

void DfsModuleInit() {
// You essentially set the file system as invalid and then open 
// using DfsOpenFileSystem().
  fbv_lock = LockCreate();
  dfs_open = 0;
  if (DfsOpenFileSystem() == DFS_FAIL) {
    printf("DfsModuleInit: Failed to open filesystem\n");
  }
}


//-----------------------------------------------------------------
// DfsInavlidate marks the current version of the filesystem in
// memory as invalid.  This is really only useful when formatting
// the disk, to prevent the current memory version from overwriting
// what you already have on the disk when the OS exits.
//-----------------------------------------------------------------

void DfsInvalidate() {
// This is just a one-line function which sets the valid bit of the 
// superblock to 0.
  sb.valid = 0;
  dfs_open = 0;


}

//-------------------------------------------------------------------
// DfsOpenFileSystem loads the file system metadata from the disk
// into memory.  Returns DFS_SUCCESS on success, and DFS_FAIL on 
// failure.
//-------------------------------------------------------------------

int DfsOpenFileSystem() {
//Basic steps:
  disk_block disk_blk;
  dfs_block dfs_blk;
  int i;
  int phys_blocks_per_fs;
// Check that filesystem is not already open
  if (dfs_open) {
    return DFS_FAIL;
  }

// Read superblock from disk.  Note this is using the disk read rather 
  if (DiskReadBlock(4, &disk_blk) == DISK_FAIL) {
    printf("DfsOpenFileSystem: Failed to read superblock\n");
    return DFS_FAIL;
  }
// than the DFS read function because the DFS read requires a valid 
// filesystem in memory already, and the filesystem cannot be valid 
// until we read the superblock. Also, we don't know the block size 
// until we read the superblock, either.

// Copy the data from the block we just read into the superblock in memory
// Copy superblock data
  bcopy(disk_blk.data, (char*)&sb, sizeof(dfs_superblock));
  
  // Check if valid
  if (sb.valid != 1) {
    printf("DfsOpenFileSystem: Filesystem not valid\n");
    return DFS_FAIL;
  }
  
  phys_blocks_per_fs = sb.blocksize / DiskBytesPerBlock();

// All other blocks are sized by virtual block size:
// Read inodes
// Read free block vector
// Read inodes
  for (i = 0; i < (sb.num_inodes * sizeof(dfs_inode) / sb.blocksize); i++) {
    if (DfsReadBlock(sb.inode_start + i, &dfs_blk) == DFS_FAIL) {
      printf("DfsOpenFileSystem: Failed to read inode block %d\n", i);
      return DFS_FAIL;
    }
    bcopy(dfs_blk.data, 
          (char*)(inodes + i * (sb.blocksize / sizeof(dfs_inode))),
          sb.blocksize);
  }
  
  // Read free block vector
  for (i = 0; i < (sb.num_blocks / (sb.blocksize * 8)); i++) {
    if (DfsReadBlock(sb.fbv_start + i, &dfs_blk) == DFS_FAIL) {
      printf("DfsOpenFileSystem: Failed to read FBV block %d\n", i);
      return DFS_FAIL;
    }
    bcopy(dfs_blk.data,
          (char*)(fbv + i * (sb.blocksize / sizeof(uint32))),
          sb.blocksize);
  }
// Change superblock to be invalid, write back to disk, then change 
// Invalidate disk copy
  sb.valid = 0;
  bcopy((char*)&sb, dfs_blk.data, sizeof(dfs_superblock));
  if (DfsWriteBlock(1, &dfs_blk) == DFS_FAIL) {
    printf("DfsOpenFileSystem: Failed to invalidate disk superblock\n");
    return DFS_FAIL;
  }
  
  // Also invalidate duplicate
  if (DfsWriteBlock(65535, &dfs_blk) == DFS_FAIL) {
    printf("DfsOpenFileSystem: Failed to invalidate duplicate superblock\n");
    return DFS_FAIL;
  }
// it back to be valid in memory
  sb.valid = 1;
  dfs_open = 1;
  return DFS_SUCCESS;

}


//-------------------------------------------------------------------
// DfsCloseFileSystem writes the current memory version of the
// filesystem metadata to the disk, and invalidates the memory's 
// version.
//-------------------------------------------------------------------


int DfsCloseFileSystem() {
  dfs_block dfs_blk;
  int i;
  
  if (!dfs_open) {
    return DFS_FAIL;
  }
  
  // Write inodes back
  for (i = 0; i < (sb.num_inodes * sizeof(dfs_inode) / sb.blocksize); i++) {
    bcopy((char*)(inodes + i * (sb.blocksize / sizeof(dfs_inode))),
          dfs_blk.data, sb.blocksize);
    if (DfsWriteBlock(sb.inode_start + i, &dfs_blk) == DFS_FAIL) {
      printf("DfsCloseFileSystem: Failed to write inode block %d\n", i);
      return DFS_FAIL;
    }
  }
  
  // Write FBV back
  for (i = 0; i < (sb.num_blocks / (sb.blocksize * 8)); i++) {
    bcopy((char*)(fbv + i * (sb.blocksize / sizeof(uint32))),
          dfs_blk.data, sb.blocksize);
    if (DfsWriteBlock(sb.fbv_start + i, &dfs_blk) == DFS_FAIL) {
      printf("DfsCloseFileSystem: Failed to write FBV block %d\n", i);
      return DFS_FAIL;
    }
  }
  
  // Write valid superblock last
  sb.valid = 1;
  bcopy((char*)&sb, dfs_blk.data, sizeof(dfs_superblock));
  if (DfsWriteBlock(1, &dfs_blk) == DFS_FAIL) {
    printf("DfsCloseFileSystem: Failed to write superblock\n");
    return DFS_FAIL;
  }
  
  // Write duplicate superblock
  if (DfsWriteBlock(65535, &dfs_blk) == DFS_FAIL) {
    printf("DfsCloseFileSystem: Failed to write duplicate superblock\n");
    return DFS_FAIL;
  }
  
  dfs_open = 0;
  return DFS_SUCCESS;
}



//-----------------------------------------------------------------
// DfsAllocateBlock allocates a DFS block for use. Remember to use 
// locks where necessary.
//-----------------------------------------------------------------

uint32 DfsAllocateBlock() {
// Check that file system has been validly loaded into memory
// Find the first free block using the free block vector (FBV), mark it in use
// Return handle to block
  int i, j;
  uint32 mask;
  
  if (!dfs_open) {
    return DFS_FAIL;
  }
  
  if (LockHandleAcquire(fbv_lock) != SYNC_SUCCESS) {
    return DFS_FAIL;
  }
  
  // Find first free block
  for (i = 0; i < DFS_FBV_MAX_NUM_WORDS; i++) {
    if (fbv[i] != 0xFFFFFFFF) {
      // Found a word with free block
      for (j = 0; j < 32; j++) {
        mask = 1 << j;
        if ((fbv[i] & mask) == 0) {
          // Found free block
          fbv[i] |= mask;
          LockHandleRelease(fbv_lock);
          return (i * 32 + j);
        }
      }
    }
  }
  
  LockHandleRelease(fbv_lock);
  return DFS_FAIL;  // No free blocks

}


//-----------------------------------------------------------------
// DfsFreeBlock deallocates a DFS block.
//-----------------------------------------------------------------

int DfsFreeBlock(uint32 blocknum) {
  int word_idx, bit_idx;
  uint32 mask;
  
  if (!dfs_open) {
    return DFS_FAIL;
  }
  
  if (blocknum >= sb.num_blocks) {
    return DFS_FAIL;
  }
  
  if (LockHandleAcquire(fbv_lock) != SYNC_SUCCESS) {
    return DFS_FAIL;
  }
  
  word_idx = blocknum / 32;
  bit_idx = blocknum % 32;
  mask = 1 << bit_idx;
  
  fbv[word_idx] &= ~mask;  // Clear the bit
  
  LockHandleRelease(fbv_lock);
  return DFS_SUCCESS;
}


//-----------------------------------------------------------------
// DfsReadBlock reads an allocated DFS block from the disk
// (which could span multiple physical disk blocks).  The block
// must be allocated in order to read from it.  Returns DFS_FAIL
// on failure, and the number of bytes read on success.  
//-----------------------------------------------------------------

int DfsReadBlock(uint32 blocknum, dfs_block *b) {
  int phys_blocks_per_fs = sb.blocksize / DiskBytesPerBlock();
  int i;
  disk_block disk_blk;
  
  if (!dfs_open) {
    return DFS_FAIL;
  }
  
  if (blocknum >= sb.num_blocks) {
    return DFS_FAIL;
  }
  
  // Read all physical blocks that make up this filesystem block
  for (i = 0; i < phys_blocks_per_fs; i++) {
    if (DiskReadBlock(blocknum * phys_blocks_per_fs + i, &disk_blk) == DISK_FAIL) {
      return DFS_FAIL;
    }
    bcopy(disk_blk.data, b->data + (i * DiskBytesPerBlock()), DiskBytesPerBlock());
  }
  
  return sb.blocksize;
}


//-----------------------------------------------------------------
// DfsWriteBlock writes to an allocated DFS block on the disk
// (which could span multiple physical disk blocks).  The block
// must be allocated in order to write to it.  Returns DFS_FAIL
// on failure, and the number of bytes written on success.  
//-----------------------------------------------------------------

int DfsWriteBlock(uint32 blocknum, dfs_block *b) {
  int phys_blocks_per_fs = sb.blocksize / DiskBytesPerBlock();
  int i;
  disk_block disk_blk;
  
  if (!dfs_open) {
    return DFS_FAIL;
  }
  
  if (blocknum >= sb.num_blocks) {
    return DFS_FAIL;
  }
  
  // Write all physical blocks that make up this filesystem block
  for (i = 0; i < phys_blocks_per_fs; i++) {
    bcopy(b->data + (i * DiskBytesPerBlock()), disk_blk.data, DiskBytesPerBlock());
    if (DiskWriteBlock(blocknum * phys_blocks_per_fs + i, &disk_blk) == DISK_FAIL) {
      return DFS_FAIL;
    }
  }
  
  return sb.blocksize;
}


////////////////////////////////////////////////////////////////////////////////
// Inode-based functions
////////////////////////////////////////////////////////////////////////////////

//-----------------------------------------------------------------
// DfsInodeFilenameExists looks through all the inuse inodes for 
// the given filename. If the filename is found, return the handle 
// of the inode. If it is not found, return DFS_FAIL.
//-----------------------------------------------------------------

uint32 DfsInodeFilenameExists(char *filename) {
    return DFS_FAIL;

}


//-----------------------------------------------------------------
// DfsInodeOpen: search the list of all inuse inodes for the 
// specified filename. If the filename exists, return the handle 
// of the inode. If it does not, allocate a new inode for this 
// filename and return its handle. Return DFS_FAIL on failure. 
// Remember to use locks whenever you allocate a new inode.
//-----------------------------------------------------------------

uint32 DfsInodeOpen(char *filename) {
    return DFS_FAIL;

}


//-----------------------------------------------------------------
// DfsInodeDelete de-allocates any data blocks used by this inode, 
// including the indirect addressing block if necessary, then mark 
// the inode as no longer in use. Use locks when modifying the 
// "inuse" flag in an inode.Return DFS_FAIL on failure, and 
// DFS_SUCCESS on success.
//-----------------------------------------------------------------

int DfsInodeDelete(uint32 handle) {
    return DFS_FAIL;

}


//-----------------------------------------------------------------
// DfsInodeReadBytes reads num_bytes from the file represented by 
// the inode handle, starting at virtual byte start_byte, copying 
// the data to the address pointed to by mem. Return DFS_FAIL on 
// failure, and the number of bytes read on success.
//-----------------------------------------------------------------

int DfsInodeReadBytes(uint32 handle, void *mem, int start_byte, int num_bytes) {
    return DFS_FAIL;

}


//-----------------------------------------------------------------
// DfsInodeWriteBytes writes num_bytes from the memory pointed to 
// by mem to the file represented by the inode handle, starting at 
// virtual byte start_byte. Note that if you are only writing part 
// of a given file system block, you'll need to read that block 
// from the disk first. Return DFS_FAIL on failure and the number 
// of bytes written on success.
//-----------------------------------------------------------------

int DfsInodeWriteBytes(uint32 handle, void *mem, int start_byte, int num_bytes) {

    return DFS_FAIL;

}


//-----------------------------------------------------------------
// DfsInodeFilesize simply returns the size of an inode's file. 
// This is defined as the maximum virtual byte number that has 
// been written to the inode thus far. Return DFS_FAIL on failure.
//-----------------------------------------------------------------

uint32 DfsInodeFilesize(uint32 handle) {
    return DFS_FAIL;

}


//-----------------------------------------------------------------
// DfsInodeAllocateVirtualBlock allocates a new filesystem block 
// for the given inode, storing its blocknumber at index 
// virtual_blocknumber in the translation table. If the 
// virtual_blocknumber resides in the indirect address space, and 
// there is not an allocated indirect addressing table, allocate it. 
// Return DFS_FAIL on failure, and the newly allocated file system 
// block number on success.
//-----------------------------------------------------------------

uint32 DfsInodeAllocateVirtualBlock(uint32 handle, uint32 virtual_blocknum) {
    return DFS_FAIL;


}



//-----------------------------------------------------------------
// DfsInodeTranslateVirtualToFilesys translates the 
// virtual_blocknum to the corresponding file system block using 
// the inode identified by handle. Return DFS_FAIL on failure.
//-----------------------------------------------------------------

uint32 DfsInodeTranslateVirtualToFilesys(uint32 handle, uint32 virtual_blocknum) {
    return DFS_FAIL;

}
