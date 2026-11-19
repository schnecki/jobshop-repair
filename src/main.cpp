// jsr: build a job-shop plan with a dispatching rule and optionally repair it after a machine
// breakdown. Prints one CSV line per phase:
//   instance,rule,phase,makespan,total_shift,rescheduled_ops,runtime_us

#include "jsr/instance.hpp"
#include "jsr/repair.hpp"
#include "jsr/schedule.hpp"

#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

namespace {

void usage() {
  std::cerr << "usage: jsr <instance> [--rule SPT|LPT|MWKR|FIFO] [--breakdown machine,from,to]"
               " [--header]\n";
}

template <typename F>
auto timed(F&& f) {
  const auto t0 = std::chrono::steady_clock::now();
  auto result = f();
  const auto t1 = std::chrono::steady_clock::now();
  return std::pair{result, std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()};
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    usage();
    return 2;
  }

  std::string path;
  jsr::Rule rule = jsr::Rule::SPT;
  std::optional<jsr::Downtime> breakdown;
  bool header = false;

  for (int a = 1; a < argc; ++a) {
    const std::string arg = argv[a];
    if (arg == "--rule" && a + 1 < argc) {
      const auto r = jsr::parseRule(argv[++a]);
      if (!r) {
        usage();
        return 2;
      }
      rule = *r;
    } else if (arg == "--breakdown" && a + 1 < argc) {
      jsr::Downtime d{};
      long long from = 0, to = 0;
      if (std::sscanf(argv[++a], "%d,%lld,%lld", &d.machine, &from, &to) != 3 || from >= to) {
        usage();
        return 2;
      }
      d.from = from;
      d.to = to;
      breakdown = d;
    } else if (arg == "--header") {
      header = true;
    } else if (path.empty()) {
      path = arg;
    } else {
      usage();
      return 2;
    }
  }

  try {
    const auto inst = jsr::readInstance(path);
    if (header) std::cout << "instance,rule,phase,makespan,total_shift,rescheduled_ops,runtime_us\n";

    const auto [plan, planUs] = timed([&] { return jsr::buildSchedule(inst, rule); });
    if (auto err = jsr::validate(inst, plan); !err.empty()) {
      std::cerr << "infeasible plan: " << err << '\n';
      return 1;
    }
    std::cout << inst.name << ',' << jsr::ruleName(rule) << ",plan," << plan.makespan(inst)
              << ",0," << inst.totalOps() << ',' << planUs << '\n';

    if (breakdown) {
      const auto [res, repairUs] = timed([&] {
        return jsr::repairRightShiftDispatch(inst, plan, breakdown->from, *breakdown, rule);
      });
      const std::array downtimes{*breakdown};
      if (auto err = jsr::validate(inst, res.schedule, downtimes); !err.empty()) {
        std::cerr << "infeasible repair: " << err << '\n';
        return 1;
      }
      std::cout << inst.name << ',' << jsr::ruleName(rule) << ",repair," << res.makespan << ','
                << res.totalShift << ',' << res.rescheduledOps << ',' << repairUs << '\n';
    }
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
