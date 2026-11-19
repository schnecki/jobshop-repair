#pragma once

#include <optional>
#include <vector>

#include "jsr/schedule.hpp"

namespace jsr {

struct RepairOptions {
  Rule rule = Rule::MWKR;
  bool localSearch = false;
};

struct RepairResult {
  Schedule schedule;
  Time makespan;
  Time totalShift;  // stability: sum of |new start - planned start| over rescheduled ops
  int rescheduledOps;
};

// Operations frozen when `breakdown` occurs at time breakdown.from: everything already started,
// except an operation running on the broken machine, which is interrupted and restarted later.
[[nodiscard]] std::vector<std::optional<Time>> frozenAt(const Instance& inst, const Schedule& plan,
                                                        const Downtime& breakdown);

// Reschedules all non-frozen operations around the breakdown, starting no earlier than
// breakdown.from. Optionally improves the result with local search.
[[nodiscard]] RepairResult repair(const Instance& inst, const Schedule& plan,
                                  const Downtime& breakdown, RepairOptions opts = {});

}  // namespace jsr
