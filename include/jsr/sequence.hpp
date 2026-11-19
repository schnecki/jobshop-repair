#pragma once

#include <optional>
#include <span>
#include <vector>

#include "jsr/schedule.hpp"

namespace jsr {

// Processing order of operation indices on each machine. This is the search space of the local
// search: a sequence is decoded into start times, rather than moving start times directly.
using MachineSequence = std::vector<std::vector<int>>;

[[nodiscard]] MachineSequence sequenceOf(const Instance& inst, const Schedule& sched);

// Semi-active decoding: every operation starts as early as job order, machine sequence,
// downtimes, fixed starts and `now` allow. Returns std::nullopt if the sequence is cyclic or not a
// permutation of all operations per machine. Throws std::invalid_argument for an incompatible
// Context (see requireCompatible).
[[nodiscard]] std::optional<Schedule> decode(const Instance& inst, const MachineSequence& seq,
                                             const Context& ctx = {});

// One critical path (operation indices in time order) ending in an operation that defines the
// makespan. Consecutive entries touch: start of next == end of previous.
// Precondition: `seq` covers all operations (as produced by sequenceOf or accepted by decode).
[[nodiscard]] std::vector<int> criticalPath(const Instance& inst, const Schedule& sched,
                                            const MachineSequence& seq);

// Swap of two adjacent operations seq[machine][pos] and seq[machine][pos + 1].
struct Swap {
  int machine;
  int pos;
  friend bool operator==(const Swap&, const Swap&) = default;
};

// N5 neighbourhood (Nowicki & Smutnicki): within each critical block swap the first two and the
// last two operations, except at the start of the first and the end of the last block.
// Moves touching fixed operations are excluded.
// Precondition as for criticalPath; throws std::invalid_argument for an incompatible Context.
[[nodiscard]] std::vector<Swap> n5Moves(const Instance& inst, const MachineSequence& seq,
                                        std::span<const int> path, const Context& ctx = {});

void swapAdjacent(MachineSequence& seq, const Swap& move);

}  // namespace jsr
