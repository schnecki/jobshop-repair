#pragma once

#include "jsr/instance.hpp"

#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace jsr {

// Machine unavailable in [from, to), e.g. a breakdown. Operations are non-preemptive.
struct Downtime {
  int machine;
  Time from;
  Time to;
};

// Start time per operation, indexed like Instance::ops.
struct Schedule {
  std::vector<Time> start;

  [[nodiscard]] Time makespan(const Instance& inst) const;
};

enum class Rule { SPT, LPT, MWKR, FIFO };

[[nodiscard]] std::optional<Rule> parseRule(std::string_view name);
[[nodiscard]] std::string_view ruleName(Rule rule);

// Giffler-Thompson active schedule generation with a dispatching rule for conflict resolution.
// Operations with frozen[i] == true keep start time `base.start[i]` (already executed or running).
// All other operations are (re)scheduled, respecting the downtimes.
[[nodiscard]] Schedule buildSchedule(const Instance& inst, Rule rule,
                                     std::span<const Downtime> downtimes = {},
                                     const Schedule* base = nullptr,
                                     const std::vector<bool>* frozen = nullptr);

// Checks precedence, machine capacity and downtime constraints. Returns an empty string if
// feasible, otherwise a description of the first violation found.
[[nodiscard]] std::string validate(const Instance& inst, const Schedule& sched,
                                   std::span<const Downtime> downtimes = {});

}  // namespace jsr
