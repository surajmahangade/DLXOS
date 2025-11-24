#ifndef __DFS_SHARED__
#define __DFS_SHARED__

typedef struct dfs_superblock {
  // STUDENT: put superblock internals here
  int valid;                    // Filesystem validity flag
  int blocksize;               // Filesystem block size
  int num_blocks;              // Total filesystem blocks
  int inode_start;             // Starting block for inodes
  int num_inodes;              // Number of inodes
  int fbv_start;               // Starting block for FBV
} dfs_superblock;

#define DFS_BLOCKSIZE 1024  // Must be an integer multiple of the disk blocksize
#define DFS_MAX_FILENAME_LENGTH 68 

typedef struct dfs_block {
  char data[DFS_BLOCKSIZE];
} dfs_block;

typedef struct dfs_inode {
  // STUDENT: put inode structure internals here
  // IMPORTANT: sizeof(dfs_inode) MUST return 128 in order to fit in enough
  // inodes in the filesystem (and to make your life easier).  To do this, 
  // adjust the maximumm length of the filename until the size of the overall inode 
  // is 128 bytes.
  int inuse;                   // Is inode in use?
  uint32 filesize;             // File size in bytes
  char filename[DFS_MAX_FILENAME_LENGTH];           // Filename (adjusted for 128 byte total)
  uint32 direct[10];           // Direct block pointers
  uint32 indirect;             // Indirect block pointer
  uint32 double_indirect;
} dfs_inode;

#define DFS_MAX_FILESYSTEM_SIZE 0x4000000  // 64MB
#define DFS_INODE_MAX_NUM 256               // Maximum number of inodes in filesystem
#define DFS_FBV_MAX_NUM_WORDS 2048 // Number of words needed for FBV

#define DFS_FAIL -1
#define DFS_SUCCESS 1



#endif
