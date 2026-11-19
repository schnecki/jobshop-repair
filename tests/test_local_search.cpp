#include <gtest/gtest.h>

#include <array>

#include "helpers.hpp"
#include "jsr/local_search.hpp"

using namespace jsr;

namespace {

constexpr std::array kRules{Rule::SPT, Rule::LPT, Rule::MWKR, Rule::FIFO};

}  // namespace

TEST(SteepestDescent, FeasibleAndNeverWorse) {
  const auto inst = test::ft06();
  for (auto r : kRules) {
    const auto start = buildSchedule(inst, r);
    const auto res = steepestDescent(inst, start);
    EXPECT_EQ(findViolation(inst, res.schedule), std::nullopt) << ruleName(r);
    EXPECT_LE(res.makespan, start.makespan(inst)) << ruleName(r);
    EXPECT_EQ(res.makespan, res.schedule.makespan(inst));
    EXPECT_GE(res.makespan, 55);
  }
}

TEST(SteepestDescent, ImprovesPoorStart) {
  const auto inst = test::ft06();
  const auto start = buildSchedule(inst, Rule::LPT);  // 86, N5 move to 81 exists
  const auto res = steepestDescent(inst, start);
  EXPECT_LT(res.makespan, start.makespan(inst));
  EXPECT_GT(res.iterations, 0);
}

// Documents the limit of pure descent: the SPT plan on ft06 (94) is already N5-optimal.
// A metaheuristic (tabu search) should turn this test's expectation into an improvement.
TEST(SteepestDescent, StuckInN5LocalOptimumFromSpt) {
  const auto inst = test::ft06();
  const auto start = buildSchedule(inst, Rule::SPT);
  const auto res = steepestDescent(inst, start);
  EXPECT_EQ(res.makespan, start.makespan(inst));
  EXPECT_EQ(res.iterations, 0);
}

TEST(SteepestDescent, EndsInLocalOptimum) {
  const auto inst = test::ft06();
  const auto res = steepestDescent(inst, buildSchedule(inst, Rule::LPT));
  const auto seq = sequenceOf(inst, res.schedule);
  for (const auto& mv : n5Moves(inst, seq, criticalPath(inst, res.schedule, seq))) {
    auto neighbour = seq;
    swapAdjacent(neighbour, mv);
    if (const auto s = decode(inst, neighbour)) {
      EXPECT_GE(s->makespan(inst), res.makespan);
    }
  }
}

TEST(SteepestDescent, ZeroIterationsReturnsInitial) {
  const auto inst = test::ft06();
  const auto start = buildSchedule(inst, Rule::SPT);
  const auto res = steepestDescent(inst, start, {}, {.maxIterations = 0});
  EXPECT_EQ(res.schedule.start, start.start);
  EXPECT_EQ(res.iterations, 0);
}

TEST(SteepestDescent, RespectsContext) {
  const auto inst = test::ft06();
  const auto plan = buildSchedule(inst, Rule::SPT);
  const std::array d{Downtime{0, 15, 30}};
  Context ctx{.downtimes = d, .now = 15};
  ctx.fixed.assign(inst.totalOps(), std::nullopt);
  for (int i = 0; i < inst.totalOps(); ++i)
    if (plan.start[i] < 15 && plan.end(inst, i) <= 15) ctx.fixed[i] = plan.start[i];

  const auto initial = buildSchedule(inst, Rule::SPT, ctx);
  const auto res = steepestDescent(inst, initial, ctx);
  EXPECT_EQ(findViolation(inst, res.schedule, ctx), std::nullopt);
  EXPECT_LE(res.makespan, initial.makespan(inst));
}
