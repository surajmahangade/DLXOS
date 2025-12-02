# Lab 4: Build & Run Guide

## Prerequisite: Disk Image
A disk image is required before most targets will work. If it does not exist yet, run `make fdisk` (in the appropriate folder) to create it. Always do this first on a fresh clone or after deleting the image.

## Flat Filesystem Tests
From `lab4/flat`:

1. Create/refresh disk image:
    make fdisk
2. Run OS tests:
    make ostests
3. Run file tests:
    make filetest

Use `make clean` if rebuilds are needed.


## Pattern Workloads
Each pattern directory builds and runs its own workload using the existing disk image (create one first via `make fdisk` if missing in that directory):

In each of these folders: `pattern1/`, `pattern2/`, `pattern3/`, `dyn-pattern/`:

    make pattern

This builds (and usually runs) the pattern test binary using the shared or newly created image. If the image is missing in that folder, run:

    make fdisk

first, then:

    make pattern

## Summary of Order
Fresh setup:
1. cd lab4/flat && make fdisk
2. make ostests (optional) / make filetest (optional)
3. cd ../pattern1 && make pattern
4. Repeat `make pattern` in `pattern2`, `pattern3`, `dyn-pattern` (run `make fdisk` first inside a folder if its image is absent).

## Notes
- If the image does not exist, `make fdisk` must be run before other `make` targets will work.
- Re-run `make fdisk` only if you intentionally want to format/reinitialize; this will destroy previous filesystem contents.

## Information
- Nothing unusual is required beyond creating the disk image first (`make fdisk`).
- Builds are standard via provided Makefiles in each folder.

## Modified Files by Question
List of files changed for each question.

### Question 1
- `lab4/flat/apps/fdisk/fdisk/fdisk.c`
- `lab4/flat/apps/fdisk/include/fdisk.h`
- `lab4/flat/include/dfs_shared.h`
- `lab4/flat/include/os/disk.h`

### Question 2
- `lab4/flat/include/files_shared.h`
- `lab4/flat/include/os/dfs.h`
- `lab4/flat/os/dfs.c`

### Question 3
- `lab4/flat/os/dfs.c`

### Question 4
- `lab4/flat/apps/ostests/ostests/ostests.c`

### Question 5
- `lab4/flat/include/dfs_shared.h`
- `lab4/flat/include/files_shared.h`
- `lab4/flat/include/os/dfs.h`
- `lab4/flat/include/os/files.h`
- `lab4/flat/os/dfs.c`
- `lab4/flat/os/files.c`

### Question 6
- `lab4/flat/apps/filetest/filetest/filetest.c`

### Question 7
- `lab4/flat/include/os/dfs.h`
- `lab4/flat/os/dfs.c`

### Question 8
- `lab4/pattern1/os/dfs.c`
- `lab4/patterm2/os/dfs.c`
- `lab4/pattern3/os/dfs.c`

### Question 9
- `lab4/dyn-pattern`

