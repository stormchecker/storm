#include "storm-pomdp/beliefs/storage/BeliefCollector.h"
#include "storm-pomdp/beliefs/storage/Belief.h"
#include "storm/adapters/RationalNumberAdapter.h"

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
                     "Unknown belief observation " << belief.observation() << ". Obervations are in [0," << beliefToIdMap.size());
    STORM_LOG_ASSERT(containsBelief(belief), "Belief " << belief.toString() << " is not present in this collector.");
    return beliefToIdMap[belief.observation()].at(belief);
}

template<typename BeliefType>
bool BeliefCollector<BeliefType>::containsBelief(BeliefType const& belief) const {
    return belief.observation() < beliefToIdMap.size() && beliefToIdMap[belief.observation()].count(belief) > 0;
}

template<typename BeliefType>
bool BeliefCollector<BeliefType>::containsId(BeliefId const& id) const {
    return id < gatheredBeliefs.size();
}

template<typename BeliefType>
BeliefId BeliefCollector<BeliefType>::getIdOptional(BeliefType const& belief) const {
    if (auto const& obs = belief.observation(); obs < beliefToIdMap.size()) {
        if (auto findRes = beliefToIdMap[obs].find(belief); findRes != beliefToIdMap[obs].end()) {
            return findRes->second;
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
    auto const id = gatheredBeliefs.size();
    gatheredBeliefs.push_back(std::move(inputBelief));
    auto const& belief = gatheredBeliefs.back();
    auto const& obs = belief.observation();
    if (obs >= beliefToIdMap.size()) {
        beliefToIdMap.resize(obs + 1);
    }
    beliefToIdMap[obs].emplace(belief, id);
    return id;
}

template class BeliefCollector<Belief<double>>;
template class BeliefCollector<Belief<storm::RationalNumber>>;

}  // namespace storm::pomdp::beliefs