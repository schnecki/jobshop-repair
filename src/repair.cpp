#include "jsr/repair.hpp"

#include <array>
#include <cstdlib>

namespace jsr {

RepairResult repairRightShiftDispatch(const Instance& inst, const Schedule& plan, Time now,
                                      const Downtime& breakdown, Rule rule) {
  std::vector<bool> frozen(inst.totalOps(), false);
  for (int i = 0; i < inst.totalOps(); ++i) {
    const auto& op = inst.ops[i];
    const bool started = plan.start[i] < now;
    // An operation running on the broken machine is interrupted and restarted later.
    const bool interrupted = op.machine == breakdown.machine &&
                             plan.start[i] < breakdown.to &&
                             plan.start[i] + op.duration > breakdown.from;
    frozen[i] = started && !interrupted;
  }

  const std::array downtimes{breakdown};
  RepairResult res{buildSchedule(inst, rule, downtimes, &plan, &frozen), 0, 0, 0};
  res.makespan = res.schedule.makespan(inst);
  for (int i = 0; i < inst.totalOps(); ++i) {
    if (frozen[i]) continue;
    res.totalShift += std::llabs(res.schedule.start[i] - plan.start[i]);
    ++res.rescheduledOps;
  }
  return res;
}

}  // namespace jsr
