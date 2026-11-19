#include <gtest/gtest.h>

#include <array>

#include "helpers.hpp"
#include "jsr/repair.hpp"

using namespace jsr;

namespace {

Context contextFor(const Instance& inst, const Schedule& plan, const Downtime& bd,
                   std::span<const Downtime> downtimes) {
  return {.downtimes = downtimes, .fixed = frozenAt(inst, plan, bd), .now = bd.from};
}

}  // namespace

TEST(FrozenAt, StartedOperationsAreFrozen) {
  const auto inst = test::ft06();
  const auto plan = buildSchedule(inst, Rule::MWKR);
  const Downtime bd{2, 10, 25};
  const auto fixed = frozenAt(inst, plan, bd);
  for (int i = 0; i < inst.totalOps(); ++i) {
    const bool started = plan.start[i] < bd.from;
    const bool interrupted =
        started && inst.ops[i].machine == bd.machine && plan.end(inst, i) > bd.from;
    EXPECT_EQ(fixed[i].has_value(), started && !interrupted) << "op " << i;
  }
}

TEST(FrozenAt, InterruptedOperationIsReleased) {
  // Single op on m0 from 0 to 10, breakdown at 5.
  const auto inst = test::fromString("1 1\n0 10\n");
  const Schedule plan{{0}};
  EXPECT_FALSE(frozenAt(inst, plan, {0, 5, 8})[0].has_value());
}

TEST(Repair, FeasibleUnderDisruption) {
  const auto inst = test::ft06();
  for (auto r : {Rule::SPT, Rule::LPT, Rule::MWKR, Rule::FIFO}) {
    const auto plan = buildSchedule(inst, r);
    const Downtime bd{2, 10, 25};
    const std::array d{bd};
    const auto res = repair(inst, plan, bd, {.rule = r});
    EXPECT_EQ(findViolation(inst, res.schedule, contextFor(inst, plan, bd, d)), std::nullopt)
        << ruleName(r);
    EXPECT_EQ(res.makespan, res.schedule.makespan(inst));
  }
}

TEST(Repair, InterruptedOperationRestartsAfterBreakdown) {
  const auto inst = test::fromString("1 1\n0 10\n");
  const Schedule plan{{0}};
  const auto res = repair(inst, plan, {0, 5, 8});
  EXPECT_EQ(res.schedule.start[0], 8);
  EXPECT_EQ(res.makespan, 18);
  EXPECT_EQ(res.totalShift, 8);
  EXPECT_EQ(res.rescheduledOps, 1);
}

TEST(Repair, BreakdownAfterPlanEndsChangesNothing) {
  const auto inst = test::ft06();
  const auto plan = buildSchedule(inst, Rule::MWKR);
  const Time end = plan.makespan(inst);
  const auto res = repair(inst, plan, {0, end + 5, end + 10});
  EXPECT_EQ(res.schedule.start, plan.start);
  EXPECT_EQ(res.totalShift, 0);
  EXPECT_EQ(res.rescheduledOps, 0);
}

TEST(Repair, LocalSearchNeverWorse) {
  const auto inst = test::ft06();
  for (auto r : {Rule::SPT, Rule::LPT, Rule::MWKR, Rule::FIFO}) {
    const auto plan = buildSchedule(inst, r);
    const Downtime bd{1, 12, 30};
    const std::array d{bd};
    const auto base = repair(inst, plan, bd, {.rule = r});
    const auto improved = repair(inst, plan, bd, {.rule = r, .localSearch = true});
    EXPECT_LE(improved.makespan, base.makespan) << ruleName(r);
    EXPECT_EQ(findViolation(inst, improved.schedule, contextFor(inst, plan, bd, d)), std::nullopt);
  }
}
