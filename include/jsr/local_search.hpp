#pragma once

#include "jsr/sequence.hpp"

namespace jsr {

struct LocalSearchOptions {
  int maxIterations = 10'000;
};

struct LocalSearchResult {
  Schedule schedule;
  Time makespan;
  int iterations;  // improving moves applied
};

// Steepest descent over the N5 neighbourhood. Starts from the machine sequence of `initial`,
// stops at a local optimum or after `maxIterations` improving moves. Never returns a schedule
// worse than `initial`.
// TODO: tabu search / simulated annealing to escape local optima; incremental (head/tail)
// move evaluation instead of a full decode per move.
[[nodiscard]] LocalSearchResult steepestDescent(const Instance& inst, const Schedule& initial,
                                                const Context& ctx = {},
                                                LocalSearchOptions opts = {});

}  // namespace jsr
