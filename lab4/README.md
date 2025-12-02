# Lab 4: Running the Filesystem and Patterns

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
- Re-run `make fdisk` only if you intentionally want to format/reinitialize; this will destroy previous filesystem contents.

