#pragma once

#include <cstdint>
#include <istream>
#include <string>
#include <vector>

namespace jsr {

using Time = std::int64_t;

struct Operation {
  int machine;
  Time duration;
  int job;  // owning job
  int pos;  // position within the job's route
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
  [[nodiscard]] int index(int job, int k) const { return jobOffset[job] + k; }
  [[nodiscard]] const Operation& op(int job, int k) const { return ops[index(job, k)]; }
  [[nodiscard]] int totalOps() const { return static_cast<int>(ops.size()); }
  [[nodiscard]] bool isFirstOfJob(int i) const { return ops[i].pos == 0; }
  [[nodiscard]] bool isLastOfJob(int i) const { return ops[i].pos == numOps(ops[i].job) - 1; }
};

// Parses the standard OR-Library / Taillard text format:
//   <jobs> <machines>
//   <machine> <duration> <machine> <duration> ...   (one line per job, machines 0-indexed)
// Lines starting with '#' are ignored. Throws std::runtime_error on malformed input.
Instance parseInstance(std::istream& in, std::string name);
Instance readInstance(const std::string& path);

}  // namespace jsr
