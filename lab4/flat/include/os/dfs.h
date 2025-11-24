#ifndef __DFS_H__
#define __DFS_H__

#include "dfs_shared.h"

int DfsReadBlock(uint32 blocknum, dfs_block *b);
int DfsWriteBlock(uint32 blocknum, dfs_block *b);
int DfsOpenFileSystem();
int DfsCloseFileSystem();
int DfsInodeReadBytes(uint32 handle, void *mem, int start_byte, int num_bytes);
int DfsInodeWriteBytes(uint32 handle, void *mem, int start_byte, int num_bytes);
uint32 DfsInodeFilesize(uint32 handle);
uint32 DfsInodeAllocateVirtualBlock(uint32 handle, uint32 virtual_blocknum);



#endif
