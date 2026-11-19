#include "jsr/schedule.hpp"

#include <algorithm>
#include <limits>
#include <sstream>

namespace jsr {

Time Schedule::makespan(const Instance& inst) const {
  Time ms = 0;
  for (int i = 0; i < inst.totalOps(); ++i) ms = std::max(ms, start[i] + inst.ops[i].duration);
  return ms;
}

std::optional<Rule> parseRule(std::string_view name) {
  if (name == "SPT") return Rule::SPT;
  if (name == "LPT") return Rule::LPT;
  if (name == "MWKR") return Rule::MWKR;
  if (name == "FIFO") return Rule::FIFO;
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

// Earliest non-preemptive start >= t on machine m that avoids all downtimes.
Time earliestStart(int m, Time t, Time duration, std::span<const Downtime> downtimes) {
  bool moved = true;
  while (moved) {
    moved = false;
    for (const auto& d : downtimes) {
      if (d.machine == m && t < d.to && t + duration > d.from) {
        t = d.to;
        moved = true;
      }
    }
  }
  return t;
}

}  // namespace

Schedule buildSchedule(const Instance& inst, Rule rule, std::span<const Downtime> downtimes,
                       const Schedule* base, const std::vector<bool>* frozen) {
  const int n = inst.numJobs;
  Schedule sched;
  sched.start.assign(inst.totalOps(), 0);

  std::vector<int> next(n, 0);         // next unscheduled operation per job
  std::vector<Time> jobReady(n, 0);
  std::vector<Time> machReady(inst.numMachines, 0);
  std::vector<Time> remainingWork(n, 0);

  for (int j = 0; j < n; ++j) {
    for (int k = 0; k < inst.numOps(j); ++k) {
      const int i = inst.jobOffset[j] + k;
      const auto& op = inst.ops[i];
      if (frozen && base && (*frozen)[i]) {
        // Frozen operations form a prefix of each job.
        sched.start[i] = base->start[i];
        next[j] = k + 1;
        jobReady[j] = base->start[i] + op.duration;
        machReady[op.machine] = std::max(machReady[op.machine], jobReady[j]);
      } else {
        remainingWork[j] += op.duration;
      }
    }
  }

  std::vector<Time> est(n);
  std::vector<int> conflict;
  conflict.reserve(n);

  while (true) {
    // Earliest completion time over all schedulable operations (Giffler-Thompson).
    Time bestEct = std::numeric_limits<Time>::max();
    int bestMachine = -1;
    for (int j = 0; j < n; ++j) {
      if (next[j] >= inst.numOps(j)) continue;
      const auto& op = inst.op(j, next[j]);
      est[j] = earliestStart(op.machine, std::max(jobReady[j], machReady[op.machine]),
                             op.duration, downtimes);
      if (est[j] + op.duration < bestEct) {
        bestEct = est[j] + op.duration;
        bestMachine = op.machine;
      }
    }
    if (bestMachine < 0) break;  // all scheduled

    conflict.clear();
    for (int j = 0; j < n; ++j) {
      if (next[j] < inst.numOps(j) && inst.op(j, next[j]).machine == bestMachine &&
          est[j] < bestEct)
        conflict.push_back(j);
    }

    auto key = [&](int j) -> Time {
      const Time d = inst.op(j, next[j]).duration;
      switch (rule) {
        case Rule::SPT: return d;
        case Rule::LPT: return -d;
        case Rule::MWKR: return -remainingWork[j];
        case Rule::FIFO: return jobReady[j];
      }
      return 0;
    };
    const int j = *std::min_element(conflict.begin(), conflict.end(),
                                    [&](int a, int b) { return key(a) < key(b); });

    const int i = inst.jobOffset[j] + next[j];
    const auto& op = inst.ops[i];
    sched.start[i] = est[j];
    jobReady[j] = est[j] + op.duration;
    machReady[op.machine] = jobReady[j];
    remainingWork[j] -= op.duration;
    ++next[j];
  }
  return sched;
}

std::string validate(const Instance& inst, const Schedule& sched,
                     std::span<const Downtime> downtimes) {
  std::ostringstream err;
  struct Interval {
    Time from, to;
    int op;
  };
  std::vector<std::vector<Interval>> perMachine(inst.numMachines);

  for (int j = 0; j < inst.numJobs; ++j) {
    for (int k = 0; k < inst.numOps(j); ++k) {
      const int i = inst.jobOffset[j] + k;
      const Time s = sched.start[i];
      const Time e = s + inst.ops[i].duration;
      if (s < 0) {
        err << "op " << i << " starts before 0";
        return err.str();
      }
      if (k > 0) {
        const int p = i - 1;
        if (s < sched.start[p] + inst.ops[p].duration) {
          err << "precedence violated: job " << j << " op " << k;
          return err.str();
        }
      }
      for (const auto& d : downtimes) {
        if (d.machine == inst.ops[i].machine && s < d.to && e > d.from) {
          err << "op " << i << " overlaps downtime on machine " << d.machine;
          return err.str();
        }
      }
      perMachine[inst.ops[i].machine].push_back({s, e, i});
    }
  }

  for (int m = 0; m < inst.numMachines; ++m) {
    auto& iv = perMachine[m];
    std::sort(iv.begin(), iv.end(), [](const Interval& a, const Interval& b) { return a.from < b.from; });
    for (std::size_t x = 1; x < iv.size(); ++x) {
      if (iv[x].from < iv[x - 1].to) {
        err << "machine " << m << " overlap: ops " << iv[x - 1].op << " and " << iv[x].op;
        return err.str();
      }
    }
  }
  return {};
}

}  // namespace jsr
