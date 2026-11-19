#include "jsr/repair.hpp"

#include <array>
#include <cstdlib>

#include "jsr/local_search.hpp"

namespace jsr {

std::vector<std::optional<Time>> frozenAt(const Instance& inst, const Schedule& plan,
                                          const Downtime& breakdown) {
  std::vector<std::optional<Time>> fixed(static_cast<std::size_t>(inst.totalOps()));
  for (int i = 0; i < inst.totalOps(); ++i) {
    const bool started = plan.start[i] < breakdown.from;
    const bool interrupted = inst.ops[i].machine == breakdown.machine &&
                             plan.start[i] < breakdown.to && plan.end(inst, i) > breakdown.from;
    if (started && !interrupted) fixed[i] = plan.start[i];
  }
  return fixed;
}

RepairResult repair(const Instance& inst, const Schedule& plan, const Downtime& breakdown,
                    RepairOptions opts) {
  const std::array downtimes{breakdown};
  const Context ctx{
      .downtimes = downtimes, .fixed = frozenAt(inst, plan, breakdown), .now = breakdown.from};

  auto sched = buildSchedule(inst, opts.rule, ctx);
  if (opts.localSearch) sched = steepestDescent(inst, sched, ctx).schedule;

  RepairResult res{std::move(sched), 0, 0, 0};
  res.makespan = res.schedule.makespan(inst);
  for (int i = 0; i < inst.totalOps(); ++i) {
    if (ctx.isFixed(i)) continue;
    res.totalShift += std::llabs(res.schedule.start[i] - plan.start[i]);
    ++res.rescheduledOps;
  }
  return res;
}

}  // namespace jsr
