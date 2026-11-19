#include <gtest/gtest.h>

#include <array>
#include <stdexcept>

#include "helpers.hpp"
#include "jsr/schedule.hpp"

using namespace jsr;

// ---------- rules ----------

TEST(Rule, NameRoundTrip) {
  for (auto r : {Rule::SPT, Rule::LPT, Rule::MWKR, Rule::FIFO})
    EXPECT_EQ(parseRule(ruleName(r)), r);
  EXPECT_EQ(parseRule("XYZ"), std::nullopt);
}

// ---------- earliestStart ----------

TEST(EarliestStart, NoDowntime) {
  EXPECT_EQ(earliestStart(0, 7, 3, {}), 7);
}

TEST(EarliestStart, FitsBeforeDowntime) {
  const std::array d{Downtime{0, 10, 20}};
  EXPECT_EQ(earliestStart(0, 5, 5, d), 5);
}

TEST(EarliestStart, ShiftedPastOverlappingDowntime) {
  const std::array d{Downtime{0, 10, 20}};
  EXPECT_EQ(earliestStart(0, 8, 5, d), 20);
}

TEST(EarliestStart, ShiftedPastChainedDowntimes) {
  const std::array d{Downtime{0, 20, 22}, Downtime{0, 10, 20}};
  EXPECT_EQ(earliestStart(0, 8, 5, d), 22);
}

TEST(EarliestStart, OtherMachineIgnored) {
  const std::array d{Downtime{1, 0, 100}};
  EXPECT_EQ(earliestStart(0, 0, 5, d), 0);
}

// ---------- buildSchedule ----------

TEST(BuildSchedule, SptOnTinyInstance) {
  const auto inst = test::tiny();
  const auto s = buildSchedule(inst, Rule::SPT);
  // m0: SPT puts J1 (2) before J0 (3).
  // m1: conflict set at C* = 6 holds J1 (est 2) and J0 (est 5); SPT picks J0 (2 < 4), so the
  // machine idles from 2 to 5. Giffler-Thompson builds active, not non-delay, schedules.
  EXPECT_EQ(s.start[inst.index(1, 0)], 0);
  EXPECT_EQ(s.start[inst.index(0, 0)], 2);
  EXPECT_EQ(s.start[inst.index(0, 1)], 5);
  EXPECT_EQ(s.start[inst.index(1, 1)], 7);
  EXPECT_EQ(s.makespan(inst), 11);
}

TEST(BuildSchedule, LptOnTinyInstance) {
  const auto inst = test::tiny();
  const auto s = buildSchedule(inst, Rule::LPT);
  EXPECT_EQ(s.start[inst.index(0, 0)], 0);
  EXPECT_EQ(s.makespan(inst), 9);
}

TEST(BuildSchedule, AllRulesFeasibleOnFt06) {
  const auto inst = test::ft06();
  for (auto r : {Rule::SPT, Rule::LPT, Rule::MWKR, Rule::FIFO}) {
    const auto s = buildSchedule(inst, r);
    EXPECT_EQ(findViolation(inst, s), std::nullopt) << ruleName(r);
    EXPECT_GE(s.makespan(inst), 55) << ruleName(r);  // known optimum
  }
}

TEST(BuildSchedule, RespectsDowntime) {
  const auto inst = test::ft06();
  const std::array d{Downtime{2, 0, 30}};
  const Context ctx{.downtimes = d};
  const auto s = buildSchedule(inst, Rule::MWKR, ctx);
  EXPECT_EQ(findViolation(inst, s, ctx), std::nullopt);
}

TEST(BuildSchedule, KeepsFixedStarts) {
  const auto inst = test::tiny();
  Context ctx;
  ctx.fixed.assign(inst.totalOps(), std::nullopt);
  ctx.fixed[inst.index(0, 0)] = 0;  // J0 started first, against SPT's preference
  const auto s = buildSchedule(inst, Rule::SPT, ctx);
  EXPECT_EQ(s.start[inst.index(0, 0)], 0);
  EXPECT_EQ(findViolation(inst, s, ctx), std::nullopt);
}

TEST(BuildSchedule, NothingStartsBeforeNow) {
  const auto inst = test::tiny();
  const Context ctx{.now = 10};
  const auto s = buildSchedule(inst, Rule::SPT, ctx);
  for (int i = 0; i < inst.totalOps(); ++i) EXPECT_GE(s.start[i], 10);
}

TEST(BuildSchedule, RejectsFixedVectorOfWrongSize) {
  const auto inst = test::tiny();
  Context ctx;
  ctx.fixed.assign(2, std::nullopt);  // 4 operations
  EXPECT_THROW((void)buildSchedule(inst, Rule::SPT, ctx), std::invalid_argument);
}

// ---------- findViolation ----------

TEST(FindViolation, DetectsPrecedence) {
  const auto inst = test::tiny();
  Schedule s{{0, 0, 3, 5}};  // J0 op1 starts while op0 still running
  EXPECT_NE(findViolation(inst, s), std::nullopt);
}

TEST(FindViolation, DetectsMachineOverlap) {
  const auto inst = test::tiny();
  Schedule s{{0, 3, 1, 5}};  // J1 op0 on m0 overlaps J0 op0
  EXPECT_NE(findViolation(inst, s), std::nullopt);
}

TEST(FindViolation, DetectsDowntimeOverlap) {
  const auto inst = test::tiny();
  const std::array d{Downtime{1, 3, 4}};
  Schedule s{{0, 3, 3, 5}};  // J0 op1 on m1 during [3,5)
  EXPECT_NE(findViolation(inst, s, {.downtimes = d}), std::nullopt);
}

TEST(FindViolation, DetectsStartBeforeNow) {
  const auto inst = test::tiny();
  Schedule s{{2, 5, 0, 7}};
  EXPECT_NE(findViolation(inst, s, {.now = 1}), std::nullopt);
}

TEST(FindViolation, DetectsMovedFixedOperation) {
  const auto inst = test::tiny();
  Context ctx;
  ctx.fixed.assign(inst.totalOps(), std::nullopt);
  ctx.fixed[0] = 1;
  Schedule s{{2, 5, 0, 7}};
  EXPECT_NE(findViolation(inst, s, ctx), std::nullopt);
}

TEST(FindViolation, AcceptsFeasibleSchedule) {
  const auto inst = test::tiny();
  Schedule s{{2, 6, 0, 2}};
  EXPECT_EQ(findViolation(inst, s), std::nullopt);
}
