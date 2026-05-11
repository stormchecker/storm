
#include "storm-pomdp/generator/NondeterministicBeliefTracker.h"
#include "storm/storage/geometry/ReduceVertexCloud.h"
#include "storm/storage/geometry/nativepolytopeconversion/QuickHull.h"
#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/constants.h"
#include "storm/utility/vector.h"

namespace storm {
namespace generator {

template<typename ValueType>
BeliefStateManager<ValueType>::BeliefStateManager(storm::models::sparse::Pomdp<ValueType> const& pomdp) : pomdp(pomdp) {
    numberActionsPerObservation = std::vector<uint64_t>(pomdp.getNrObservations(), 0);
    statePerObservationAndOffset = std::vector<std::vector<uint64_t>>(pomdp.getNrObservations(), std::vector<uint64_t>());
    for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
        numberActionsPerObservation[pomdp.getObservation(state)] = pomdp.getNumberOfChoices(state);
        statePerObservationAndOffset[pomdp.getObservation(state)].push_back(state);
        observationOffsetId.push_back(statePerObservationAndOffset[pomdp.getObservation(state)].size() - 1);
    }
}

template<typename ValueType>
uint32_t BeliefStateManager<ValueType>::getObservation(uint64_t state) const {
    return pomdp.getObservation(state);
}

template<typename ValueType>
uint64_t BeliefStateManager<ValueType>::getNumberOfStates() const {
    return pomdp.getNumberOfStates();
}

template<typename ValueType>
uint64_t BeliefStateManager<ValueType>::getActionsForObservation(uint32_t observation) const {
    return numberActionsPerObservation[observation];
}

template<typename ValueType>
ValueType BeliefStateManager<ValueType>::getRisk(uint64_t state) const {
    return riskPerState.at(state);
}

template<typename ValueType>
storm::models::sparse::Pomdp<ValueType> const& BeliefStateManager<ValueType>::getPomdp() const {
    return pomdp;
}

template<typename ValueType>
void BeliefStateManager<ValueType>::setRiskPerState(std::vector<ValueType> const& risk) {
    riskPerState = risk;
}

template<typename ValueType>
uint64_t BeliefStateManager<ValueType>::getFreshId() {
    beliefIdCounter++;
    return beliefIdCounter;
}

template<typename ValueType>
uint64_t BeliefStateManager<ValueType>::getObservationOffset(uint64_t state) const {
    STORM_LOG_ASSERT(state < observationOffsetId.size(), "State " << state << " not a state id");
    return observationOffsetId[state];
}

template<typename ValueType>
uint64_t BeliefStateManager<ValueType>::numberOfStatesPerObservation(uint32_t observation) const {
    STORM_LOG_ASSERT(observation < observationOffsetId.size(), "Observation " << observation << " not an observation id");
    return statePerObservationAndOffset[observation].size();
}

template<typename ValueType>
uint64_t BeliefStateManager<ValueType>::getState(uint32_t obs, uint64_t offset) const {
    STORM_LOG_ASSERT(obs < statePerObservationAndOffset.size(), "Obs " << obs << " not a known observatoin");
    STORM_LOG_ASSERT(offset < statePerObservationAndOffset[obs].size(), "Offset " << offset << " too high for observation " << obs);
    return statePerObservationAndOffset[obs][offset];
}

template<typename ValueType>
SparseBeliefState<ValueType>::SparseBeliefState(std::shared_ptr<BeliefStateManager<ValueType>> const& manager, uint64_t state)
    : manager(manager), belief(), id(0), prevId(0) {
    id = manager->getFreshId();
    belief[state] = storm::utility::one<ValueType>();
    risk = manager->getRisk(state);
}

template<typename ValueType>
SparseBeliefState<ValueType>::SparseBeliefState(std::shared_ptr<BeliefStateManager<ValueType>> const& manager, std::map<uint64_t, ValueType> const& belief,
                                                std::size_t hash, ValueType const& risk, uint64_t prevId)
    : manager(manager), belief(belief), prestoredhash(hash), risk(risk), id(0), prevId(prevId) {
    id = manager->getFreshId();
}

template<typename ValueType>
ValueType SparseBeliefState<ValueType>::get(uint64_t state) const {
    return belief.at(state);
}

template<typename ValueType>
ValueType SparseBeliefState<ValueType>::getRisk() const {
    return risk;
}

template<typename ValueType>
std::size_t SparseBeliefState<ValueType>::hash() const noexcept {
    return prestoredhash;
}

template<typename ValueType>
bool SparseBeliefState<ValueType>::isValid() const {
    return !belief.empty();
}

template<typename ValueType>
std::string SparseBeliefState<ValueType>::toString() const {
    std::stringstream sstr;
    sstr << "id: " << id << "; ";
    bool first = true;
    for (auto const& beliefentry : belief) {
        if (!first) {
            sstr << ", ";
        } else {
            first = false;
        }
        sstr << beliefentry.first << " : " << beliefentry.second;
    }
    sstr << " (from " << prevId << ")";
    return sstr.str();
}

template<typename ValueType>
bool operator==(SparseBeliefState<ValueType> const& lhs, SparseBeliefState<ValueType> const& rhs) {
    if (lhs.hash() != rhs.hash()) {
        return false;
    }
    if (lhs.belief.size() != rhs.belief.size()) {
        return false;
    }
    storm::utility::ConstantsComparator<ValueType> cmp(storm::utility::convertNumber<ValueType>(0.00001), true);
    auto lhsIt = lhs.belief.begin();
    auto rhsIt = rhs.belief.begin();
    while (lhsIt != lhs.belief.end()) {
        if (lhsIt->first != rhsIt->first || !cmp.isEqual(lhsIt->second, rhsIt->second)) {
            return false;
        }
        ++lhsIt;
        ++rhsIt;
    }
    return true;
    // return std::equal(lhs.belief.begin(), lhs.belief.end(), rhs.belief.begin());
}

template<typename ValueType>
void SparseBeliefState<ValueType>::update(uint32_t newObservation, std::unordered_set<SparseBeliefState<ValueType>>& previousBeliefs) const {
    updateHelper({{}}, {storm::utility::zero<ValueType>()}, belief.begin(), newObservation, previousBeliefs);
}

template<typename ValueType>
uint64_t SparseBeliefState<ValueType>::getSupportSize() const {
    return manager->getNumberOfStates();
}

template<typename ValueType>
std::map<uint64_t, ValueType> const& SparseBeliefState<ValueType>::getBeliefMap() const {
    return belief;
}

template<typename ValueType>
void SparseBeliefState<ValueType>::setSupport(storm::storage::BitVector& support) const {
    for (auto const& entry : belief) {
        support.set(entry.first, true);
    }
}

template<typename ValueType>
void SparseBeliefState<ValueType>::updateHelper(std::vector<std::map<uint64_t, ValueType>> const& partialBeliefs, std::vector<ValueType> const& sums,
                                                typename std::map<uint64_t, ValueType>::const_iterator nextStateIt, uint32_t newObservation,
                                                std::unordered_set<SparseBeliefState<ValueType>>& previousBeliefs) const {
    if (nextStateIt == belief.end()) {
        for (uint64_t i = 0; i < partialBeliefs.size(); ++i) {
            auto const& partialBelief = partialBeliefs[i];
            auto const& sum = sums[i];
            if (storm::utility::isZero(sum)) {
                continue;
            }
            std::size_t newHash = 0;
            ValueType risk = storm::utility::zero<ValueType>();
            std::map<uint64_t, ValueType> finalBelief;
            for (auto& entry : partialBelief) {
                assert(!storm::utility::isZero(sum));
                finalBelief[entry.first] = entry.second / sum;
                // boost::hash_combine(newHash, std::hash<ValueType>()(entry.second));
                boost::hash_combine(newHash, entry.first);
                risk += entry.second / sum * manager->getRisk(entry.first);
            }
            previousBeliefs.insert(SparseBeliefState<ValueType>(manager, finalBelief, newHash, risk, id));
        }
    } else {
        uint64_t state = nextStateIt->first;
        auto newNextStateIt = nextStateIt;
        newNextStateIt++;
        std::vector<std::map<uint64_t, ValueType>> newPartialBeliefs;
        std::vector<ValueType> newSums;
        for (uint64_t i = 0; i < partialBeliefs.size(); ++i) {
            for (auto row = manager->getPomdp().getNondeterministicChoiceIndices()[state];
                 row < manager->getPomdp().getNondeterministicChoiceIndices()[state + 1]; ++row) {
                std::map<uint64_t, ValueType> newPartialBelief = partialBeliefs[i];
                ValueType newSum = sums[i];
                for (auto const& transition : manager->getPomdp().getTransitionMatrix().getRow(row)) {
                    if (newObservation != manager->getPomdp().getObservation(transition.getColumn())) {
                        continue;
                    }

                    if (newPartialBelief.count(transition.getColumn()) == 0) {
                        newPartialBelief[transition.getColumn()] = transition.getValue() * nextStateIt->second;
                    } else {
                        newPartialBelief[transition.getColumn()] += transition.getValue() * nextStateIt->second;
                    }
                    newSum += transition.getValue() * nextStateIt->second;
                }
                newPartialBeliefs.push_back(newPartialBelief);
                newSums.push_back(newSum);
            }
        }
        updateHelper(newPartialBeliefs, newSums, newNextStateIt, newObservation, previousBeliefs);
    }
}

template<typename ValueType, typename BeliefState>
NondeterministicBeliefTracker<ValueType, BeliefState>::NondeterministicBeliefTracker(
    storm::models::sparse::Pomdp<ValueType> const& pomdp, typename NondeterministicBeliefTracker<ValueType, BeliefState>::Options options)
    : pomdp(pomdp), manager(std::make_shared<BeliefStateManager<ValueType>>(pomdp)), beliefs(), options(options) {
    //
}

template<typename ValueType, typename BeliefState>
bool NondeterministicBeliefTracker<ValueType, BeliefState>::reset(uint32_t observation) {
    beliefs = std::unordered_set<BeliefState>();
    bool hit = false;
    for (auto state : pomdp.getInitialStates()) {
        if (observation == pomdp.getObservation(state)) {
            hit = true;
            beliefs.emplace(manager, state);
        }
    }
    lastObservation = observation;
    return hit;
}

template<typename ValueType, typename BeliefState>
bool NondeterministicBeliefTracker<ValueType, BeliefState>::track(uint64_t newObservation) {
    STORM_LOG_THROW(!beliefs.empty(), storm::exceptions::InvalidOperationException, "Cannot track without a belief (need to reset).");
    std::unordered_set<BeliefState> newBeliefs;
    storm::utility::Stopwatch trackTimer(true);
    for (auto const& belief : beliefs) {
        belief.update(newObservation, newBeliefs);
        if (options.trackTimeOut > 0 && static_cast<uint64_t>(trackTimer.getTimeInMilliseconds()) > options.trackTimeOut) {
            return false;
        }
    }
    beliefs = newBeliefs;
    lastObservation = newObservation;
    return !beliefs.empty();
}

template<typename ValueType, typename BeliefState>
ValueType NondeterministicBeliefTracker<ValueType, BeliefState>::getCurrentRisk(bool max) {
    STORM_LOG_THROW(!beliefs.empty(), storm::exceptions::InvalidOperationException, "Risk is only defined for beliefs (run reset() first).");
    ValueType result = beliefs.begin()->getRisk();
    if (max) {
        for (auto const& belief : beliefs) {
            if (belief.getRisk() > result) {
                result = belief.getRisk();
            }
        }
    } else {
        for (auto const& belief : beliefs) {
            if (belief.getRisk() < result) {
                result = belief.getRisk();
            }
        }
    }
    return result;
}

template<typename ValueType, typename BeliefState>
void NondeterministicBeliefTracker<ValueType, BeliefState>::setRisk(std::vector<ValueType> const& risk) {
    manager->setRiskPerState(risk);
}

template<typename ValueType, typename BeliefState>
std::unordered_set<BeliefState> const& NondeterministicBeliefTracker<ValueType, BeliefState>::getCurrentBeliefs() const {
    return beliefs;
}

template<typename ValueType, typename BeliefState>
uint32_t NondeterministicBeliefTracker<ValueType, BeliefState>::getCurrentObservation() const {
    return lastObservation;
}

template<typename ValueType, typename BeliefState>
uint64_t NondeterministicBeliefTracker<ValueType, BeliefState>::getNumberOfBeliefs() const {
    return beliefs.size();
}

template<typename ValueType, typename BeliefState>
uint64_t NondeterministicBeliefTracker<ValueType, BeliefState>::getCurrentDimension() const {
    storm::storage::BitVector support(beliefs.begin()->getSupportSize());
    for (auto const& belief : beliefs) {
        belief.setSupport(support);
    }
    return support.getNumberOfSetBits();
}

//
template<typename ValueType, typename BeliefState>
uint64_t NondeterministicBeliefTracker<ValueType, BeliefState>::reduce() {
    reductionTimedOut = false;
    std::shared_ptr<storm::utility::solver::SmtSolverFactory> solverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();
    storm::storage::geometry::ReduceVertexCloud<ValueType> rvc(solverFactory, options.wiggle, options.timeOut);
    std::vector<std::map<uint64_t, ValueType>> points;
    std::vector<typename std::unordered_set<BeliefState>::iterator> iterators;
    for (auto it = beliefs.begin(); it != beliefs.end(); ++it) {
        // TODO get rid of the getBeliefMap function.
        points.push_back(it->getBeliefMap());
        iterators.push_back(it);
    }
    auto res = rvc.eliminate(points, pomdp.getNumberOfStates());
    storm::storage::BitVector eliminate = ~res.first;
    if (res.second) {
        reductionTimedOut = true;
    }

    auto selectedIterators = storm::utility::vector::filterVector(iterators, eliminate);
    for (auto iter : selectedIterators) {
        beliefs.erase(iter);
    }
    return eliminate.getNumberOfSetBits();
}

template<typename ValueType, typename BeliefState>
bool NondeterministicBeliefTracker<ValueType, BeliefState>::hasTimedOut() const {
    return reductionTimedOut;
}

template class BeliefStateManager<double>;
template class SparseBeliefState<double>;
template bool operator==(SparseBeliefState<double> const&, SparseBeliefState<double> const&);
template class NondeterministicBeliefTracker<double, SparseBeliefState<double>>;

template class BeliefStateManager<storm::RationalNumber>;
template class SparseBeliefState<storm::RationalNumber>;
template bool operator==(SparseBeliefState<storm::RationalNumber> const&, SparseBeliefState<storm::RationalNumber> const&);
template class NondeterministicBeliefTracker<storm::RationalNumber, SparseBeliefState<storm::RationalNumber>>;

}  // namespace generator
}  // namespace storm
