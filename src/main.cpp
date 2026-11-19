// jsr: build a job-shop plan with a dispatching rule, optionally improve it with local search,
// and optionally repair it after a machine breakdown. Prints one CSV line per phase:
//   instance,rule,improve,phase,makespan,total_shift,rescheduled_ops,runtime_us

#include <array>
#include <chrono>
#include <cstdio>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#include "jsr/instance.hpp"
#include "jsr/local_search.hpp"
#include "jsr/repair.hpp"
#include "jsr/schedule.hpp"

namespace {

struct Args {
  std::string path;
  jsr::Rule rule = jsr::Rule::SPT;
  std::optional<jsr::Downtime> breakdown;
  bool improve = false;
  bool header = false;
};

void usage() {
  std::cerr << "usage: jsr <instance> [--rule SPT|LPT|MWKR|FIFO] [--breakdown machine,from,to]"
               " [--improve] [--header]\n";
}

std::optional<Args> parseArgs(int argc, char** argv) {
  Args args;
  for (int a = 1; a < argc; ++a) {
    const std::string arg = argv[a];
    if (arg == "--rule" && a + 1 < argc) {
      const auto r = jsr::parseRule(argv[++a]);
      if (!r) return std::nullopt;
      args.rule = *r;
    } else if (arg == "--breakdown" && a + 1 < argc) {
      int machine = 0;
      long long from = 0, to = 0;
      if (std::sscanf(argv[++a], "%d,%lld,%lld", &machine, &from, &to) != 3 || from >= to)
        return std::nullopt;
      args.breakdown = jsr::Downtime{machine, from, to};
    } else if (arg == "--improve") {
      args.improve = true;
    } else if (arg == "--header") {
      args.header = true;
    } else if (args.path.empty()) {
      args.path = arg;
    } else {
      return std::nullopt;
    }
  }
  if (args.path.empty()) return std::nullopt;
  return args;
}

template <typename F>
auto timed(F&& f) {
  const auto t0 = std::chrono::steady_clock::now();
  auto result = f();
  const auto t1 = std::chrono::steady_clock::now();
  return std::pair{std::move(result),
                   std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count()};
}

}  // namespace

int main(int argc, char** argv) {
  const auto args = parseArgs(argc, argv);
  if (!args) {
    usage();
    return 2;
  }

  try {
    const auto inst = jsr::readInstance(args->path);
    const auto row = [&](std::string_view phase, jsr::Time makespan, jsr::Time shift, int ops,
                         long long us) {
      std::cout << inst.name << ',' << jsr::ruleName(args->rule) << ',' << args->improve << ','
                << phase << ',' << makespan << ',' << shift << ',' << ops << ',' << us << '\n';
    };
    if (args->header)
      std::cout << "instance,rule,improve,phase,makespan,total_shift,rescheduled_ops,runtime_us\n";

    auto [plan, planUs] = timed([&] {
      auto s = jsr::buildSchedule(inst, args->rule);
      return args->improve ? jsr::steepestDescent(inst, s).schedule : s;
    });
    if (const auto err = jsr::findViolation(inst, plan)) {
      std::cerr << "infeasible plan: " << *err << '\n';
      return 1;
    }
    row("plan", plan.makespan(inst), 0, inst.totalOps(), planUs);

    if (const auto& bd = args->breakdown) {
      const auto [res, repairUs] = timed([&] {
        return jsr::repair(inst, plan, *bd, {.rule = args->rule, .localSearch = args->improve});
      });
      const std::array downtimes{*bd};
      const jsr::Context ctx{
          .downtimes = downtimes, .fixed = jsr::frozenAt(inst, plan, *bd), .now = bd->from};
      if (const auto err = jsr::findViolation(inst, res.schedule, ctx)) {
        std::cerr << "infeasible repair: " << *err << '\n';
        return 1;
      }
      row("repair", res.makespan, res.totalShift, res.rescheduledOps, repairUs);
    }
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
