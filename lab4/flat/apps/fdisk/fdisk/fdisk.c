#include "usertraps.h"
#include "misc.h"

#include "fdisk.h"

dfs_superblock sb;
dfs_inode inodes[DFS_INODE_MAX_NUM];
uint32 fbv[DFS_FBV_MAX_NUM_WORDS];

int diskblocksize = 0; // These are global in order to speed things up
int disksize = 0;      // (i.e. fewer traps to OS to get the same number)

int FdiskWriteBlock(uint32 blocknum, dfs_block *b); //You can use your own function. This function 
//calls disk_write_block() to write physical blocks to disk

void main (int argc, char *argv[])
{
	// STUDENT: put your code here. Follow the guidelines below. They are just the main steps. 
	// You need to think of the finer details. You can use bzero() to zero out bytes in memory
  int i;
  dfs_block block;
  int num_filesystem_blocks;
  
  Printf("fdisk (%d): Formatting disk...\n", getpid());
  //Initializations and argc check

  // Need to invalidate filesystem before writing to it to make sure that the OS
  // doesn't wipe out what we do here with the old version in memory
  // You can use dfs_invalidate(); but it will be implemented in Problem 2. You can just do 
  sb.valid = 0;

  disksize = disk_size(); // in bytes
  diskblocksize = disk_blocksize();
  num_filesystem_blocks = disksize / DFS_BLOCKSIZE;

  Printf("fdisk (%d): disksize=%d, blocksize=%d, fs_blocks=%d\n", 
         getpid(), disksize, diskblocksize, num_filesystem_blocks);
  // Make sure the disk exists before doing anything else
  if (disk_create() == DISK_FAIL) {
    Printf("fdisk (%d): ERROR - could not create disk\n", getpid());
    Exit();
  }
 

  // Write all inodes as not in use and empty (all zeros)
  bzero((char*)inodes, sizeof(inodes));
  for (i = 0; i < FDISK_NUM_INODES; i++) {
    inodes[i].inuse = 0;
    inodes[i].filesize = 0;
    inodes[i].direct[0] = inodes[i].direct[1] = inodes[i].direct[2] = 0;
    inodes[i].direct[3] = inodes[i].direct[4] = inodes[i].direct[5] = 0;
    inodes[i].direct[6] = inodes[i].direct[7] = inodes[i].direct[8] = 0;
    inodes[i].direct[9] = 0;
    inodes[i].indirect = 0;
    inodes[i].double_indirect = 0;
  }
  for (i = 0; i < FDISK_INODE_NUM_BLOCKS; i++) {
    bcopy((char*)(inodes + i * (DFS_BLOCKSIZE/sizeof(dfs_inode))), 
          block.data, DFS_BLOCKSIZE);
    if (FdiskWriteBlock(FDISK_INODE_BLOCK_START + i, &block) == DISK_FAIL) {
      Printf("fdisk (%d): ERROR writing inode block %d\n", getpid(), i);
      Exit();
    }
  }
  // Next, setup free block vector (fbv) and write free block vector to the disk
  bzero((char*)fbv, sizeof(fbv));
  for (i = 0; i < 42; i++) {
    fbv[i / 32] |= (1 << (i % 32));  // Mark as in use
  }
  
  // Write FBV to disk (blocks 34-41)
  for (i = 0; i < FDISK_FBV_NUM_BLOCKS; i++) {
    bcopy((char*)(fbv + i * (DFS_BLOCKSIZE/sizeof(uint32))), 
          block.data, DFS_BLOCKSIZE);
    if (FdiskWriteBlock(FDISK_FBV_BLOCK_START + i, &block) == DISK_FAIL) {
      Printf("fdisk (%d): ERROR writing FBV block %d\n", getpid(), i);
      Exit();
    }
  }

  sb.valid = 1;
  sb.blocksize = DFS_BLOCKSIZE;
  sb.num_blocks = num_filesystem_blocks;
  sb.inode_start = FDISK_INODE_BLOCK_START;
  sb.num_inodes = FDISK_NUM_INODES;
  sb.fbv_start = FDISK_FBV_BLOCK_START;
  // Finally, setup superblock as valid filesystem and write superblock and boot record to disk: 
  bzero(block.data, DFS_BLOCKSIZE);
  if (FdiskWriteBlock(0, &block) == DISK_FAIL) {
    Printf("fdisk (%d): ERROR writing boot block\n", getpid());
    Exit();
  }
  
  // Write superblock to block 1
  bcopy((char*)&sb, block.data, sizeof(sb));
  if (FdiskWriteBlock(1, &block) == DISK_FAIL) {
    Printf("fdisk (%d): ERROR writing superblock\n", getpid());
    Exit();
  }

  // boot record is all zeros in the first physical block, and superblock structure goes into the second physical block
  // Write duplicate superblock to last block (65535)
  if (FdiskWriteBlock(65535, &block) == DISK_FAIL) {
    Printf("fdisk (%d): ERROR writing duplicate superblock\n", getpid());
    Exit();
  }
  Printf("fdisk (%d): Formatted DFS disk for %d bytes.\n", getpid(), disksize);
}

int FdiskWriteBlock(uint32 blocknum, dfs_block *b) {
  // STUDENT: put your code here
  int phys_block_size = diskblocksize;
  int fs_block_size = DFS_BLOCKSIZE;
  int phys_blocks_per_fs = fs_block_size / phys_block_size;
  int i;
  disk_block phys_block;
  
  // Write each physical block that makes up this filesystem block
  for (i = 0; i < phys_blocks_per_fs; i++) {
    bcopy(b->data + (i * phys_block_size), phys_block.data, phys_block_size);
    if (disk_write_block(blocknum * phys_blocks_per_fs + i, &phys_block) == DISK_FAIL) {
      return DISK_FAIL;
    }
  }
  return fs_block_size;
}
