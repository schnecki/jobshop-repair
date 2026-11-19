// Minimal self-contained tests (no framework dependency).

#include "jsr/instance.hpp"
#include "jsr/repair.hpp"
#include "jsr/schedule.hpp"

#include <array>
#include <iostream>

namespace {

int failures = 0;

void check(bool cond, const char* what) {
  if (!cond) {
    std::cerr << "FAIL: " << what << '\n';
    ++failures;
  }
}

}  // namespace

int main() {
  const auto inst = jsr::readInstance(JSR_INSTANCE_DIR "/ft06.txt");
  check(inst.numJobs == 6 && inst.numMachines == 6, "ft06 dimensions");

  for (auto rule : {jsr::Rule::SPT, jsr::Rule::LPT, jsr::Rule::MWKR, jsr::Rule::FIFO}) {
    const auto plan = jsr::buildSchedule(inst, rule);
    check(jsr::validate(inst, plan).empty(), "plan feasible");
    check(plan.makespan(inst) >= 55, "plan makespan >= optimum 55");

    const jsr::Downtime bd{2, 10, 25};
    const auto res = jsr::repairRightShiftDispatch(inst, plan, bd.from, bd, rule);
    const std::array downtimes{bd};
    check(jsr::validate(inst, res.schedule, downtimes).empty(), "repair feasible");
    for (int i = 0; i < inst.totalOps(); ++i) {
      if (plan.start[i] + inst.ops[i].duration <= bd.from)
        check(res.schedule.start[i] == plan.start[i], "completed ops unchanged");
    }
  }

  if (failures == 0) std::cout << "all tests passed\n";
  return failures == 0 ? 0 : 1;
}
