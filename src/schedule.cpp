#include "jsr/schedule.hpp"

#include <algorithm>
#include <limits>
#include <sstream>

namespace jsr {

Time Schedule::makespan(const Instance& inst) const {
  Time ms = 0;
  for (int i = 0; i < inst.totalOps(); ++i) ms = std::max(ms, end(inst, i));
  return ms;
}

Time earliestStart(int machine, Time t, Time duration, std::span<const Downtime> downtimes) {
  for (bool moved = true; moved;) {
    moved = false;
    for (const auto& d : downtimes) {
      if (d.machine == machine && t < d.to && t + duration > d.from) {
        t = d.to;
        moved = true;
      }
    }
  }
  return t;
}

std::optional<Rule> parseRule(std::string_view name) {
  for (auto r : {Rule::SPT, Rule::LPT, Rule::MWKR, Rule::FIFO})
    if (ruleName(r) == name) return r;
  return std::nullopt;
}

std::string_view ruleName(Rule rule) {
  switch (rule) {
    case Rule::SPT: return "SPT";
    case Rule::LPT: return "LPT";
    case Rule::MWKR: return "MWKR";
    case Rule::FIFO: return "FIFO";
  }
  return "?";
}

namespace {

// Dispatching state of one job: its next unscheduled operation and when it is ready.
struct JobState {
  int next = 0;
  Time ready = 0;
  Time remainingWork = 0;
};

// Lower value = higher priority.
Time priority(Rule rule, const JobState& job, Time duration) {
  switch (rule) {
    case Rule::SPT: return duration;
    case Rule::LPT: return -duration;
    case Rule::MWKR: return -job.remainingWork;
    case Rule::FIFO: return job.ready;
  }
  return 0;
}

}  // namespace

Schedule buildSchedule(const Instance& inst, Rule rule, const Context& ctx) {
  const int n = inst.numJobs;
  Schedule sched;
  sched.start.assign(static_cast<std::size_t>(inst.totalOps()), 0);

  std::vector<JobState> jobs(static_cast<std::size_t>(n), JobState{0, ctx.now, 0});
  std::vector<Time> machReady(static_cast<std::size_t>(inst.numMachines), ctx.now);

  // Place fixed (already started) operations; they form a prefix of each job.
  for (int i = 0; i < inst.totalOps(); ++i) {
    const auto& op = inst.ops[i];
    auto& job = jobs[op.job];
    if (ctx.isFixed(i)) {
      sched.start[i] = *ctx.fixed[i];
      job.next = op.pos + 1;
      job.ready = std::max(job.ready, sched.end(inst, i));
      machReady[op.machine] = std::max(machReady[op.machine], sched.end(inst, i));
    } else {
      job.remainingWork += op.duration;
    }
  }

  std::vector<Time> est(static_cast<std::size_t>(n));
  std::vector<int> conflict;
  conflict.reserve(static_cast<std::size_t>(n));

  while (true) {
    // Operation with the earliest completion time defines machine and window (Giffler-Thompson).
    Time bestEct = std::numeric_limits<Time>::max();
    int bestMachine = -1;
    for (int j = 0; j < n; ++j) {
      if (jobs[j].next >= inst.numOps(j)) continue;
      const auto& op = inst.op(j, jobs[j].next);
      est[j] = earliestStart(op.machine, std::max(jobs[j].ready, machReady[op.machine]),
                             op.duration, ctx.downtimes);
      if (est[j] + op.duration < bestEct) {
        bestEct = est[j] + op.duration;
        bestMachine = op.machine;
      }
    }
    if (bestMachine < 0) break;

    conflict.clear();
    for (int j = 0; j < n; ++j)
      if (jobs[j].next < inst.numOps(j) && inst.op(j, jobs[j].next).machine == bestMachine &&
          est[j] < bestEct)
        conflict.push_back(j);

    const int j = *std::ranges::min_element(conflict, {}, [&](int c) {
      return priority(rule, jobs[c], inst.op(c, jobs[c].next).duration);
    });

    auto& job = jobs[j];
    const int i = inst.index(j, job.next);
    sched.start[i] = est[j];
    job.ready = sched.end(inst, i);
    job.remainingWork -= inst.ops[i].duration;
    machReady[bestMachine] = job.ready;
    ++job.next;
  }
  return sched;
}

std::optional<std::string> findViolation(const Instance& inst, const Schedule& sched,
                                         const Context& ctx) {
  std::ostringstream err;
  std::vector<std::vector<int>> perMachine(static_cast<std::size_t>(inst.numMachines));

  for (int i = 0; i < inst.totalOps(); ++i) {
    const auto& op = inst.ops[i];
    const Time s = sched.start[i];
    if (ctx.isFixed(i)) {
      if (s != *ctx.fixed[i]) {
        err << "fixed op " << i << " moved from " << *ctx.fixed[i] << " to " << s;
        return err.str();
      }
    } else if (s < ctx.now) {
      err << "op " << i << " starts at " << s << " before now=" << ctx.now;
      return err.str();
    }
    if (s < 0) {
      err << "op " << i << " starts before 0";
      return err.str();
    }
    if (!inst.isFirstOfJob(i) && s < sched.end(inst, i - 1)) {
      err << "precedence violated: job " << op.job << " op " << op.pos;
      return err.str();
    }
    for (const auto& d : ctx.downtimes) {
      if (d.machine == op.machine && s < d.to && sched.end(inst, i) > d.from) {
        err << "op " << i << " overlaps downtime on machine " << d.machine;
        return err.str();
      }
    }
    perMachine[op.machine].push_back(i);
  }

  for (int m = 0; m < inst.numMachines; ++m) {
    auto& ops = perMachine[m];
    std::ranges::sort(ops, {}, [&](int i) { return sched.start[i]; });
    for (std::size_t x = 1; x < ops.size(); ++x) {
      if (sched.start[ops[x]] < sched.end(inst, ops[x - 1])) {
        err << "machine " << m << " overlap: ops " << ops[x - 1] << " and " << ops[x];
        return err.str();
      }
    }
  }
  return std::nullopt;
}

}  // namespace jsr
