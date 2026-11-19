# jobshop-repair

Real-time **rescheduling** for the job-shop problem: build a production plan, disrupt it with a
machine breakdown, and repair it quickly while keeping the schedule stable.

Core in modern C++20, experiments and analysis in Python. Developed test-first.

## Problem

- *n* jobs, *m* machines, each job a fixed route of operations (machine, duration).
- Non-preemptive operations; a machine processes one operation at a time.
- **Disruption:** machine *k* is down in `[from, to)`. At `now = from` every started operation is
  frozen, except one running on the broken machine, which is interrupted and restarted later.
  Everything else is rescheduled, starting no earlier than `now`.
- Objectives: makespan, and **stability** (total start-time deviation from the original plan).

## Architecture

| Module | Responsibility |
|---|---|
| `instance` | OR-Library / Taillard parser; flat, job-major operation storage |
| `schedule` | `Context` (downtimes, fixed starts, `now`), Giffler-Thompson generator with dispatching rules (SPT, LPT, MWKR, FIFO), feasibility checker |
| `sequence` | Machine sequences, semi-active decoding (Kahn topological order, cycle detection), critical path, N5 neighbourhood |
| `local_search` | Steepest descent over N5 |
| `repair` | Freezing at disruption time, repair by dispatching and optional local search |

Design choices:

- Operations live in one flat `std::vector`; a schedule is one start-time vector indexed the same
  way. No pointer-based graph, cache-friendly scans.
- Search happens on **machine sequences**, which are decoded into start times. The same decoder
  handles downtimes, frozen operations and `now`, so plan and repair share one code path.
- `Context` replaces optional raw pointers: an empty context means "empty shop at time 0".

## Results (ft06, optimum 55)

Mean makespan, plan and repair over 5 random breakdowns (`python3 bench/run_bench.py`):

| Rule | Plan | Plan + LS | Repair | Repair + LS |
|---|---|---|---|---|
| FIFO | 65 | **55** | 72.2 | 66.8 |
| MWKR | 67 | 58 | 72.6 | 66.4 |
| LPT | 86 | 71 | 86.2 | 79.0 |
| SPT | 94 | 94 | 98.8 | 90.6 |

SPT's plan is already a local optimum of N5. Pure descent cannot escape it; a test documents
this (`SteepestDescent.StuckInN5LocalOptimumFromSpt`) as the target for tabu search.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DJSR_WERROR=ON
cmake --build build
ctest --test-dir build --output-on-failure

# sanitizers (ASan + UBSan)
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DJSR_SANITIZE=ON
cmake --build build-asan && ctest --test-dir build-asan
```

GoogleTest is taken from the system if available, otherwise fetched by CMake.

## Run

```bash
./build/jsr instances/ft06.txt --rule FIFO --improve --breakdown 2,10,25 --header
python3 bench/run_bench.py          # results/results.csv, results/repair.png
```

Output, one CSV line per phase:

```
instance,rule,improve,phase,makespan,total_shift,rescheduled_ops,runtime_us
```

## Development process (TDD)

Every change follows red, green, refactor:

1. **Red:** write a test in `tests/` that states the new behaviour. Build and watch it fail for
   the expected reason (`ctest --test-dir build --output-on-failure`).
2. **Green:** write the smallest implementation that passes.
3. **Refactor:** clean up with all tests green; run the sanitizer build before committing.

Conventions:

- One test file per module (`tests/test_<module>.cpp`); shared fixtures in `tests/helpers.hpp`
  (`tiny()` for hand-checkable cases, `ft06()` for a real instance).
- Hand-computed expectations get a comment showing the derivation.
- Properties over examples where possible: feasibility (`findViolation`), "never worse than
  start", "frozen operations untouched".
- CI (GitHub Actions) builds with `-Werror`, runs all tests in Release and with sanitizers, and
  smoke-runs the CLI.

## Roadmap

Each item starts with a failing test.

- [ ] Tabu search over N5 (make `StuckInN5LocalOptimumFromSpt` improve)
- [ ] Stability-aware objective: makespan + λ · shift
- [ ] Incremental head/tail move evaluation instead of a full decode per move
- [ ] Taillard instances; runtime scaling benchmark; `perf` profiling write-up
- [ ] CP-SAT baseline (OR-Tools, Python)
