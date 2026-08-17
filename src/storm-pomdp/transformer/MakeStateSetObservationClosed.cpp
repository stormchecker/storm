#include "storm-pomdp/transformer/MakeStateSetObservationClosed.h"

#include "storm/adapters/RationalNumberAdapter.h"

namespace storm {
namespace transformer {

template<typename ValueType>
MakeStateSetObservationClosed<ValueType>::MakeStateSetObservationClosed(std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> pomdp) : pomdp(pomdp) {}

template<typename ValueType>
std::pair<std::shared_ptr<storm::models::sparse::Pomdp<ValueType>>, std::set<uint32_t>> MakeStateSetObservationClosed<ValueType>::transform(
    storm::storage::BitVector const& stateSet) const {
    // Collect observations of target states
    std::set<uint32_t> oldObservations;
    for (auto const state : stateSet) {
        oldObservations.insert(pomdp->getObservation(state));
    }

    // Collect observations that belong to both, target states and non-target states.
    // Add a fresh observation for each of them
    std::map<uint32_t, uint32_t> oldToNewObservationMap;
    uint32_t freshObs = pomdp->getNrObservations();
    for (uint64_t state = stateSet.getNextUnsetIndex(0); state < stateSet.size(); state = stateSet.getNextUnsetIndex(state + 1)) {
        uint32_t obs = pomdp->getObservation(state);
        if (oldObservations.count(obs) > 0) {
            // this observation belongs to both, target and non-target states.
            if (oldToNewObservationMap.emplace(obs, freshObs).second) {
                // We actually inserted something, i.e., we have not seen this observation before.
                // For the next observation (that is different from obs) we want to insert another fresh observation index
                // This is to preserve the assumption that states with the same observation have the same enabled actions.
                ++freshObs;
            }
        }
    }

    // Check whether the state set already is observation closed.
    if (oldToNewObservationMap.empty()) {
        return {pomdp, std::move(oldObservations)};
    } else {
        // Create new observations
        auto newObservationVector = pomdp->getObservations();
        for (auto const state : stateSet) {
            auto findRes = oldToNewObservationMap.find(pomdp->getObservation(state));
            if (findRes != oldToNewObservationMap.end()) {
                newObservationVector[state] = findRes->second;
            }
        }
        // Create a copy of the pomdp and change observations accordingly.
        // This transformation preserves canonicity.
        auto transformed = std::make_shared<storm::models::sparse::Pomdp<ValueType>>(*pomdp);
        transformed->updateObservations(std::move(newObservationVector), true);

        // Finally, get the new set of target observations
        std::set<uint32_t> newObservations;
        for (auto const& obs : oldObservations) {
            auto findRes = oldToNewObservationMap.find(obs);
            if (findRes == oldToNewObservationMap.end()) {
                newObservations.insert(obs);
            } else {
                newObservations.insert(findRes->second);
            }
        }

        return {transformed, std::move(newObservations)};
    }
}

template class MakeStateSetObservationClosed<double>;
template class MakeStateSetObservationClosed<storm::RationalNumber>;
}  // namespace transformer
}  // namespace storm