#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jsr {

using Time = std::int64_t;

struct Operation {
  int machine;
  Time duration;
};

// Job-shop instance. Operations are stored flat (job-major) for cache-friendly access:
// operation k of job j lives at index jobOffset[j] + k.
struct Instance {
  std::string name;
  int numJobs = 0;
  int numMachines = 0;
  std::vector<Operation> ops;
  std::vector<int> jobOffset;  // size numJobs + 1

  [[nodiscard]] int numOps(int job) const { return jobOffset[job + 1] - jobOffset[job]; }
  [[nodiscard]] const Operation& op(int job, int k) const { return ops[jobOffset[job] + k]; }
  [[nodiscard]] int totalOps() const { return static_cast<int>(ops.size()); }
};

// Reads the standard OR-Library / Taillard text format:
//   <jobs> <machines>
//   <machine> <duration> <machine> <duration> ...   (one line per job, machines 0-indexed)
// Lines starting with '#' are ignored.
Instance readInstance(const std::string& path);

}  // namespace jsr
