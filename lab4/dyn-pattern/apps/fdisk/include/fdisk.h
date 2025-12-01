#ifndef __FDISK_H__
#define __FDISK_H__

typedef unsigned int uint32;

#include "dfs_shared.h" // This gets us structures and #define's from main filesystem driver

#define FDISK_INODE_BLOCK_START 2 // Starts after super block (which is in file system block 0, physical block 1)
#define FDISK_INODE_NUM_BLOCKS 32 // Number of file system blocks to use for inodes
#define FDISK_NUM_INODES 256  //STUDENT: define this
#define FDISK_FBV_BLOCK_START FDISK_INODE_BLOCK_START+FDISK_INODE_NUM_BLOCKS  //STUDENT: define this
#define FDISK_FBV_NUM_BLOCKS 8 // Number of file system blocks to use for FBV: 8 blocks * 1024 bytes/block * 8 bits/byte = 65536 bits (enough for 64K blocks)
#define FDISK_BOOT_FILESYSTEM_BLOCKNUM 0 // Where the boot record and superblock reside in the filesystem

#ifndef NULL
#define NULL (void *)0x0
#endif

//STUDENT: define additional parameters here, if any
#define FDISK_DATA_BLOCK_START 42

#endif
