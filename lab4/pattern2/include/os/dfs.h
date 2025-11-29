#ifndef __DFS_H__
#define __DFS_H__

#include "dfs_shared.h"


#define DFS_CACHE_NUM_SLOTS 128


// the original uncached DFS functions
int DfsReadBlockUncached(uint32 blocknum, dfs_block *b);
int DfsWriteBlockUncached(uint32 blocknum, dfs_block *b);


//cache functions
int DfsCacheHit(int blocknum);
int DfsCacheAllocateSlot(int blocknum);
int DfsCacheFlush();

// int DfsReadBlock(uint32 blocknum, dfs_block *b);
// int DfsWriteBlock(uint32 blocknum, dfs_block *b);
int DfsOpenFileSystem();
int DfsCloseFileSystem();
int DfsInodeReadBytes(uint32 handle, void *mem, int start_byte, int num_bytes);
int DfsInodeWriteBytes(uint32 handle, void *mem, int start_byte, int num_bytes);
uint32 DfsInodeFilesize(uint32 handle);
uint32 DfsInodeAllocateVirtualBlock(uint32 handle, uint32 virtual_blocknum);
uint32 DfsInodeTranslateVirtualToFilesys(uint32 handle, uint32 virtual_blocknum);
uint32 DfsInodeFilesize(uint32 handle);
uint32 DfsInodeOpen(char *filename);
int DfsInodeDelete(uint32 handle);
uint32 DfsInodeFilenameExists(char *filename);
uint32 DfsAllocateBlock();
int DfsFreeBlock(uint32 blocknum);
void DfsModuleInit();
void DfsInvalidate()





#endif
