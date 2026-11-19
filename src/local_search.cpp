#include "jsr/local_search.hpp"

#include <optional>
#include <utility>

namespace jsr {

LocalSearchResult steepestDescent(const Instance& inst, const Schedule& initial, const Context& ctx,
                                  LocalSearchOptions opts) {
  LocalSearchResult best{initial, initial.makespan(inst), 0};
  auto seq = sequenceOf(inst, initial);

  while (best.iterations < opts.maxIterations) {
    const auto path = criticalPath(inst, best.schedule, seq);

    std::optional<std::pair<MachineSequence, Schedule>> bestNeighbour;
    Time bestNeighbourMs = best.makespan;
    for (const auto& mv : n5Moves(inst, seq, path, ctx)) {
      auto neighbour = seq;
      swapAdjacent(neighbour, mv);
      auto decoded = decode(inst, neighbour, ctx);
      if (!decoded) continue;
      if (const Time ms = decoded->makespan(inst); ms < bestNeighbourMs) {
        bestNeighbourMs = ms;
        bestNeighbour.emplace(std::move(neighbour), std::move(*decoded));
      }
    }
    if (!bestNeighbour) break;  // local optimum

    seq = std::move(bestNeighbour->first);
    best.schedule = std::move(bestNeighbour->second);
    best.makespan = bestNeighbourMs;
    ++best.iterations;
  }
  return best;
}

}  // namespace jsr
