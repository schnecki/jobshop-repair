#include "jsr/instance.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace jsr {

Instance readInstance(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open instance file: " + path);

  // Collect all non-comment tokens.
  std::stringstream data;
  for (std::string line; std::getline(in, line);) {
    if (!line.empty() && line.front() == '#') continue;
    data << line << '\n';
  }

  Instance inst;
  const auto slash = path.find_last_of('/');
  inst.name = slash == std::string::npos ? path : path.substr(slash + 1);
  if (!(data >> inst.numJobs >> inst.numMachines) || inst.numJobs <= 0 || inst.numMachines <= 0)
    throw std::runtime_error("invalid header in " + path);

  inst.jobOffset.reserve(inst.numJobs + 1);
  inst.ops.reserve(static_cast<std::size_t>(inst.numJobs) * inst.numMachines);
  for (int j = 0; j < inst.numJobs; ++j) {
    inst.jobOffset.push_back(static_cast<int>(inst.ops.size()));
    for (int k = 0; k < inst.numMachines; ++k) {
      Operation op{};
      if (!(data >> op.machine >> op.duration))
        throw std::runtime_error("unexpected end of data in " + path);
      if (op.machine < 0 || op.machine >= inst.numMachines || op.duration < 0)
        throw std::runtime_error("invalid operation in " + path);
      inst.ops.push_back(op);
    }
  }
  inst.jobOffset.push_back(static_cast<int>(inst.ops.size()));
  return inst;
}

}  // namespace jsr
