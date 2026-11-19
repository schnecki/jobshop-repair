#pragma once

#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "jsr/instance.hpp"

namespace jsr {

// Machine unavailable in [from, to), e.g. a breakdown. Operations are non-preemptive.
struct Downtime {
  int machine;
  Time from;
  Time to;
};

// Execution context for (re)scheduling. Default: empty shop at time 0.
struct Context {
  std::span<const Downtime> downtimes{};
  // Either empty, or one entry per operation: a value fixes the start time (already started).
  // Fixed operations must form a prefix of each job.
  std::vector<std::optional<Time>> fixed{};
  Time now = 0;  // non-fixed operations cannot start before `now`

  [[nodiscard]] bool isFixed(int op) const { return !fixed.empty() && fixed[op].has_value(); }
};

// Throws std::invalid_argument if `ctx.fixed` is neither empty nor sized to the instance.
// Every function taking a Context calls this.
void requireCompatible(const Instance& inst, const Context& ctx);

// Start time per operation, indexed like Instance::ops.
struct Schedule {
  std::vector<Time> start;

  [[nodiscard]] Time end(const Instance& inst, int op) const {
    return start[op] + inst.ops[op].duration;
  }
  [[nodiscard]] Time makespan(const Instance& inst) const;
};

// Earliest non-preemptive start >= t on machine m that avoids all downtimes.
[[nodiscard]] Time earliestStart(int machine, Time t, Time duration,
                                 std::span<const Downtime> downtimes);

enum class Rule { SPT, LPT, MWKR, FIFO };

[[nodiscard]] std::optional<Rule> parseRule(std::string_view name);
[[nodiscard]] std::string_view ruleName(Rule rule);

// Giffler-Thompson active schedule generation; the dispatching rule resolves conflicts.
[[nodiscard]] Schedule buildSchedule(const Instance& inst, Rule rule, const Context& ctx = {});

// First violated constraint (precedence, machine capacity, downtime, fixed starts, `now`),
// or std::nullopt if the schedule is feasible.
[[nodiscard]] std::optional<std::string> findViolation(const Instance& inst, const Schedule& sched,
                                                       const Context& ctx = {});

}  // namespace jsr
