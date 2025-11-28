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
static lock_t inode_lock;  // global variable related to Q3
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
  inode_lock = LockCreate(); // global variable related to Q3
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
  if (DiskWriteBlock(4, &dfs_blk) == DFS_FAIL) { // physical block 4
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
  if (DiskWriteBlock(4, &dfs_blk) == DFS_FAIL) { // physical block 4
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
  int i;
  if (!dfs_open) {
    return DFS_FAIL;
  }
  
  for (i = 0; i < sb.num_inodes; i++) {
    if (inodes[i].inuse && dstrncmp(inodes[i].filename, filename, 44) == 0) {
      return i;
    }
  }
  
  return DFS_FAIL;
}


// creating rename function

int DfsInodeRename(uint32 handle, char *newname) {
  if (!dfs_open) {
    return DFS_FAIL;
  }
  
  if (handle >= sb.num_inodes) {
    return DFS_FAIL;
  }
  
  if (!inodes[handle].inuse) {
    return DFS_FAIL;
  }
  
  // Check if new name already exists
  if (DfsInodeFilenameExists(newname) != DFS_FAIL) {
    return DFS_FAIL;  // New name already in use
  }
  
  // Copy new name to inode
  dstrncpy(inodes[handle].filename, newname, DFS_MAX_FILENAME_LENGTH);
  
  return DFS_SUCCESS;
}


//-----------------------------------------------------------------
// DfsInodeOpen: search the list of all inuse inodes for the 
// specified filename. If the filename exists, return the handle 
// of the inode. If it does not, allocate a new inode for this 
// filename and return its handle. Return DFS_FAIL on failure. 
// Remember to use locks whenever you allocate a new inode.
//-----------------------------------------------------------------

uint32 DfsInodeOpen(char *filename) {
  int i;
  int j;
  uint32 handle;
  
  if (!dfs_open) {
    return DFS_FAIL;
  }
  
  // Check if file already exists
  handle = DfsInodeFilenameExists(filename);
  if (handle != DFS_FAIL) {
    return handle;
  }
  
  // Allocate new inode
  if (LockHandleAcquire(inode_lock) != SYNC_SUCCESS) {
    return DFS_FAIL;
  }
  
  for (i = 0; i < sb.num_inodes; i++) {
    if (!inodes[i].inuse) {
      inodes[i].inuse = 1;
      inodes[i].filesize = 0;
      dstrncpy(inodes[i].filename, filename, 44);
      for (j = 0; j < 10; j++) {
        inodes[i].direct[j] = 0;
      }
      inodes[i].indirect = 0;
      inodes[i].double_indirect = 0;
      LockHandleRelease(inode_lock);
      return i;
    }
  }
  
  LockHandleRelease(inode_lock);
  return DFS_FAIL;  // No free inodes
}


//-----------------------------------------------------------------
// DfsInodeDelete de-allocates any data blocks used by this inode, 
// including the indirect addressing block if necessary, then mark 
// the inode as no longer in use. Use locks when modifying the 
// "inuse" flag in an inode.Return DFS_FAIL on failure, and 
// DFS_SUCCESS on success.
//-----------------------------------------------------------------

int DfsInodeDelete(uint32 handle) {
  int i;
  int j;
  dfs_block blk;
  uint32 *indirect_table;
  uint32 *double_indirect_table;
  uint32 indirect_block;
  
  if (!dfs_open || handle >= sb.num_inodes) {
    return DFS_FAIL;
  }
  
  if (!inodes[handle].inuse) {
    return DFS_FAIL;
  }
  
  // Free direct blocks
  for (i = 0; i < 10; i++) {
    if (inodes[handle].direct[i] != 0) {
      DfsFreeBlock(inodes[handle].direct[i]);
      inodes[handle].direct[i] = 0;
    }
  }
  
  // Free indirect blocks
  if (inodes[handle].indirect != 0) {
    DfsReadBlock(inodes[handle].indirect, &blk);
    indirect_table = (uint32*)blk.data;
    for (i = 0; i < (sb.blocksize / sizeof(uint32)); i++) {
      if (indirect_table[i] != 0) {
        DfsFreeBlock(indirect_table[i]);
      }
    }
    DfsFreeBlock(inodes[handle].indirect);
    inodes[handle].indirect = 0;
  }
  
  // Free double indirect blocks
  if (inodes[handle].double_indirect != 0) {
    DfsReadBlock(inodes[handle].double_indirect, &blk);
    double_indirect_table = (uint32*)blk.data;
    for (i = 0; i < (sb.blocksize / sizeof(uint32)); i++) {
      if (double_indirect_table[i] != 0) {
        indirect_block = double_indirect_table[i];
        DfsReadBlock(indirect_block, &blk);
        indirect_table = (uint32*)blk.data;
        for (j = 0; j < (sb.blocksize / sizeof(uint32)); j++) {
          if (indirect_table[j] != 0) {
            DfsFreeBlock(indirect_table[j]);
          }
        }
        DfsFreeBlock(indirect_block);
      }
    }
    DfsFreeBlock(inodes[handle].double_indirect);
    inodes[handle].double_indirect = 0;
  }
  
  // Mark inode as free
  if (LockHandleAcquire(inode_lock) != SYNC_SUCCESS) {
    return DFS_FAIL;
  }
  inodes[handle].inuse = 0;
  inodes[handle].filesize = 0;
  LockHandleRelease(inode_lock);
  
  return DFS_SUCCESS;
}



//-----------------------------------------------------------------
// DfsInodeReadBytes reads num_bytes from the file represented by 
// the inode handle, starting at virtual byte start_byte, copying 
// the data to the address pointed to by mem. Return DFS_FAIL on 
// failure, and the number of bytes read on success.
//-----------------------------------------------------------------

int DfsInodeReadBytes(uint32 handle, void *mem, int start_byte, int num_bytes) {
  int bytes_read = 0;
  int virtual_block, block_offset;
  uint32 fs_block;
  dfs_block blk;
  int bytes_to_read;
  
  if (!dfs_open || handle >= sb.num_inodes || !inodes[handle].inuse) {
    return DFS_FAIL;
  }
  
  if (start_byte >= inodes[handle].filesize) {
    return 0;
  }
  
  if (start_byte + num_bytes > inodes[handle].filesize) {
    num_bytes = inodes[handle].filesize - start_byte;
  }
  
  while (bytes_read < num_bytes) {
    virtual_block = (start_byte + bytes_read) / sb.blocksize;
    block_offset = (start_byte + bytes_read) % sb.blocksize;
    
    fs_block = DfsInodeTranslateVirtualToFilesys(handle, virtual_block);
    if (fs_block == 0 || fs_block == DFS_FAIL) {
      return DFS_FAIL;
    }
    
    if (DfsReadBlock(fs_block, &blk) == DFS_FAIL) {
      return DFS_FAIL;
    }
    
    bytes_to_read = min(num_bytes - bytes_read, sb.blocksize - block_offset);
    bcopy(blk.data + block_offset, (char*)mem + bytes_read, bytes_to_read);
    bytes_read += bytes_to_read;
  }
  
  return bytes_read;
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
  int bytes_written = 0;
  int virtual_block, block_offset;
  uint32 fs_block;
  dfs_block blk;
  int bytes_to_write;
  
  if (!dfs_open || handle >= sb.num_inodes || !inodes[handle].inuse) {
    return DFS_FAIL;
  }
  
  while (bytes_written < num_bytes) {
    virtual_block = (start_byte + bytes_written) / sb.blocksize;
    block_offset = (start_byte + bytes_written) % sb.blocksize;
    
    fs_block = DfsInodeTranslateVirtualToFilesys(handle, virtual_block);
    
    if (fs_block == 0 || fs_block == DFS_FAIL) {
      fs_block = DfsInodeAllocateVirtualBlock(handle, virtual_block);
      if (fs_block == DFS_FAIL) {
        return DFS_FAIL;
      }
      bzero(blk.data, sb.blocksize);
    } else {
      if (DfsReadBlock(fs_block, &blk) == DFS_FAIL) {
        return DFS_FAIL;
      }
    }
    
    bytes_to_write = min(num_bytes - bytes_written, sb.blocksize - block_offset);
    bcopy((char*)mem + bytes_written, blk.data + block_offset, bytes_to_write);
    
    if (DfsWriteBlock(fs_block, &blk) == DFS_FAIL) {
      return DFS_FAIL;
    }
    
    bytes_written += bytes_to_write;
  }
  
  if (start_byte + num_bytes > inodes[handle].filesize) {
    inodes[handle].filesize = start_byte + num_bytes;
  }
  
  return bytes_written;
}


//-----------------------------------------------------------------
// DfsInodeFilesize simply returns the size of an inode's file. 
// This is defined as the maximum virtual byte number that has 
// been written to the inode thus far. Return DFS_FAIL on failure.
//-----------------------------------------------------------------

uint32 DfsInodeFilesize(uint32 handle) {
  if (!dfs_open || handle >= sb.num_inodes || !inodes[handle].inuse) {
    return DFS_FAIL;
  }
  return inodes[handle].filesize;
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
  uint32 new_block;
  dfs_block blk;
  uint32 *table;
  uint32 indirect_block;
  int entries_per_block = sb.blocksize / sizeof(uint32);
  
  if (!dfs_open || handle >= sb.num_inodes || !inodes[handle].inuse) {
    return DFS_FAIL;
  }
  
  new_block = DfsAllocateBlock();
  if (new_block == DFS_FAIL) {
    return DFS_FAIL;
  }
  
  // Direct blocks
  if (virtual_blocknum < 10) {
    inodes[handle].direct[virtual_blocknum] = new_block;
    return new_block;
  }
  
  // Single indirect
  if (virtual_blocknum < 10 + entries_per_block) {
    if (inodes[handle].indirect == 0) {
      inodes[handle].indirect = DfsAllocateBlock();
      if (inodes[handle].indirect == DFS_FAIL) {
        DfsFreeBlock(new_block);
        return DFS_FAIL;
      }
      bzero(blk.data, sb.blocksize);
      DfsWriteBlock(inodes[handle].indirect, &blk);
    }
    DfsReadBlock(inodes[handle].indirect, &blk);
    table = (uint32*)blk.data;
    table[virtual_blocknum - 10] = new_block;
    DfsWriteBlock(inodes[handle].indirect, &blk);
    return new_block;
  }
  
  // Double indirect
  virtual_blocknum -= (10 + entries_per_block);
  
  if (inodes[handle].double_indirect == 0) {
    inodes[handle].double_indirect = DfsAllocateBlock();
    if (inodes[handle].double_indirect == DFS_FAIL) {
      DfsFreeBlock(new_block);
      return DFS_FAIL;
    }
    bzero(blk.data, sb.blocksize);
    DfsWriteBlock(inodes[handle].double_indirect, &blk);
  }
  
  DfsReadBlock(inodes[handle].double_indirect, &blk);
  table = (uint32*)blk.data;
  
  if (table[virtual_blocknum / entries_per_block] == 0) {
    table[virtual_blocknum / entries_per_block] = DfsAllocateBlock();
    if (table[virtual_blocknum / entries_per_block] == DFS_FAIL) {
      DfsFreeBlock(new_block);
      return DFS_FAIL;
    }
    DfsWriteBlock(inodes[handle].double_indirect, &blk);
    bzero(blk.data, sb.blocksize);
    DfsWriteBlock(table[virtual_blocknum / entries_per_block], &blk);
  }
  
  indirect_block = table[virtual_blocknum / entries_per_block];
  DfsReadBlock(indirect_block, &blk);
  table = (uint32*)blk.data;
  table[virtual_blocknum % entries_per_block] = new_block;
  DfsWriteBlock(indirect_block, &blk);
  
  return new_block;
}



//-----------------------------------------------------------------
// DfsInodeTranslateVirtualToFilesys translates the 
// virtual_blocknum to the corresponding file system block using 
// the inode identified by handle. Return DFS_FAIL on failure.
//-----------------------------------------------------------------

uint32 DfsInodeTranslateVirtualToFilesys(uint32 handle, uint32 virtual_blocknum) {
  dfs_block blk;
  uint32 *table;
  uint32 indirect_block;
  int entries_per_block = sb.blocksize / sizeof(uint32);
  
  if (!dfs_open || handle >= sb.num_inodes || !inodes[handle].inuse) {
    return DFS_FAIL;
  }
  
  // Direct blocks (0-9)
  if (virtual_blocknum < 10) {
    return inodes[handle].direct[virtual_blocknum];
  }
  
  // Single indirect (10 - 10+entries_per_block-1)
  if (virtual_blocknum < 10 + entries_per_block) {
    if (inodes[handle].indirect == 0) {
      return 0;
    }
    DfsReadBlock(inodes[handle].indirect, &blk);
    table = (uint32*)blk.data;
    return table[virtual_blocknum - 10];
  }
  
  // Double indirect
  if (inodes[handle].double_indirect == 0) {
    return 0;
  }
  
  virtual_blocknum -= (10 + entries_per_block);
  DfsReadBlock(inodes[handle].double_indirect, &blk);
  table = (uint32*)blk.data;
  indirect_block = table[virtual_blocknum / entries_per_block];
  
  if (indirect_block == 0) {
    return 0;
  }
  
  DfsReadBlock(indirect_block, &blk);
  table = (uint32*)blk.data;
  return table[virtual_blocknum % entries_per_block];
}
