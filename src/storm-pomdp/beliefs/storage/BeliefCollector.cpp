#include "storm-pomdp/beliefs/storage/BeliefCollector.h"
#include "storm-pomdp/beliefs/storage/Belief.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/OutOfRangeException.h"

namespace storm::pomdp::beliefs {

template<typename BeliefType>
bool BeliefCollector<BeliefType>::isEqual(BeliefId const& first, BeliefId const& second) const {
    return first == second || getBeliefFromId(first) == getBeliefFromId(second);
}

template<typename BeliefType>
BeliefId BeliefCollector<BeliefType>::getNumberOfBeliefIds() const {
    return gatheredBeliefs.size();
}

template<typename BeliefType>
BeliefType const& BeliefCollector<BeliefType>::getBeliefFromId(BeliefId const& id) const {
    STORM_LOG_ASSERT(id < gatheredBeliefs.size(), "Unexpected belief id " << id << ". Ids are in [0," << getNumberOfBeliefIds() << ").");
    return gatheredBeliefs[id];
}

template<typename BeliefType>
BeliefId BeliefCollector<BeliefType>::getIdFromBelief(BeliefType const& belief) const {
    STORM_LOG_ASSERT(belief.observation() < beliefToIdMap.size(),
                     "Unknown belief observation " << belief.observation() << ". Observations are in [0," << beliefToIdMap.size());
    auto const id = getIdOptional(belief);
    STORM_LOG_THROW(id != InvalidBeliefId, storm::exceptions::OutOfRangeException, "Belief " << belief.toString() << " is not present in this collector.");
    return id;
}

template<typename BeliefType>
bool BeliefCollector<BeliefType>::containsBelief(BeliefType const& belief) const {
    return getIdOptional(belief) != InvalidBeliefId;
}

template<typename BeliefType>
bool BeliefCollector<BeliefType>::containsId(BeliefId const& id) const {
    return id < gatheredBeliefs.size();
}

template<typename BeliefType>
BeliefId BeliefCollector<BeliefType>::getIdOptional(BeliefType const& belief) const {
    if (auto const obs = belief.observation(); obs < beliefToIdMap.size()) {
        auto const [begin, end] = beliefToIdMap[obs].equal_range(typename BeliefType::BeliefHash{}(belief));
        for (auto it = begin; it != end; ++it) {
            if (gatheredBeliefs[it->second] == belief) {
                return it->second;
            }
        }
    }
    return InvalidBeliefId;
}

template<typename BeliefType>
BeliefId BeliefCollector<BeliefType>::getIdOrAddBelief(BeliefType&& belief) {
    if (auto id = getIdOptional(belief); id != InvalidBeliefId) {
        return id;
    }
    return addBelief(std::move(belief));
}

template<typename BeliefType>
BeliefId BeliefCollector<BeliefType>::addBelief(BeliefType&& inputBelief) {
    auto const obs = inputBelief.observation();
    auto const hash = typename BeliefType::BeliefHash{}(inputBelief);
    if (obs >= beliefToIdMap.size()) {
        beliefToIdMap.resize(static_cast<uint64_t>(obs) + 1);
    }
    auto const id = gatheredBeliefs.size();
    gatheredBeliefs.push_back(std::move(inputBelief));
    beliefToIdMap[obs].emplace(hash, id);
    return id;
}

template class BeliefCollector<Belief<double>>;
template class BeliefCollector<Belief<storm::RationalNumber>>;

}  // namespace storm::pomdp::beliefs
