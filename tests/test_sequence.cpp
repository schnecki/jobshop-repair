#include <gtest/gtest.h>

#include <algorithm>
#include <array>

#include "helpers.hpp"
#include "jsr/sequence.hpp"

using namespace jsr;

namespace {

int machinePos(const MachineSequence& seq, int machine, int op) {
  const auto& s = seq[machine];
  return static_cast<int>(std::ranges::find(s, op) - s.begin());
}

}  // namespace

// ---------- sequenceOf ----------

TEST(SequenceOf, OrdersOperationsByStartTime) {
  const auto inst = test::tiny();
  const Schedule s{{2, 6, 0, 2}};  // m0: J1, J0   m1: J1, J0
  const auto seq = sequenceOf(inst, s);
  ASSERT_EQ(seq.size(), 2u);
  EXPECT_EQ(seq[0], (std::vector{inst.index(1, 0), inst.index(0, 0)}));
  EXPECT_EQ(seq[1], (std::vector{inst.index(1, 1), inst.index(0, 1)}));
}

// ---------- decode ----------

TEST(Decode, ReproducesActiveSchedule) {
  const auto inst = test::ft06();
  for (auto r : {Rule::SPT, Rule::LPT, Rule::MWKR, Rule::FIFO}) {
    const auto s = buildSchedule(inst, r);
    const auto d = decode(inst, sequenceOf(inst, s));
    ASSERT_TRUE(d.has_value());
    EXPECT_EQ(findViolation(inst, *d), std::nullopt);
    EXPECT_EQ(d->makespan(inst), s.makespan(inst)) << ruleName(r);
  }
}

TEST(Decode, StartsAsEarlyAsPossible) {
  const auto inst = test::tiny();
  // m0: J0 then J1, m1: J0 then J1  ->  J0: 0-3, 3-5; J1: 3-5, 5-9
  const MachineSequence seq{{inst.index(0, 0), inst.index(1, 0)},
                            {inst.index(0, 1), inst.index(1, 1)}};
  const auto d = decode(inst, seq);
  ASSERT_TRUE(d.has_value());
  EXPECT_EQ(d->start, (std::vector<Time>{0, 3, 3, 5}));
}

TEST(Decode, DetectsCycle) {
  // Opposite routes: J0 = m0 -> m1, J1 = m1 -> m0.
  const auto inst = test::fromString("2 2\n0 1 1 1\n1 1 0 1\n");
  // m0: J1.op1 before J0.op0 and m1: J0.op1 before J1.op0 closes the cycle
  // J0.op0 -> J0.op1 -> J1.op0 -> J1.op1 -> J0.op0.
  const MachineSequence cyclic{{inst.index(1, 1), inst.index(0, 0)},
                               {inst.index(0, 1), inst.index(1, 0)}};
  EXPECT_FALSE(decode(inst, cyclic).has_value());
}

TEST(Decode, RespectsContext) {
  const auto inst = test::ft06();
  const auto plan = buildSchedule(inst, Rule::MWKR);
  const std::array d{Downtime{2, 10, 25}};
  Context ctx{.downtimes = d, .now = 10};
  ctx.fixed.assign(inst.totalOps(), std::nullopt);
  for (int i = 0; i < inst.totalOps(); ++i)
    if (plan.end(inst, i) <= 10) ctx.fixed[i] = plan.start[i];

  const auto s = decode(inst, sequenceOf(inst, plan), ctx);
  ASSERT_TRUE(s.has_value());
  EXPECT_EQ(findViolation(inst, *s, ctx), std::nullopt);
}

// ---------- criticalPath ----------

TEST(CriticalPath, LengthEqualsMakespan) {
  const auto inst = test::ft06();
  const auto s = buildSchedule(inst, Rule::MWKR);
  const auto seq = sequenceOf(inst, s);
  const auto path = criticalPath(inst, s, seq);
  ASSERT_FALSE(path.empty());
  EXPECT_EQ(s.start[path.front()], 0);
  EXPECT_EQ(s.end(inst, path.back()), s.makespan(inst));
  for (std::size_t k = 1; k < path.size(); ++k)
    EXPECT_EQ(s.start[path[k]], s.end(inst, path[k - 1]));
}

TEST(CriticalPath, LinksAreJobOrMachineArcs) {
  const auto inst = test::ft06();
  const auto s = buildSchedule(inst, Rule::SPT);
  const auto seq = sequenceOf(inst, s);
  const auto path = criticalPath(inst, s, seq);
  for (std::size_t k = 1; k < path.size(); ++k) {
    const auto& a = inst.ops[path[k - 1]];
    const auto& b = inst.ops[path[k]];
    const bool jobArc = a.job == b.job && a.pos + 1 == b.pos;
    const bool machineArc = a.machine == b.machine && machinePos(seq, a.machine, path[k - 1]) + 1 ==
                                                          machinePos(seq, b.machine, path[k]);
    EXPECT_TRUE(jobArc || machineArc) << "between " << path[k - 1] << " and " << path[k];
  }
}

// ---------- n5Moves ----------

TEST(N5Moves, OnlySwapAdjacentCriticalOperations) {
  const auto inst = test::ft06();
  const auto s = buildSchedule(inst, Rule::SPT);
  const auto seq = sequenceOf(inst, s);
  const auto path = criticalPath(inst, s, seq);
  const auto moves = n5Moves(inst, seq, path);
  ASSERT_FALSE(moves.empty());
  for (const auto& mv : moves) {
    ASSERT_LT(mv.pos + 1, static_cast<int>(seq[mv.machine].size()));
    const int a = seq[mv.machine][mv.pos];
    const int b = seq[mv.machine][mv.pos + 1];
    EXPECT_NE(std::ranges::find(path, a), path.end());
    EXPECT_NE(std::ranges::find(path, b), path.end());
  }
}

TEST(N5Moves, NoMovesForSingleMachineBlockFreePath) {
  // One job only: critical path consists of job arcs, no machine blocks.
  const auto inst = test::fromString("1 3\n0 1 1 2 2 3\n");
  const auto s = buildSchedule(inst, Rule::SPT);
  const auto seq = sequenceOf(inst, s);
  EXPECT_TRUE(n5Moves(inst, seq, criticalPath(inst, s, seq)).empty());
}

TEST(N5Moves, ExcludesFixedOperations) {
  const auto inst = test::ft06();
  const auto s = buildSchedule(inst, Rule::SPT);
  const auto seq = sequenceOf(inst, s);
  const auto path = criticalPath(inst, s, seq);
  Context ctx;
  ctx.fixed.assign(inst.totalOps(), std::nullopt);
  for (int i = 0; i < inst.totalOps(); ++i) ctx.fixed[i] = s.start[i];  // everything fixed
  EXPECT_TRUE(n5Moves(inst, seq, path, ctx).empty());
}

TEST(SwapAdjacent, SwapsAdjacentOperations) {
  MachineSequence seq{{4, 7, 9}};
  swapAdjacent(seq, {0, 1});
  EXPECT_EQ(seq[0], (std::vector{4, 9, 7}));
}
