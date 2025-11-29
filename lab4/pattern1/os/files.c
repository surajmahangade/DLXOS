#include "ostraps.h"
#include "dlxos.h"
#include "process.h"
#include "dfs.h"
#include "files.h"
#include "synch.h"

// You have already been told about the most likely places where you should use locks. You may use 
// additional locks if it is really necessary.
static lock_t file_lock;  // Lock for file descriptor operations
static file_descriptor file_descriptors[FILE_MAX_OPEN_FILES];

// STUDENT: put your file-level functions here

void FilesModuleInit() {
  int i;
  file_lock = LockCreate();
  for (i = 0; i < FILE_MAX_OPEN_FILES; i++) {
    file_descriptors[i].inuse = 0;
  }
}


int FileOpen(char *filename, char *mode) {
  int i;
  uint32 inode_handle;
  int pid = GetCurrentPid();
  
  // Acquire lock for file descriptor allocation
  if (LockHandleAcquire(file_lock) != SYNC_SUCCESS) {
    return FILE_FAIL;
  }
  
  // Check if file is already open
  for (i = 0; i < FILE_MAX_OPEN_FILES; i++) {
    if (file_descriptors[i].inuse && 
        dstrncmp(file_descriptors[i].filename, filename, FILE_MAX_FILENAME_LENGTH) == 0) {
      LockHandleRelease(file_lock);
      return FILE_FAIL;  // File already open
    }
  }
  
  // Handle write mode - delete existing file
  if (dstrncmp(mode, "w", 1) == 0) {
    inode_handle = DfsInodeFilenameExists(filename);
    if (inode_handle != DFS_FAIL) {
      if (DfsInodeDelete(inode_handle) == DFS_FAIL) {
        LockHandleRelease(file_lock);
        return FILE_FAIL;
      }
    }
    inode_handle = DfsInodeOpen(filename);
  } 
  // Handle append mode
  else if (dstrncmp(mode, "a", 1) == 0) {
    inode_handle = DfsInodeOpen(filename);
  }
  // Handle read mode
  else if (dstrncmp(mode, "r", 1) == 0) {
    inode_handle = DfsInodeFilenameExists(filename);
    if (inode_handle == DFS_FAIL) {
      LockHandleRelease(file_lock);
      return FILE_FAIL;  // File doesn't exist
    }
  } else {
    LockHandleRelease(file_lock);
    return FILE_FAIL;  // Invalid mode
  }
  
  if (inode_handle == DFS_FAIL) {
    LockHandleRelease(file_lock);
    return FILE_FAIL;
  }
  
  // Find free file descriptor
  for (i = 0; i < FILE_MAX_OPEN_FILES; i++) {
    if (!file_descriptors[i].inuse) {
      file_descriptors[i].inuse = 1;
      dstrncpy(file_descriptors[i].filename, filename, FILE_MAX_FILENAME_LENGTH);
      file_descriptors[i].inode = inode_handle;
      file_descriptors[i].eof = 0;
      dstrncpy(file_descriptors[i].mode, mode, 2);
      file_descriptors[i].pid = pid;
      
      // Set position based on mode
      if (dstrncmp(mode, "a", 1) == 0) {
        file_descriptors[i].current_pos = DfsInodeFilesize(inode_handle);
      } else {
        file_descriptors[i].current_pos = 0;
      }
      
      LockHandleRelease(file_lock);
      return i;
    }
  }
  
  LockHandleRelease(file_lock);
  return FILE_FAIL;  // No free descriptors
}



int FileClose(int handle) {
  int pid = GetCurrentPid();
  
  if (handle < 0 || handle >= FILE_MAX_OPEN_FILES) {
    return FILE_FAIL;
  }
  
  if (!file_descriptors[handle].inuse) {
    return FILE_FAIL;
  }
  
  if (file_descriptors[handle].pid != pid) {
    return FILE_FAIL;  // Not owner
  }
  
  if (LockHandleAcquire(file_lock) != SYNC_SUCCESS) {
    return FILE_FAIL;
  }
  
  file_descriptors[handle].inuse = 0;
  
  LockHandleRelease(file_lock);
  return FILE_SUCCESS;
}

int FileRead(int handle, void *mem, int num_bytes) {
  int bytes_read;
  int pid = GetCurrentPid();
  
  if (handle < 0 || handle >= FILE_MAX_OPEN_FILES) {
    return FILE_FAIL;
  }
  
  if (!file_descriptors[handle].inuse) {
    return FILE_FAIL;
  }
  
  if (file_descriptors[handle].pid != pid) {
    return FILE_FAIL;
  }
  
  if (file_descriptors[handle].eof) {
    return FILE_FAIL;
  }
  
  if (num_bytes > FILE_MAX_READWRITE_BYTES) {
    num_bytes = FILE_MAX_READWRITE_BYTES;
  }
  
  bytes_read = DfsInodeReadBytes(file_descriptors[handle].inode, mem, 
                                  file_descriptors[handle].current_pos, num_bytes);
  
  if (bytes_read == DFS_FAIL) {
    return FILE_FAIL;
  }
  
  file_descriptors[handle].current_pos += bytes_read;
  
  // Check for EOF
  if (file_descriptors[handle].current_pos >= 
      DfsInodeFilesize(file_descriptors[handle].inode)) {
    file_descriptors[handle].eof = 1;
  }
  
  return bytes_read;
}

int FileWrite(int handle, void *mem, int num_bytes) {
  int bytes_written;
  int pid = GetCurrentPid();
  
  if (handle < 0 || handle >= FILE_MAX_OPEN_FILES) {
    return FILE_FAIL;
  }
  
  if (!file_descriptors[handle].inuse) {
    return FILE_FAIL;
  }
  
  if (file_descriptors[handle].pid != pid) {
    return FILE_FAIL;
  }
  
  // Check if opened for reading only
  if (dstrncmp(file_descriptors[handle].mode, "r", 1) == 0) {
    return FILE_FAIL;
  }
  
  if (num_bytes > FILE_MAX_READWRITE_BYTES) {
    num_bytes = FILE_MAX_READWRITE_BYTES;
  }
  
  bytes_written = DfsInodeWriteBytes(file_descriptors[handle].inode, mem,
                                      file_descriptors[handle].current_pos, num_bytes);
  
  if (bytes_written == DFS_FAIL) {
    return FILE_FAIL;
  }
  
  file_descriptors[handle].current_pos += bytes_written;
  
  return bytes_written;
}

int FileSeek(int handle, int num_bytes, int from_where) {
  int new_pos;
  int pid = GetCurrentPid();
  uint32 filesize;
  
  if (handle < 0 || handle >= FILE_MAX_OPEN_FILES) {
    return FILE_FAIL;
  }
  
  if (!file_descriptors[handle].inuse) {
    return FILE_FAIL;
  }
  
  if (file_descriptors[handle].pid != pid) {
    return FILE_FAIL;
  }
  
  filesize = DfsInodeFilesize(file_descriptors[handle].inode);
  
  if (from_where == FILE_SEEK_SET) {
    new_pos = num_bytes;
  } else if (from_where == FILE_SEEK_CUR) {
    new_pos = file_descriptors[handle].current_pos + num_bytes;
  } else if (from_where == FILE_SEEK_END) {
    new_pos = filesize + num_bytes;
  } else {
    return FILE_FAIL;
  }
  
  if (new_pos < 0) {
    new_pos = 0;
  }
  
  file_descriptors[handle].current_pos = new_pos;
  file_descriptors[handle].eof = 0;  // Clear EOF flag
  
  return FILE_SUCCESS;
}

int FileDelete(char *filename) {
  uint32 inode_handle;
  int i;
  
  // Check if file is currently open
  for (i = 0; i < FILE_MAX_OPEN_FILES; i++) {
    if (file_descriptors[i].inuse && 
        dstrncmp(file_descriptors[i].filename, filename, FILE_MAX_FILENAME_LENGTH) == 0) {
      return FILE_FAIL;  // Cannot delete open file
    }
  }
  
  inode_handle = DfsInodeFilenameExists(filename);
  if (inode_handle == DFS_FAIL) {
    return FILE_FAIL;  // File doesn't exist
  }
  
  if (DfsInodeDelete(inode_handle) == DFS_FAIL) {
    return FILE_FAIL;
  }
  
  return FILE_SUCCESS;
}

int FileRename(char *oldname, char *newname) {
  uint32 old_inode, new_inode;
  int i;
  
  // Check if old file exists
  old_inode = DfsInodeFilenameExists(oldname);
  if (old_inode == DFS_FAIL) {
    return FILE_FAIL;
  }
  
  // Check if new name already exists
  new_inode = DfsInodeFilenameExists(newname);
  if (new_inode != DFS_FAIL) {
    return FILE_FAIL;  // New name already exists
  }
  
  // Check if file is open
  for (i = 0; i < FILE_MAX_OPEN_FILES; i++) {
    if (file_descriptors[i].inuse && 
        dstrncmp(file_descriptors[i].filename, oldname, FILE_MAX_FILENAME_LENGTH) == 0) {
      return FILE_FAIL;  // Cannot rename open file
    }
  }
  
  // Simply update the filename in the inode
  // Access the inode directly through DFS (this requires exposing inodes or adding a DFS function)
  // For now, we'll need to add a helper function in dfs.c
  
  return DfsInodeRename(old_inode, newname);
}

