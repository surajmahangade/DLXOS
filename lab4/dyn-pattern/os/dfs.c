#include "ostraps.h"
#include "dlxos.h"
#include "traps.h"
#include "queue.h"
#include "disk.h"
#include "dfs.h"
#include "synch.h"

// Additional cache metadata
typedef struct adaptive_cache_entry {
  int valid;
  int dirty;
  uint32 blocknum;
  dfs_block data;
  uint32 timestamp;
  uint32 access_count;      // How many times accessed
  int reference_bit;         // For clock algorithm
} adaptive_cache_entry;



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
static lock_t cache_lock; // Lock for cache operations



static uint32 access_history[PATTERN_WINDOW_SIZE];
static int history_index = 0;


// Global adaptive state
static adaptive_cache_entry adaptive_cache[DFS_CACHE_NUM_SLOTS];
static AccessPattern current_pattern = PATTERN_UNKNOWN;
static uint32 last_accessed_block = 0;
static int sequential_count = 0;
static int random_count = 0;
static int loop_detected = 0;




// Statistics
static uint32 cache_hits = 0;
static uint32 cache_misses = 0;
static uint32 disk_reads = 0;
static uint32 disk_writes = 0;
static uint32 cache_clock = 0;
static uint32 total_miss_latency = 0;
// STUDENT: put your file system level functions below.
// Some skeletons are provided. You can implement additional functions.

inline int min(int a, int b) {
  return (a < b) ? a : b;
}

///////////////////////////////////////////////////////////////////
// Non-inode functions first
///////////////////////////////////////////////////////////////////

// Add near the top of dfs.c, after the includes and before DfsModuleInit

//-----------------------------------------------------------------
// Helper function to sleep for specified milliseconds
//-----------------------------------------------------------------
// static void sleep_ms(int milliseconds) {
//   int start_jiffies;
//   int sleep_jiffies;
//   EnableIntrs();
//   start_jiffies = ClkGetCurJiffies();
//   sleep_jiffies = (milliseconds * 1000) / ClkGetResolution(); // Convert ms to jiffies
  
//   // Busy wait (simple implementation)
//   while ((ClkGetCurJiffies() - start_jiffies) < sleep_jiffies) {
//     // Just wait
//     // print sleep ClkGetCurJiffies() - start_jiffies;
//     // printf("current jiffies: %d\n", ClkGetCurJiffies() - start_jiffies);

//   }
//   DisableIntrs();
// }

static void sleep_ms(int milliseconds) {
  // Fallback latency simulation without relying on interrupts or yields.
  // We simply burn cycles proportional to milliseconds.
  // Note: This does not advance jiffies; we only use it to emulate disk delay.
  volatile uint32 i;
  uint32 loops = milliseconds * 5000; // tuned constant; adjust if too fast/slow
  for (i = 0; i < loops; i++) {
    // busy work
  }
}

//-----------------------------------------------------------------
// Helper function to get current time in milliseconds
//-----------------------------------------------------------------
static uint32 GetCurrentTime() {
  // Convert jiffies to milliseconds using 64-bit math to avoid overflow
  unsigned long long j = (unsigned long long)ClkGetCurJiffies();
  unsigned long long res_us = (unsigned long long)ClkGetResolution();
  return (uint32)((j * res_us) / 1000ULL);
}

//-----------------------------------------------------------------
// DfsModuleInit is called at boot time to initialize things and
// open the file system for use.
//-----------------------------------------------------------------

void DfsModuleInit() {
  int i;
  
  fbv_lock = LockCreate();
  inode_lock = LockCreate();
  cache_lock = LockCreate();
  dfs_open = 0;
  
  // Initialize adaptive cache
  for (i = 0; i < DFS_CACHE_NUM_SLOTS; i++) {
    adaptive_cache[i].valid = 0;
    adaptive_cache[i].dirty = 0;
    adaptive_cache[i].blocknum = 0;
    adaptive_cache[i].timestamp = 0;
    adaptive_cache[i].access_count = 0;
    adaptive_cache[i].reference_bit = 0;
  }
  
  for (i = 0; i < PATTERN_WINDOW_SIZE; i++) {
    access_history[i] = 0;
  }
  
  current_pattern = PATTERN_UNKNOWN;
  sequential_count = 0;
  random_count = 0;
  loop_detected = 0;
  last_accessed_block = 0;
  history_index = 0;
  
  cache_hits = 0;
  cache_misses = 0;
  disk_reads = 0;
  disk_writes = 0;
  cache_clock = 0;
  
  if (DfsOpenFileSystem() == DFS_FAIL) {
    printf("DfsModuleInit: Failed to open filesystem\n");
  }
}

void DetectAccessPattern(uint32 blocknum) {
  int i, j;
  int sequential_streak = 0;
  int loop_count = 0;
  int unique_blocks = 0;
  
  // Add to history
  access_history[history_index] = blocknum;
  history_index = (history_index + 1) % PATTERN_WINDOW_SIZE;
  
  // Detect sequential access
  if (blocknum == last_accessed_block + 1 || blocknum == last_accessed_block - 1) {
    sequential_count++;
    random_count = 0;
  } else if (blocknum != last_accessed_block) {
    random_count++;
    if (random_count > 5) {
      sequential_count = 0;
    }
  }
  
  last_accessed_block = blocknum;
  
  // Analyze access history every PATTERN_WINDOW_SIZE accesses
  if (cache_clock % PATTERN_WINDOW_SIZE == 0 && cache_clock > 0) {
    // Count sequential accesses in history
    for (i = 1; i < PATTERN_WINDOW_SIZE; i++) {
      if (access_history[i] == access_history[i-1] + 1 || 
          access_history[i] == access_history[i-1] - 1) {
        sequential_streak++;
      }
    }
    
    // Count unique blocks (for working set size)
    unique_blocks = 0;
    for (i = 0; i < PATTERN_WINDOW_SIZE; i++) {
      int is_unique = 1;
      for (j = 0; j < i; j++) {
        if (access_history[i] == access_history[j]) {
          is_unique = 0;
          break;
        }
      }
      if (is_unique) unique_blocks++;
    }
    
    // Detect loops (same block accessed multiple times)
    for (i = 0; i < PATTERN_WINDOW_SIZE; i++) {
      for (j = i + 1; j < PATTERN_WINDOW_SIZE; j++) {
        if (access_history[i] == access_history[j]) {
          loop_count++;
        }
      }
    }
    
    // Determine pattern
    if (sequential_streak > PATTERN_WINDOW_SIZE * 0.7) {
      current_pattern = PATTERN_SEQUENTIAL;
      printf("Pattern Detected: SEQUENTIAL\n");
    } else if (loop_count > PATTERN_WINDOW_SIZE * 0.3) {
      current_pattern = PATTERN_LOOPING;
      printf("Pattern Detected: LOOPING\n");
    } else if (unique_blocks < PATTERN_WINDOW_SIZE * 0.3) {
      current_pattern = PATTERN_TEMPORAL;
      printf("Pattern Detected: TEMPORAL (small working set)\n");
    } else {
      current_pattern = PATTERN_RANDOM;
      printf("Pattern Detected: RANDOM\n");
    }
  }
}
// Adaptive cache hit check
int DfsAdaptiveCacheHit(int blocknum) {
  int i;
  
  for (i = 0; i < DFS_CACHE_NUM_SLOTS; i++) {
    if (adaptive_cache[i].valid && adaptive_cache[i].blocknum == blocknum) {
      adaptive_cache[i].access_count++;
      adaptive_cache[i].reference_bit = 1;  // For clock algorithm
      return i;
    }
  }
  
  return DFS_FAIL;
}

// Adaptive cache allocation with pattern-aware replacement
int DfsAdaptiveCacheAllocateSlot(int blocknum) {
  int i;
  static int clock_hand = 0;  // For clock algorithm
  int evict_slot = -1;
  AccessPattern effective_pattern;
  uint32 total_accesses;
  
  // First, look for empty slot
  for (i = 0; i < DFS_CACHE_NUM_SLOTS; i++) {
    if (!adaptive_cache[i].valid) {
      adaptive_cache[i].valid = 1;
      adaptive_cache[i].dirty = 0;
      adaptive_cache[i].blocknum = blocknum;
      adaptive_cache[i].timestamp = cache_clock++;
      adaptive_cache[i].access_count = 0;
      adaptive_cache[i].reference_bit = 1;
      return i;
    }
  }
  
  // Choose replacement policy based on detected pattern
  // switch (current_pattern) {
  //   case PATTERN_SEQUENTIAL:
  //     // Use FIFO for sequential access
  //     evict_slot = clock_hand;
  //     clock_hand = (clock_hand + 1) % DFS_CACHE_NUM_SLOTS;
  //     break;
      
  //   case PATTERN_LOOPING:
  //     // Use MRU for looping patterns
  //     {
  //       uint32 max_time = 0;
  //       for (i = 0; i < DFS_CACHE_NUM_SLOTS; i++) {
  //         if (adaptive_cache[i].timestamp > max_time) {
  //           max_time = adaptive_cache[i].timestamp;
  //           evict_slot = i;
  //         }
  //       }
  //     }
  //     break;
      
  //   case PATTERN_TEMPORAL:
  //     // Use LRU for temporal locality
  //     {
  //       uint32 min_time = adaptive_cache[0].timestamp;
  //       evict_slot = 0;
  //       for (i = 1; i < DFS_CACHE_NUM_SLOTS; i++) {
  //         if (adaptive_cache[i].timestamp < min_time) {
  //           min_time = adaptive_cache[i].timestamp;
  //           evict_slot = i;
  //         }
  //       }
  //     }
  //     break;
      
  //   case PATTERN_RANDOM:
  //   case PATTERN_UNKNOWN:
  //   default:
  //     // Use Clock (Second Chance) algorithm as default
  //     while (1) {
  //       if (adaptive_cache[clock_hand].reference_bit == 0) {
  //         evict_slot = clock_hand;
  //         clock_hand = (clock_hand + 1) % DFS_CACHE_NUM_SLOTS;
  //         break;
  //       }
  //       adaptive_cache[clock_hand].reference_bit = 0;  // Give second chance
  //       clock_hand = (clock_hand + 1) % DFS_CACHE_NUM_SLOTS;
  //     }
  //     break;
  // }
  

    // Choose replacement policy based on detected pattern + simple thrash guard
  effective_pattern = current_pattern;
  total_accesses = cache_hits + cache_misses;

  // If we are clearly thrashing (very low hit rate), stop trusting the classifier
  if (total_accesses > (uint32)(DFS_CACHE_NUM_SLOTS * 4)) {
    // hit_rate < ~20%  => fall back to Clock (robust default)
    if (cache_hits * 5 < cache_misses) {
      effective_pattern = PATTERN_RANDOM;  // treated as "default" below
    }
  }

  switch (effective_pattern) {
    case PATTERN_SEQUENTIAL:
      // Sequential scan: evict MRU (the page we just used is least likely to be reused)
      {
        uint32 newest_time = adaptive_cache[0].timestamp;
        evict_slot = 0;
        for (i = 1; i < DFS_CACHE_NUM_SLOTS; i++) {
          if (adaptive_cache[i].timestamp > newest_time) {
            newest_time = adaptive_cache[i].timestamp;
            evict_slot = i;
          }
        }
      }
      break;

    case PATTERN_LOOPING:
      // Looping: evict LFU-ish (lowest access_count), tie-break by oldest timestamp
      {
        uint32 min_access = adaptive_cache[0].access_count;
        uint32 oldest_time = adaptive_cache[0].timestamp;
        evict_slot = 0;
        for (i = 1; i < DFS_CACHE_NUM_SLOTS; i++) {
          if (adaptive_cache[i].access_count < min_access ||
              (adaptive_cache[i].access_count == min_access &&
               adaptive_cache[i].timestamp < oldest_time)) {
            min_access = adaptive_cache[i].access_count;
            oldest_time = adaptive_cache[i].timestamp;
            evict_slot = i;
          }
        }
      }
      break;

    case PATTERN_TEMPORAL:
      // Temporal locality: classic LRU
      {
        uint32 min_time = adaptive_cache[0].timestamp;
        evict_slot = 0;
        for (i = 1; i < DFS_CACHE_NUM_SLOTS; i++) {
          if (adaptive_cache[i].timestamp < min_time) {
            min_time = adaptive_cache[i].timestamp;
            evict_slot = i;
          }
        }
      }
      break;

    case PATTERN_RANDOM:
    case PATTERN_UNKNOWN:
    default:
      // Default: Clock (Second Chance) as a safe general-purpose policy
      while (1) {
        if (adaptive_cache[clock_hand].reference_bit == 0) {
          evict_slot = clock_hand;
          clock_hand = (clock_hand + 1) % DFS_CACHE_NUM_SLOTS;
          break;
        }
        adaptive_cache[clock_hand].reference_bit = 0;  // second chance used
        clock_hand = (clock_hand + 1) % DFS_CACHE_NUM_SLOTS;
      }
      break;
  }

  // Evict chosen slot
  if (adaptive_cache[evict_slot].dirty) {
    if (DfsWriteBlockUncached(adaptive_cache[evict_slot].blocknum, 
                               &adaptive_cache[evict_slot].data) == DFS_FAIL) {
      return DFS_FAIL;
    }
  }
  
  adaptive_cache[evict_slot].valid = 1;
  adaptive_cache[evict_slot].dirty = 0;
  adaptive_cache[evict_slot].blocknum = blocknum;
  adaptive_cache[evict_slot].timestamp = cache_clock++;
  adaptive_cache[evict_slot].access_count = 0;
  adaptive_cache[evict_slot].reference_bit = 1;
  
  return evict_slot;
}

// Adaptive cache flush
int DfsAdaptiveCacheFlush() {
  int i;
  
  if (LockHandleAcquire(cache_lock) != SYNC_SUCCESS) {
    return DFS_FAIL;
  }
  
  for (i = 0; i < DFS_CACHE_NUM_SLOTS; i++) {
    if (adaptive_cache[i].valid && adaptive_cache[i].dirty) {
      if (DfsWriteBlockUncached(adaptive_cache[i].blocknum, 
                                 &adaptive_cache[i].data) == DFS_FAIL) {
        LockHandleRelease(cache_lock);
        return DFS_FAIL;
      }
      adaptive_cache[i].dirty = 0;
    }
    adaptive_cache[i].valid = 0;
  }
  
  LockHandleRelease(cache_lock);
  return DFS_SUCCESS;
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
  disk_block disk_blk;
  dfs_block dfs_blk;
  int i, j;
  int phys_blocks_per_fs = DFS_BLOCKSIZE / DiskBytesPerBlock();
  
  if (dfs_open) {
    return DFS_SUCCESS;
  }
  
  // Read superblock from DFS block 1 (spans multiple physical blocks)
  for (i = 0; i < phys_blocks_per_fs; i++) {
    if (DiskReadBlock(1 * phys_blocks_per_fs + i, &disk_blk) == DISK_FAIL) {
      printf("DfsOpenFileSystem: Failed to read superblock physical block %d\n", i);
      return DFS_FAIL;
    }
    bcopy(disk_blk.data, dfs_blk.data + (i * DiskBytesPerBlock()), DiskBytesPerBlock());
  }
  
  bcopy(dfs_blk.data, (char*)&sb, sizeof(dfs_superblock));
  
  if (sb.valid != 1) {
    printf("DfsOpenFileSystem: Filesystem not valid\n");
    return DFS_FAIL;
  }
  
  // Read inodes - read each DFS block manually
  for (i = 0; i < (sb.num_inodes * sizeof(dfs_inode) / sb.blocksize); i++) {
    // Read one DFS block worth of inodes
    for (j = 0; j < phys_blocks_per_fs; j++) {
      if (DiskReadBlock((sb.inode_start + i) * phys_blocks_per_fs + j, &disk_blk) == DISK_FAIL) {
        printf("DfsOpenFileSystem: Failed to read inode block %d physical block %d\n", i, j);
        return DFS_FAIL;
      }
      bcopy(disk_blk.data, dfs_blk.data + (j * DiskBytesPerBlock()), DiskBytesPerBlock());
    }
    bcopy(dfs_blk.data, 
          (char*)(inodes + i * (sb.blocksize / sizeof(dfs_inode))),
          sb.blocksize);
  }
  
  // Read FBV - read each DFS block manually
  for (i = 0; i < (sb.num_blocks / (sb.blocksize * 8)); i++) {
    for (j = 0; j < phys_blocks_per_fs; j++) {
      if (DiskReadBlock((sb.fbv_start + i) * phys_blocks_per_fs + j, &disk_blk) == DISK_FAIL) {
        printf("DfsOpenFileSystem: Failed to read FBV block %d physical block %d\n", i, j);
        return DFS_FAIL;
      }
      bcopy(disk_blk.data, dfs_blk.data + (j * DiskBytesPerBlock()), DiskBytesPerBlock());
    }
    bcopy(dfs_blk.data,
          (char*)(fbv + i * (sb.blocksize / sizeof(uint32))),
          sb.blocksize);
  }
  
  // Invalidate disk copy - write to DFS block 1
  sb.valid = 0;
  bzero(dfs_blk.data, sb.blocksize);
  bcopy((char*)&sb, dfs_blk.data, sizeof(dfs_superblock));
  
  for (i = 0; i < phys_blocks_per_fs; i++) {
    bcopy(dfs_blk.data + (i * DiskBytesPerBlock()), disk_blk.data, DiskBytesPerBlock());
    if (DiskWriteBlock(1 * phys_blocks_per_fs + i, &disk_blk) == DISK_FAIL) {
      printf("DfsOpenFileSystem: Failed to invalidate superblock\n");
      return DFS_FAIL;
    }
  }
  
  // Invalidate duplicate at block 65535
    // Invalidate duplicate superblock copy at a high-numbered DFS block, but guard disk size
  {
    int duplicate_fs_block = 65535; // as per spec; may exceed actual disk size
    int total_phys_blocks = duplicate_fs_block * phys_blocks_per_fs + phys_blocks_per_fs;
    int disk_phys_blocks = DiskSize() / DiskBytesPerBlock();
    if (total_phys_blocks <= disk_phys_blocks) {
      for (i = 0; i < phys_blocks_per_fs; i++) {
        if (DiskWriteBlock(duplicate_fs_block * phys_blocks_per_fs + i, &disk_blk) == DISK_FAIL) {
          printf("DfsOpenFileSystem: Failed to invalidate duplicate\n");
          return DFS_FAIL;
        }
      }
    }
  }
  
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
  disk_block disk_blk;
  int i, j;
  int phys_blocks_per_fs = DFS_BLOCKSIZE / DiskBytesPerBlock();
  
  if (!dfs_open) {
    return DFS_FAIL;
  }
  
  // Flush cache first
  if (DfsCacheFlush() == DFS_FAIL) {
    printf("DfsCloseFileSystem: Failed to flush cache\n");
    // return DFS_FAIL;
  }
  
  // Write inodes back
  for (i = 0; i < (sb.num_inodes * sizeof(dfs_inode) / sb.blocksize); i++) {
    bcopy((char*)(inodes + i * (sb.blocksize / sizeof(dfs_inode))),
          dfs_blk.data, sb.blocksize);
    for (j = 0; j < phys_blocks_per_fs; j++) {
      bcopy(dfs_blk.data + (j * DiskBytesPerBlock()), disk_blk.data, DiskBytesPerBlock());
      if (DiskWriteBlock((sb.inode_start + i) * phys_blocks_per_fs + j, &disk_blk) == DISK_FAIL) {
        printf("DfsCloseFileSystem: Failed to write inode block %d\n", i);
        return DFS_FAIL;
      }
    }
  }
  
  // Write FBV back
  for (i = 0; i < (sb.num_blocks / (sb.blocksize * 8)); i++) {
    bcopy((char*)(fbv + i * (sb.blocksize / sizeof(uint32))),
          dfs_blk.data, sb.blocksize);
    for (j = 0; j < phys_blocks_per_fs; j++) {
      bcopy(dfs_blk.data + (j * DiskBytesPerBlock()), disk_blk.data, DiskBytesPerBlock());
      if (DiskWriteBlock((sb.fbv_start + i) * phys_blocks_per_fs + j, &disk_blk) == DISK_FAIL) {
        printf("DfsCloseFileSystem: Failed to write FBV block %d\n", i);
        return DFS_FAIL;
      }
    }
  }
  
  // Write superblock to DFS block 1
  sb.valid = 1;
  bzero(dfs_blk.data, sb.blocksize);
  bcopy((char*)&sb, dfs_blk.data, sizeof(dfs_superblock));
  
  for (i = 0; i < phys_blocks_per_fs; i++) {
    bcopy(dfs_blk.data + (i * DiskBytesPerBlock()), disk_blk.data, DiskBytesPerBlock());
    if (DiskWriteBlock(1 * phys_blocks_per_fs + i, &disk_blk) == DISK_FAIL) {
      printf("DfsCloseFileSystem: Failed to write superblock\n");
      return DFS_FAIL;
    }
  }
  
    // Write duplicate superblock copy guarded by disk size
  {
    int duplicate_fs_block = 65535;
    int total_phys_blocks = duplicate_fs_block * phys_blocks_per_fs + phys_blocks_per_fs;
    int disk_phys_blocks = DiskSize() / DiskBytesPerBlock();
    if (total_phys_blocks <= disk_phys_blocks) {
      for (i = 0; i < phys_blocks_per_fs; i++) {
        if (DiskWriteBlock(duplicate_fs_block * phys_blocks_per_fs + i, &disk_blk) == DISK_FAIL) {
          printf("DfsCloseFileSystem: Failed to write duplicate superblock\n");
          return DFS_FAIL;
        }
      }
    }
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
  
  for (i = 0; i < DFS_FBV_MAX_NUM_WORDS; i++) {
    if (fbv[i] != 0xFFFFFFFF) {
      for (j = 0; j < 32; j++) {
        mask = 1 << j;
        if ((fbv[i] & mask) == 0) {
          uint32 candidate = (uint32)(i * 32 + j);
          // Never hand out DFS block 0 since 0 is used as the "unallocated" sentinel
          if (candidate == 0) {
            continue;
          }
          fbv[i] |= mask;
          LockHandleRelease(fbv_lock);
          return candidate;
        }
      }
    }
  }
  
  LockHandleRelease(fbv_lock);
  return DFS_FAIL;
}


//-----------------------------------------------------------------
// DfsFreeBlock deallocates a DFS block.
//-----------------------------------------------------------------

int DfsFreeBlock(uint32 blocknum) {
  int word_idx, bit_idx;
  uint32 mask;
  
  if (!dfs_open || blocknum >= sb.num_blocks) {
    return DFS_FAIL;
  }
  
  if (LockHandleAcquire(fbv_lock) != SYNC_SUCCESS) {
    return DFS_FAIL;
  }
  
  word_idx = blocknum / 32;
  bit_idx = blocknum % 32;
  // Guard against out-of-range index and use unsigned shift to avoid UB
  if (word_idx < DFS_FBV_MAX_NUM_WORDS) {
    mask = (uint32)1U << bit_idx;
    // If bit not set, nothing to do
    if ((fbv[word_idx] & mask) == 0) {
      LockHandleRelease(fbv_lock);
      return DFS_SUCCESS;
    }
    // Clear bit using invert() to avoid unary ~ promotion quirks in simulator
    fbv[word_idx] = fbv[word_idx] & invert(mask);
  } else {
    LockHandleRelease(fbv_lock);
    return DFS_FAIL;
  }
  LockHandleRelease(fbv_lock);
  return DFS_SUCCESS;
}


//-----------------------------------------------------------------
// DfsReadBlock reads an allocated DFS block from the disk
// (which could span multiple physical disk blocks).  The block
// must be allocated in order to read from it.  Returns DFS_FAIL
// on failure, and the number of bytes read on success.  
//-----------------------------------------------------------------
// uncached versions from previous questions
int DfsReadBlockUncached(uint32 blocknum, dfs_block *b) {
  int phys_blocks_per_fs = sb.blocksize / DiskBytesPerBlock();
  int i;
  disk_block disk_blk;
  
  if (!dfs_open || blocknum >= sb.num_blocks) {
    return DFS_FAIL;
  }
  printf("DfsReadBlockUncached: Reading block %d\n", blocknum);
  
  for (i = 0; i < phys_blocks_per_fs; i++) {
    // Add 5ms delay to simulate disk latency
    sleep_ms(5);
    printf("DfsReadBlockUncached: Reading physical block %d\n", blocknum * phys_blocks_per_fs + i);
    
    if (DiskReadBlock(blocknum * phys_blocks_per_fs + i, &disk_blk) == DISK_FAIL) {
      return DFS_FAIL;
    }
    bcopy(disk_blk.data, b->data + (i * DiskBytesPerBlock()), DiskBytesPerBlock());
  }
  
  disk_reads++;
  return sb.blocksize;
}

// Adaptive DfsReadBlock
int DfsReadBlock(uint32 blocknum, dfs_block *b) {
  int slot;
  uint32 start_time, end_time, latency;
  // double hit_rate, miss_rate;
  uint32 total_ops, hit_int, hit_dec, miss_int, miss_dec;
  
  if (!dfs_open || blocknum >= sb.num_blocks) {
    return DFS_FAIL;
  }
  
  if (LockHandleAcquire(cache_lock) != SYNC_SUCCESS) {
    return DFS_FAIL;
  }
  
  // Detect access pattern
  DetectAccessPattern(blocknum);
  
  // Check for cache hit
  slot = DfsAdaptiveCacheHit(blocknum);
  
  if (slot != DFS_FAIL) {
    // Cache HIT
    cache_hits++;
    adaptive_cache[slot].timestamp = cache_clock++;
    bcopy(adaptive_cache[slot].data.data, b->data, sb.blocksize);
    LockHandleRelease(cache_lock);
    return sb.blocksize;
  }
  
  // Cache MISS
  cache_misses++;
  start_time = GetCurrentTime();
  
  // Allocate cache slot using adaptive algorithm
  slot = DfsAdaptiveCacheAllocateSlot(blocknum);
  if (slot == DFS_FAIL) {
    LockHandleRelease(cache_lock);
    return DFS_FAIL;
  }
  
  // Read from disk
  if (DfsReadBlockUncached(blocknum, &adaptive_cache[slot].data) == DFS_FAIL) {
    adaptive_cache[slot].valid = 0;
    LockHandleRelease(cache_lock);
    return DFS_FAIL;
  }
  
  end_time = GetCurrentTime();
  latency = end_time - start_time;
  total_miss_latency += latency;
  
  // Copy to user buffer
  bcopy(adaptive_cache[slot].data.data, b->data, sb.blocksize);
  
  // Print statistics
  total_ops = (cache_hits + cache_misses);
  if (total_ops == 0) total_ops = 1; 

  // 2. Calculate Integer and Decimal parts using integer math
  // We want 3 decimal places, so we work with 100,000 multiplier
  hit_int = (cache_hits * 100) / total_ops;
  hit_dec = ((unsigned long long)cache_hits * 100000 / total_ops) % 1000;

  miss_int = (cache_misses * 100) / total_ops;
  miss_dec = ((unsigned long long)cache_misses * 100000 / total_ops) % 1000;

  // 3. Print using %d (integers) ONLY. Do not use %f.
  printf("Cache Miss: Hit Rate = %d.%03d%%, Miss Rate = %d.%03d%%, Disk Reads = %u, Disk Writes = %u, Miss Handling Latency = %ums\n",
    hit_int, hit_dec, 
    miss_int, miss_dec, 
    disk_reads, disk_writes,
    (total_miss_latency / (cache_misses ? cache_misses : 1)));
  // hit_rate = (cache_hits * 100.0) / (cache_hits + cache_misses);
  // miss_rate = (cache_misses * 100.0) / (cache_hits + cache_misses);
  
  // printf("Cache Miss: Hit Rate = %.3f%%, Miss Rate = %.3f%%, Disk Reads = %u, Disk Writes = %u, Miss Handling Latency = %ums\n",
  //     hit_rate, miss_rate, (unsigned)disk_reads, (unsigned)disk_writes,
  //     (unsigned)(total_miss_latency / (cache_misses ? cache_misses : 1)));

  
  LockHandleRelease(cache_lock);
  return sb.blocksize;
}




//-----------------------------------------------------------------
// DfsWriteBlock writes to an allocated DFS block on the disk
// (which could span multiple physical disk blocks).  The block
// must be allocated in order to write to it.  Returns DFS_FAIL
// on failure, and the number of bytes written on success.  
//-----------------------------------------------------------------

int DfsWriteBlockUncached(uint32 blocknum, dfs_block *b) {
  int phys_blocks_per_fs = sb.blocksize / DiskBytesPerBlock();
  int i;
  disk_block disk_blk;
  
  if (!dfs_open || blocknum >= sb.num_blocks) {
    return DFS_FAIL;
  }
  
  for (i = 0; i < phys_blocks_per_fs; i++) {
    // Add 5ms delay to simulate disk latency
    sleep_ms(5);
    
    bcopy(b->data + (i * DiskBytesPerBlock()), disk_blk.data, DiskBytesPerBlock());
    if (DiskWriteBlock(blocknum * phys_blocks_per_fs + i, &disk_blk) == DISK_FAIL) {
      return DFS_FAIL;
    }
  }
  
  disk_writes++;
  return sb.blocksize;
}

// Adaptive DfsWriteBlock
int DfsWriteBlock(uint32 blocknum, dfs_block *b) {
  int slot;
  uint32 start_time, end_time, latency;
  // double hit_rate, miss_rate;
  uint32 total_ops, hit_int, hit_dec, miss_int, miss_dec;
  
  if (!dfs_open || blocknum >= sb.num_blocks) {
    return DFS_FAIL;
  }
  
  if (LockHandleAcquire(cache_lock) != SYNC_SUCCESS) {
    return DFS_FAIL;
  }
  
  printf("DfsWriteBlock: Writing block %d\n", blocknum);
  // Detect access pattern
  DetectAccessPattern(blocknum);
  
  // Check for cache hit
  slot = DfsAdaptiveCacheHit(blocknum);
  printf("DfsWriteBlock: Cache slot = %d\n", slot);
  if (slot != DFS_FAIL) {
    // Cache HIT
    cache_hits++;
    adaptive_cache[slot].timestamp = cache_clock++;
    bcopy(b->data, adaptive_cache[slot].data.data, sb.blocksize);
    adaptive_cache[slot].dirty = 1;
    LockHandleRelease(cache_lock);
    return sb.blocksize;
  }
  printf("DfsWriteBlock: Cache MISS for block %d\n", blocknum);
  
  // Cache MISS
  cache_misses++;
  start_time = GetCurrentTime();
  
  // Allocate cache slot
  slot = DfsAdaptiveCacheAllocateSlot(blocknum);
  printf("DfsWriteBlock: Allocated cache slot %d for block %d\n", slot, blocknum);
  if (slot == DFS_FAIL) {
    LockHandleRelease(cache_lock);
    return DFS_FAIL;
  }
  printf("DfsWriteBlock: Reading block %d into cache slot %d\n", blocknum, slot);
  
  // Read existing data first
  if (DfsReadBlockUncached(blocknum, &adaptive_cache[slot].data) == DFS_FAIL) {
    adaptive_cache[slot].valid = 0;
    LockHandleRelease(cache_lock);
    return DFS_FAIL;
  }
  printf("DfsWriteBlock: Updating cache slot %d with new data for block %d\n", slot, blocknum);
  
  end_time = GetCurrentTime();
  latency = end_time - start_time;
  total_miss_latency += latency;
  
  printf("DfsWriteBlock: Updating cache slot %d with new data for block %d\n", slot, blocknum);
  // Update cache
  bcopy(b->data, adaptive_cache[slot].data.data, sb.blocksize);
  adaptive_cache[slot].dirty = 1;
  
  // Print statistics
  total_ops = (cache_hits + cache_misses);
  if (total_ops == 0) total_ops = 1;

  // Calculate Integer and Decimal parts using integer math
  hit_int = (cache_hits * 100) / total_ops;
  hit_dec = ((unsigned long long)cache_hits * 100000 / total_ops) % 1000;
  miss_int = (cache_misses * 100) / total_ops;
  miss_dec = ((unsigned long long)cache_misses * 100000 / total_ops) % 1000;

  // Print using %d (integers) ONLY. Do not use %f.
  printf("Cache Miss: Hit Rate = %d.%03d%%, Miss Rate = %d.%03d%%, Disk Reads = %u, Disk Writes = %u, Miss Handling Latency = %ums\n",
    hit_int, hit_dec, 
    miss_int, miss_dec, 
    disk_reads, disk_writes,
    (total_miss_latency / (cache_misses ? cache_misses : 1)));
  // hit_rate = (cache_hits * 100.0) / (cache_hits + cache_misses);
  // miss_rate = (cache_misses * 100.0) / (cache_hits + cache_misses);
  
  // printf("Cache Miss: Hit Rate = %.3f%%, Miss Rate = %.3f%%, Disk Reads = %u, Disk Writes = %u, Miss Handling Latency = %ums\n",
  //     hit_rate, miss_rate, (unsigned)disk_reads, (unsigned)disk_writes,
  //     (unsigned)(total_miss_latency / (cache_misses ? cache_misses : 1)));
  
  LockHandleRelease(cache_lock);
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
    // 44 = DFS_MAX_FILENAME_LENGTH
    if (inodes[i].inuse && dstrncmp(inodes[i].filename, filename, DFS_MAX_FILENAME_LENGTH) == 0) {
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
      // 44 = DFS_MAX_FILENAME_LENGTH
      dstrncpy(inodes[i].filename, filename, DFS_MAX_FILENAME_LENGTH);
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
      // Debug: help diagnose double-indirect translation failures around block 266
      // printf("DfsInodeReadBytes: translate failed (inode=%u vb=%d start=%d size=%d fs=%u di=%u)\n",
      //        handle, virtual_block, start_byte, num_bytes, (unsigned)fs_block,
      //        (unsigned)inodes[handle].double_indirect);
      if (inodes[handle].double_indirect != 0) {
        dfs_block tblblk;
        if (DfsReadBlock(inodes[handle].double_indirect, &tblblk) != DFS_FAIL) {
          uint32 *t = (uint32*)tblblk.data;
          int e = sb.blocksize / sizeof(uint32);
          int idx1 = (virtual_block - (10 + e));
          if (idx1 < 0) idx1 = 0;
          // printf("  DI[0]=%u DI[1]=%u DI[idx=%d]=%u\n",
          //        (unsigned)t[0], (unsigned)t[1], idx1/e, (unsigned)t[idx1/e]);
        }
      }
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
    printf("Writing byte %d of %d\n", bytes_written, num_bytes);
    virtual_block = (start_byte + bytes_written) / sb.blocksize;
    block_offset = (start_byte + bytes_written) % sb.blocksize;
    
    fs_block = DfsInodeTranslateVirtualToFilesys(handle, virtual_block);
    printf("Virtual block: %d, Block offset: %d, FS block: %d\n", virtual_block, block_offset, fs_block);
    
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
    printf("Writing %d bytes to FS block %d at offset %d\n", bytes_to_write, fs_block, block_offset);
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
  
  // recheck this
  uint32 max_blocks = 10 + entries_per_block + entries_per_block * entries_per_block;
  if (virtual_blocknum >= max_blocks) return DFS_FAIL;
  // end recheck 

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
  
    // Read first-level (double-indirect) table
  DfsReadBlock(inodes[handle].double_indirect, &blk);
  table = (uint32*)blk.data;

  // Determine first-level index and ensure child indirect block exists
  {
    uint32 idx1 = virtual_blocknum / (uint32)entries_per_block;
    if (table[idx1] == 0) {
      uint32 child = DfsAllocateBlock();
      if (child == DFS_FAIL) {
        DfsFreeBlock(new_block);
        return DFS_FAIL;
      }
      table[idx1] = child;
      // Persist updated double-indirect table
      DfsWriteBlock(inodes[handle].double_indirect, &blk);
      // Initialize new child indirect block with zeros
      bzero(blk.data, sb.blocksize);
      DfsWriteBlock(child, &blk);
      indirect_block = child;
    } else {
      indirect_block = table[idx1];
    }
  }

  // Now write mapping in the child indirect block
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
  // recheck this
  uint32 max_blocks = 10 + entries_per_block + entries_per_block * entries_per_block;
  if (virtual_blocknum >= max_blocks) return DFS_FAIL;
  // end recheck

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


//-----------------------------------------------------------------
// Cache functions
//-----------------------------------------------------------------


// int DfsAdaptiveCacheHit(int blocknum) {
//   int i;
  
//   for (i = 0; i < DFS_CACHE_NUM_SLOTS; i++) {
//     if (adaptive_cache[i].valid && adaptive_cache[i].blocknum == blocknum) {
//       return i;  // Return slot index
//     }
//   }
  
//   return DFS_FAIL;
// }


// int DfsAdaptiveCacheAllocateSlot(int blocknum) {
//   int i;
//   int lru_slot = 0;
//   uint32 lru_time = adaptive_cache[0].timestamp;
  
//   // First, look for empty slot
//   for (i = 0; i < DFS_CACHE_NUM_SLOTS; i++) {
//     if (!adaptive_cache[i].valid) {
//       adaptive_cache[i].valid = 1;
//       adaptive_cache[i].dirty = 0;
//       adaptive_cache[i].blocknum = blocknum;
//       adaptive_cache[i].timestamp = cache_clock++;
//       return i;
//     }
//   }
  
//   // No empty slot, find LRU (Least Recently Used)
//   for (i = 1; i < DFS_CACHE_NUM_SLOTS; i++) {
//     if (adaptive_cache[i].timestamp < lru_time) {
//       lru_time = adaptive_cache[i].timestamp;
//       lru_slot = i;
//     }
//   }
  
//   // Evict LRU slot - write back if dirty
//   if (adaptive_cache[lru_slot].dirty) {
//     if (DfsWriteBlockUncached(adaptive_cache[lru_slot].blocknum, &adaptive_cache[lru_slot].data) == DFS_FAIL) {
//       return DFS_FAIL;
//     }
//   }
  
//   // Allocate this slot
//   adaptive_cache[lru_slot].valid = 1;
//   adaptive_cache[lru_slot].dirty = 0;
//   adaptive_cache[lru_slot].blocknum = blocknum;
//   adaptive_cache[lru_slot].timestamp = cache_clock++;
  
//   return lru_slot;
// }

// int DfsAdaptiveCacheFlush() {
//   int i;
  
//   if (LockHandleAcquire(cache_lock) != SYNC_SUCCESS) {
//     return DFS_FAIL;
//   }
  
//   for (i = 0; i < DFS_CACHE_NUM_SLOTS; i++) {
//     if (adaptive_cache[i].valid && adaptive_cache[i].dirty) {
//       if (DfsWriteBlockUncached(adaptive_cache[i].blocknum, &adaptive_cache[i].data) == DFS_FAIL) {
//         LockHandleRelease(cache_lock);
//         return DFS_FAIL;
//       }
//       adaptive_cache[i].dirty = 0;
//     }
//     adaptive_cache[i].valid = 0;  // Clear all slots
//   }
  
//   LockHandleRelease(cache_lock);
//   return DFS_SUCCESS;
// }

int DfsCacheFlush() {
  int i;
  if (LockHandleAcquire(cache_lock) != SYNC_SUCCESS) return DFS_FAIL;
  for (i = 0; i < DFS_CACHE_NUM_SLOTS; i++) {
    if (adaptive_cache[i].valid && adaptive_cache[i].dirty) {
      if (DfsWriteBlockUncached(adaptive_cache[i].blocknum,
                                &adaptive_cache[i].data) == DFS_FAIL) {
        LockHandleRelease(cache_lock);
        return DFS_FAIL;
      }
      adaptive_cache[i].dirty = 0;
    }
    adaptive_cache[i].valid = 0;
  }
  LockHandleRelease(cache_lock);
  return DFS_SUCCESS;
}

