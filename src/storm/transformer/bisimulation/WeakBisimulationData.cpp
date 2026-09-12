#include "storm/transformer/bisimulation/WeakBisimulationData.h"

#include <utility>

#include "storm/utility/macros.h"

namespace storm::bisimulation {

WeakBisimulationData::WeakBisimulationData(storm::storage::BitVector divergentStates, storm::storage::BitVector stepSensitiveStates,
                                           storm::storage::BitVector silentStates)
    : divergentStates(std::move(divergentStates)), stepSensitiveStates(std::move(stepSensitiveStates)), silentStates(std::move(silentStates)) {
    STORM_LOG_ASSERT(this->divergentStates.size() == this->stepSensitiveStates.size() && this->divergentStates.size() == this->silentStates.size(),
                     "Weak bisimulation data has inconsistent sizes.");
    STORM_LOG_ASSERT(this->divergentStates.isSubsetOf(this->silentStates), "A divergent state cannot leave its block and thus has to be silent.");
}

bool WeakBisimulationData::isDivergent(Partition::Block const& block) const {
    STORM_LOG_ASSERT(!block.empty(), "Tried to inspect an empty block.");
    return divergentStates.get(block.front());
}

bool WeakBisimulationData::isStepSensitive(Partition::Block const& block) const {
    STORM_LOG_ASSERT(!block.empty(), "Tried to inspect an empty block.");
    return stepSensitiveStates.get(block.front());
}

bool WeakBisimulationData::checkBlockHomogeneity(Partition const& partition) const {
    bool result = true;
    partition.forEachBlock([this, &result](Partition::Block const& block) {
        bool const divergent = isDivergent(block);
        bool const stepSensitive = isStepSensitive(block);
        for (uint64_t const state : block) {
            result = result && divergentStates.get(state) == divergent && stepSensitiveStates.get(state) == stepSensitive;
        }
    });
    return result;
}

}  // namespace storm::bisimulation
