
# Lab 3 — Heap Management (lab3)

This README explains how to build and run the programs in this lab, what files were changed, and a few notes for the TA.

## How to build / run

Each subproject (fork, heap-mgmt, heap-mgmt-dyn, one-level) is self-contained. To build the code for a specific folder, change into that directory and run `make`.

Example (from the `lab3` root):

```bash
cd fork
make

cd ../heap-mgmt
make

cd ../heap-mgmt-dyn
make

cd ../one-level
make
```

## What We modified

Note: the following lists files that were edited while implementing the lab exercises.

- Edited across all relevant folders (fork, heap-mgmt, heap-mgmt-dyn, one-level):
	- `memory.c`
	- `memory.h`
	- `memory_constants.h`
	- `process.c`
	- `process.h`

- Additional edits for the `fork` folder:
	- `traps.c`
	- `traps.h`

- note for dynamic-heap (`heap-mgmt-dyn`):
	- After copying files from `heap-mgmt`, I edited `memory_constants.h` in `heap-mgmt-dyn` to make the dynamic-heap configuration work as intended.


- The dynamic heap implementation (heap-mgmt-dyn) supports running multiple user processes concurrently. In testing it is able to handle approximately 10–15 processes in parallel before resource limits (heap pages / allocations) become constrained. Your mileage may vary depending on simulator memory settings.

- Print Assumtion in memory.c when allocating a physical page for a virtual page in the heap
  We haveprinted the physical page and the virtual page is relative to the heap start, also note that the first page is not printing since the first page is allocated during process creation. Count starts from 0 for heap page number.