#include <gtest/gtest.h>

#include <stdexcept>

#include "helpers.hpp"

using namespace jsr;

TEST(Instance, ParsesHeaderAndOperations) {
  const auto inst = test::tiny();
  EXPECT_EQ(inst.numJobs, 2);
  EXPECT_EQ(inst.numMachines, 2);
  ASSERT_EQ(inst.totalOps(), 4);
  EXPECT_EQ(inst.op(1, 1).machine, 1);
  EXPECT_EQ(inst.op(1, 1).duration, 4);
}

TEST(Instance, StoresJobAndPositionPerOperation) {
  const auto inst = test::tiny();
  const int i = inst.index(1, 1);
  EXPECT_EQ(inst.ops[i].job, 1);
  EXPECT_EQ(inst.ops[i].pos, 1);
  EXPECT_TRUE(inst.isLastOfJob(i));
  EXPECT_TRUE(inst.isFirstOfJob(inst.index(1, 0)));
}

TEST(Instance, IgnoresCommentLines) {
  const auto inst = test::fromString("# comment\n1 1\n0 5\n");
  EXPECT_EQ(inst.op(0, 0).duration, 5);
}

TEST(Instance, RejectsTruncatedData) {
  EXPECT_THROW(test::fromString("2 2\n0 3 1 2\n0 2\n"), std::runtime_error);
}

TEST(Instance, RejectsMachineOutOfRange) {
  EXPECT_THROW(test::fromString("1 2\n0 3 2 2\n"), std::runtime_error);
}

TEST(Instance, RejectsMissingFile) {
  EXPECT_THROW(readInstance("/nonexistent/instance.txt"), std::runtime_error);
}

TEST(Instance, ReadsFt06) {
  const auto inst = test::ft06();
  EXPECT_EQ(inst.name, "ft06.txt");
  EXPECT_EQ(inst.totalOps(), 36);
}
