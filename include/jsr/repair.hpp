#pragma once

#include "jsr/schedule.hpp"

namespace jsr {

struct RepairResult {
  Schedule schedule;
  Time makespan;
  Time totalShift;  // stability: sum of |new start - old start| over rescheduled operations
  int rescheduledOps;
};

// Disruption at time `now`: operations started before `now` are frozen, the rest is
// rescheduled around the breakdown with the given dispatching rule.
[[nodiscard]] RepairResult repairRightShiftDispatch(const Instance& inst, const Schedule& plan,
                                                    Time now, const Downtime& breakdown,
                                                    Rule rule);

// TODO(local search): improve a repaired schedule, e.g. N5/N7 neighbourhood on the critical
// path with tabu search or simulated annealing, while penalising deviation from `plan`.

}  // namespace jsr
