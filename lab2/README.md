## Lab2 — Build & Run (q2 → q5)

This directory contains Make targets to build and run questions q2 through q5. Two ways to run each question:

- Steps to run:
  - cd into `lab2/` and run `make qX` (where `qX` is `q2`, `q3`, `q4`, or `q5`).
  - You can override variables on the command line, e.g. `make q2 NumProcesses=6` for questions q2, q3, q4 and `make q5 num_N3=2 num_H2O=4` for q5.

- Top-level defaults (in `lab2/Makefile`):
  - `NumProcesses ?= 4`
  - `num_N3 ?= 1`
  - `num_H2O ?= 2`

What each question spawns / assumes

- q2 (producer/consumer):
  - The `makeprocs` program takes one argument: `<number of processes to create>`.
  - For each i in 0..(N-1) it creates one consumer and one producer. So total spawned worker processes = 2 * N

- q3 (bounded buffer producer/consumer):
  - Same interface as q2: pass `NumProcesses` to `makeprocs`.
  - It creates 2 * NumProcesses worker processes

- q4 (producer/consumer with locking/conds):
  - Same as q2/q3: pass `NumProcesses`; it also creates 2 * NumProcesses worker processes.

- q5 (chemical reactions):
  - `makeprocs` expects two arguments: `<num_N3> <num_H2O>`.
  - Defaults: `num_N3=1`, `num_H2O=2`

Examples

- Build & run q2/q3/q4:
  - make q2 NumProcesses=4
  - make q3 NumProcesses=4
  - make q4 NumProcesses=4

- Build & run q5 (chemical):
  - make q5 num_N3=3 num_H2O=10

