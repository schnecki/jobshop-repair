#include "jsr/sequence.hpp"

#include <algorithm>
#include <utility>

namespace jsr {

namespace {

// Position of each operation within its machine sequence.
std::vector<int> machinePositions(const Instance& inst, const MachineSequence& seq) {
  std::vector<int> pos(static_cast<std::size_t>(inst.totalOps()), -1);
  for (const auto& ops : seq)
    for (std::size_t p = 0; p < ops.size(); ++p) pos[ops[p]] = static_cast<int>(p);
  return pos;
}

// Every operation appears exactly once, in the sequence of its own machine.
bool coversAllOperations(const Instance& inst, const MachineSequence& seq) {
  if (seq.size() != static_cast<std::size_t>(inst.numMachines)) return false;
  std::vector<bool> seen(static_cast<std::size_t>(inst.totalOps()), false);
  int count = 0;
  for (std::size_t m = 0; m < seq.size(); ++m) {
    for (const int i : seq[m]) {
      if (i < 0 || i >= inst.totalOps() || seen[i]) return false;
      if (inst.ops[i].machine != static_cast<int>(m)) return false;
      seen[i] = true;
      ++count;
    }
  }
  return count == inst.totalOps();
}

}  // namespace

MachineSequence sequenceOf(const Instance& inst, const Schedule& sched) {
  MachineSequence seq(static_cast<std::size_t>(inst.numMachines));
  for (int i = 0; i < inst.totalOps(); ++i) seq[inst.ops[i].machine].push_back(i);
  for (auto& ops : seq)
    std::ranges::sort(ops, {},
                      [&](int i) { return std::pair{sched.start[i], sched.end(inst, i)}; });
  return seq;
}

std::optional<Schedule> decode(const Instance& inst, const MachineSequence& seq,
                               const Context& ctx) {
  requireCompatible(inst, ctx);
  if (!coversAllOperations(inst, seq)) return std::nullopt;
  const int total = inst.totalOps();
  const auto mpos = machinePositions(inst, seq);

  // Kahn's algorithm over the disjunctive graph: job arcs plus the chosen machine arcs.
  std::vector<int> indegree(static_cast<std::size_t>(total), 0);
  for (int i = 0; i < total; ++i)
    indegree[i] = (inst.isFirstOfJob(i) ? 0 : 1) + (mpos[i] > 0 ? 1 : 0);

  std::vector<int> ready;
  ready.reserve(static_cast<std::size_t>(total));
  for (int i = 0; i < total; ++i)
    if (indegree[i] == 0) ready.push_back(i);

  Schedule sched;
  sched.start.assign(static_cast<std::size_t>(total), 0);
  int processed = 0;

  while (!ready.empty()) {
    const int i = ready.back();
    ready.pop_back();
    ++processed;

    const auto& op = inst.ops[i];
    if (ctx.isFixed(i)) {
      sched.start[i] = *ctx.fixed[i];
    } else {
      Time t = ctx.now;
      if (!inst.isFirstOfJob(i)) t = std::max(t, sched.end(inst, i - 1));
      if (mpos[i] > 0) t = std::max(t, sched.end(inst, seq[op.machine][mpos[i] - 1]));
      sched.start[i] = earliestStart(op.machine, t, op.duration, ctx.downtimes);
    }

    if (!inst.isLastOfJob(i) && --indegree[i + 1] == 0) ready.push_back(i + 1);
    const auto& machineOps = seq[op.machine];
    if (const auto next = static_cast<std::size_t>(mpos[i]) + 1; next < machineOps.size())
      if (--indegree[machineOps[next]] == 0) ready.push_back(machineOps[next]);
  }

  if (processed != total) return std::nullopt;  // cycle
  return sched;
}

std::vector<int> criticalPath(const Instance& inst, const Schedule& sched,
                              const MachineSequence& seq) {
  const int total = inst.totalOps();
  if (total == 0) return {};
  const auto mpos = machinePositions(inst, seq);

  int cur = 0;
  for (int i = 1; i < total; ++i)
    if (sched.end(inst, i) > sched.end(inst, cur)) cur = i;

  // Walk backwards along arcs without slack. Prefer machine arcs so that blocks stay intact.
  std::vector<int> path{cur};
  while (true) {
    const auto& op = inst.ops[cur];
    int pred = -1;
    if (mpos[cur] > 0) {
      const int mp = seq[op.machine][mpos[cur] - 1];
      if (sched.end(inst, mp) == sched.start[cur]) pred = mp;
    }
    if (pred < 0 && !inst.isFirstOfJob(cur) && sched.end(inst, cur - 1) == sched.start[cur])
      pred = cur - 1;
    if (pred < 0) break;
    path.push_back(pred);
    cur = pred;
  }
  std::ranges::reverse(path);
  return path;
}

std::vector<Swap> n5Moves(const Instance& inst, const MachineSequence& seq,
                          std::span<const int> path, const Context& ctx) {
  requireCompatible(inst, ctx);
  const auto mpos = machinePositions(inst, seq);

  // Critical blocks: maximal runs of path operations linked by machine arcs.
  struct Block {
    int machine, first, length;
  };
  std::vector<Block> blocks;
  for (std::size_t k = 0; k < path.size(); ++k) {
    const int i = path[k];
    const bool continues = k > 0 && inst.ops[path[k - 1]].machine == inst.ops[i].machine &&
                           mpos[path[k - 1]] + 1 == mpos[i];
    if (continues)
      ++blocks.back().length;
    else
      blocks.push_back({inst.ops[i].machine, mpos[i], 1});
  }

  std::vector<Swap> moves;
  const auto add = [&](int machine, int pos) {
    const Swap mv{machine, pos};
    if (ctx.isFixed(seq[machine][pos]) || ctx.isFixed(seq[machine][pos + 1])) return;
    if (std::ranges::find(moves, mv) == moves.end()) moves.push_back(mv);
  };
  for (std::size_t b = 0; b < blocks.size(); ++b) {
    const auto& blk = blocks[b];
    if (blk.length < 2) continue;
    if (b > 0) add(blk.machine, blk.first);
    if (b + 1 < blocks.size()) add(blk.machine, blk.first + blk.length - 2);
  }
  return moves;
}

void swapAdjacent(MachineSequence& seq, const Swap& move) {
  auto& ops = seq[move.machine];
  std::swap(ops[move.pos], ops[move.pos + 1]);
}

}  // namespace jsr
