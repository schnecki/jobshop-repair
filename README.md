# jobshop-repair

Real-time **rescheduling** for the job-shop problem: build a production plan, disrupt it with a
machine breakdown, and repair it quickly while keeping the schedule stable.

Core in modern C++20, experiments and analysis in Python.

## Problem

- *n* jobs, *m* machines, each job a fixed sequence of operations (machine, duration).
- Non-preemptive operations; a machine processes one operation at a time.
- **Disruption:** machine *k* is down in `[from, to)`. At time `now = from`, all operations that
  have started are frozen (except one interrupted on the broken machine, which restarts later).
  The rest is rescheduled.
- Objectives: makespan, and **stability**: total start-time deviation from the original plan.

## Status

| Component | Status |
|---|---|
| Instance parser (OR-Library / Taillard format) | done |
| Giffler-Thompson active schedule generator with dispatching rules (SPT, LPT, MWKR, FIFO) | done |
| Feasibility validator (precedence, capacity, downtime) | done |
| Breakdown repair: freeze + re-dispatch around downtime | done |
| Benchmark runner + plots (Python) | done |
| Local search on the critical path (N5/N7, tabu / simulated annealing) | TODO |
| Stability-aware repair objective (makespan + λ · shift) | TODO |
| Incremental head/tail evaluation for fast move evaluation | TODO |
| Profiling write-up (`perf`) | TODO |
| CP-SAT baseline (OR-Tools, Python) | TODO |
| Taillard / larger instances | TODO |

## Build and run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build

./build/jsr instances/ft06.txt --rule MWKR --breakdown 2,10,25 --header
python3 bench/run_bench.py          # writes results/results.csv and results/repair.png
```

Output, one CSV line per phase:

```
instance,rule,phase,makespan,total_shift,rescheduled_ops,runtime_us
ft06.txt,MWKR,plan,67,0,36,3
ft06.txt,MWKR,repair,71,238,29,3
```

## Design notes

- Operations are stored flat (job-major) in one `std::vector`; schedules are a single start-time
  vector indexed the same way. No pointer-heavy graph structures, cache-friendly scans.
- Frozen operations form a prefix of each job, so the repair reuses the same generator.
- Downtimes are handled at start-time computation (earliest non-preemptive start avoiding
  windows).

## Results

*To be added (ft06 optimum is 55; current dispatching-only plans: 65–94).*
