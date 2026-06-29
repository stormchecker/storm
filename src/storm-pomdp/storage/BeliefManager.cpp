#include "storm-pomdp/storage/BeliefManager.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/solver/GlpkLpSolver.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

template<typename PomdpType, typename BeliefValueType, typename StateType>
uint64_t BeliefManager<PomdpType, BeliefValueType, StateType>::Triangulation::size() const {
    return weights.size();
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
BeliefManager<PomdpType, BeliefValueType, StateType>::FreudenthalDiff::FreudenthalDiff(StateType const &dimension, BeliefValueType diff)
    : dimension(dimension), diff(std::move(diff)) {
    // Intentionally left empty
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
bool BeliefManager<PomdpType, BeliefValueType, StateType>::FreudenthalDiff::operator>(FreudenthalDiff const &other) const {
    if (diff != other.diff) {
        return diff > other.diff;
    } else {
        return dimension < other.dimension;
    }
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
bool BeliefManager<PomdpType, BeliefValueType, StateType>::Belief_equal_to::operator()(const BeliefType &lhBelief, const BeliefType &rhBelief) const {
    return lhBelief == rhBelief;
}

template<>
bool BeliefManager<storm::models::sparse::Pomdp<double>, double, uint64_t>::Belief_equal_to::operator()(const BeliefType &lhBelief,
                                                                                                        const BeliefType &rhBelief) const {
    // If the sizes are different, we don't have to look inside the belief
    if (lhBelief.size() != rhBelief.size()) {
        return false;
    }
    // Assumes that beliefs are ordered
    auto lhIt = lhBelief.begin();
    auto rhIt = rhBelief.begin();
    while (lhIt != lhBelief.end() || rhIt != rhBelief.end()) {
        // Iterate over the entries simultaneously, beliefs not equal if they contain either different states or different values for the same state
        if (lhIt->first != rhIt->first || std::fabs(lhIt->second - rhIt->second) > 1e-15) {
            return false;
        }
        ++lhIt;
        ++rhIt;
    }
    return lhIt == lhBelief.end() && rhIt == rhBelief.end();
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::size_t BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefHash::operator()(const BeliefType &belief) const {
    std::size_t seed = 0;
    // Assumes that beliefs are ordered
    for (auto const &entry : belief) {
        boost::hash_combine(seed, entry.first);
        boost::hash_combine(seed, entry.second);
    }
    return seed;
}

template<>
std::size_t BeliefManager<storm::models::sparse::Pomdp<double>, double, uint64_t>::BeliefHash::operator()(const BeliefType &belief) const {
    std::size_t seed = 0;
    // Assumes that beliefs are ordered
    for (auto const &entry : belief) {
        boost::hash_combine(seed, entry.first);
        boost::hash_combine(seed, round(storm::utility::convertNumber<double>(entry.second) * 1e15));
    }
    return seed;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefManager(PomdpType const &pomdp, BeliefValueType const &precision,
                                                                    TriangulationMode const &triangulationMode)
    : pomdp(pomdp), cc(precision, false), triangulationMode(triangulationMode) {
    beliefToIdMap.resize(pomdp.getNrObservations());
    initialBeliefId = computeInitialBelief();
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
void BeliefManager<PomdpType, BeliefValueType, StateType>::setRewardModel(std::optional<std::string> rewardModelName) {
    if (rewardModelName) {
        auto const &rewardModel = pomdp.getRewardModel(rewardModelName.value());
        pomdpActionRewardVector = rewardModel.getTotalRewardVector(pomdp.getTransitionMatrix());
    } else {
        setRewardModel(pomdp.getUniqueRewardModelName());
    }
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
void BeliefManager<PomdpType, BeliefValueType, StateType>::unsetRewardModel() {
    pomdpActionRewardVector.clear();
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefId BeliefManager<PomdpType, BeliefValueType, StateType>::noId() const {
    return std::numeric_limits<BeliefId>::max();
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
bool BeliefManager<PomdpType, BeliefValueType, StateType>::isEqual(BeliefId const &first, BeliefId const &second) const {
    return isEqual(getBelief(first), getBelief(second));
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::string BeliefManager<PomdpType, BeliefValueType, StateType>::toString(BeliefId const &beliefId) const {
    return toString(getBelief(beliefId));
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::string BeliefManager<PomdpType, BeliefValueType, StateType>::toString(Triangulation const &t) const {
    std::stringstream str;
    str << "(\n";
    for (uint64_t i = 0; i < t.size(); ++i) {
        str << "\t" << t.weights[i] << " * \t" << toString(getBelief(t.gridPoints[i])) << "\n";
    }
    str << ")\n";
    return str.str();
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::ValueType BeliefManager<PomdpType, BeliefValueType, StateType>::getWeightedSum(
    BeliefId const &beliefId, std::vector<ValueType> const &summands) {
    auto result = storm::utility::zero<ValueType>();
    for (auto const &entry : getBelief(beliefId)) {
        result += storm::utility::convertNumber<ValueType>(entry.second) * storm::utility::convertNumber<ValueType>(summands.at(entry.first));
    }
    return result;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::pair<bool, typename BeliefManager<PomdpType, BeliefValueType, StateType>::ValueType> BeliefManager<PomdpType, BeliefValueType, StateType>::getWeightedSum(
    BeliefId const &beliefId, std::unordered_map<StateType, ValueType> const &summands) {
    bool successful = true;
    auto result = storm::utility::zero<ValueType>();
    for (auto const &entry : getBelief(beliefId)) {
        auto probIter = summands.find(entry.first);
        if (probIter != summands.end()) {
            result += storm::utility::convertNumber<ValueType>(entry.second) * storm::utility::convertNumber<ValueType>(summands.at(entry.first));
        } else {
            successful = false;
            break;
        }
    }
    return {successful, result};
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefId const &BeliefManager<PomdpType, BeliefValueType, StateType>::getInitialBelief() const {
    return initialBeliefId;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::ValueType BeliefManager<PomdpType, BeliefValueType, StateType>::getBeliefActionReward(
    BeliefId const &beliefId, uint64_t const &localActionIndex) const {
    auto const &belief = getBelief(beliefId);
    STORM_LOG_ASSERT(!pomdpActionRewardVector.empty(), "Requested a reward although no reward model was specified.");
    auto result = storm::utility::zero<ValueType>();
    auto const &choiceIndices = pomdp.getTransitionMatrix().getRowGroupIndices();
    for (auto const &entry : belief) {
        uint64_t choiceIndex = choiceIndices[entry.first] + localActionIndex;
        STORM_LOG_ASSERT(choiceIndex < choiceIndices[entry.first + 1], "Invalid local action index.");
        STORM_LOG_ASSERT(choiceIndex < pomdpActionRewardVector.size(), "Invalid choice index.");
        result += storm::utility::convertNumber<ValueType>(entry.second) * pomdpActionRewardVector[choiceIndex];
    }
    return result;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
uint32_t BeliefManager<PomdpType, BeliefValueType, StateType>::getBeliefObservation(BeliefId beliefId) {
    return getBeliefObservation(getBelief(beliefId));
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
uint64_t BeliefManager<PomdpType, BeliefValueType, StateType>::getBeliefNumberOfChoices(BeliefId beliefId) {
    auto const &belief = getBelief(beliefId);
    return pomdp.getNumberOfChoices(belief.begin()->first);
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::Triangulation BeliefManager<PomdpType, BeliefValueType, StateType>::triangulateBelief(
    BeliefId beliefId, BeliefValueType resolution) {
    return triangulateBelief(getBelief(beliefId), resolution);
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
template<typename DistributionType>
void BeliefManager<PomdpType, BeliefValueType, StateType>::addToDistribution(DistributionType &distr, StateType const &state, BeliefValueType const &value) {
    auto insertionRes = distr.emplace(state, value);
    if (!insertionRes.second) {
        insertionRes.first->second += value;
    }
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
template<typename DistributionType>
void BeliefManager<PomdpType, BeliefValueType, StateType>::adjustDistribution(DistributionType &distr) {
    if (distr.size() == 1 && cc.isEqual(distr.begin()->second, storm::utility::one<BeliefValueType>())) {
        // If the distribution consists of only one entry and its value is sufficiently close to 1, make it exactly 1 to avoid numerical problems
        distr.begin()->second = storm::utility::one<BeliefValueType>();
    }
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
void BeliefManager<PomdpType, BeliefValueType, StateType>::joinSupport(BeliefId const &beliefId, BeliefSupportType &support) {
    auto const &belief = getBelief(beliefId);
    for (auto const &entry : belief) {
        support.insert(entry.first);
    }
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefId BeliefManager<PomdpType, BeliefValueType, StateType>::getNumberOfBeliefIds() const {
    return beliefs.size();
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::vector<std::pair<typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefId,
                      typename BeliefManager<PomdpType, BeliefValueType, StateType>::ValueType>>
BeliefManager<PomdpType, BeliefValueType, StateType>::expandAndTriangulate(BeliefId const &beliefId, uint64_t actionIndex,
                                                                           std::vector<BeliefValueType> const &observationResolutions) {
    return expandInternal(beliefId, actionIndex, observationResolutions);
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::vector<std::pair<typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefId,
                      typename BeliefManager<PomdpType, BeliefValueType, StateType>::ValueType>>
BeliefManager<PomdpType, BeliefValueType, StateType>::expandAndClip(BeliefId const &beliefId, uint64_t actionIndex,
                                                                    std::vector<uint64_t> const &observationResolutions) {
    return expandInternal(beliefId, actionIndex, std::nullopt, observationResolutions);
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::vector<std::pair<typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefId,
                      typename BeliefManager<PomdpType, BeliefValueType, StateType>::ValueType>>
BeliefManager<PomdpType, BeliefValueType, StateType>::expand(BeliefId const &beliefId, uint64_t actionIndex) {
    return expandInternal(beliefId, actionIndex);
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefType const &BeliefManager<PomdpType, BeliefValueType, StateType>::getBelief(
    BeliefId const &id) const {
    STORM_LOG_ASSERT(id != noId(), "Tried to get a non-existent belief.");
    STORM_LOG_ASSERT(id < getNumberOfBeliefIds(), "Belief index " << id << " is out of range.");
    return beliefs[id];
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefId BeliefManager<PomdpType, BeliefValueType, StateType>::getId(
    BeliefType const &belief) const {
    uint32_t obs = getBeliefObservation(belief);
    STORM_LOG_ASSERT(obs < beliefToIdMap.size(), "Belief has unknown observation.");
    auto idIt = beliefToIdMap[obs].find(belief);
    STORM_LOG_ASSERT(idIt != beliefToIdMap[obs].end(), "Unknown Belief.");
    return idIt->second;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::string BeliefManager<PomdpType, BeliefValueType, StateType>::toString(BeliefType const &belief) const {
    std::stringstream str;
    str << "{ ";
    bool first = true;
    for (auto const &entry : belief) {
        if (first) {
            first = false;
        } else {
            str << ", ";
        }
        str << entry.first << ": " << std::setprecision(std::numeric_limits<double>::max_digits10 + 1) << entry.second;
    }
    str << " }";
    return str.str();
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
bool BeliefManager<PomdpType, BeliefValueType, StateType>::isEqual(BeliefType const &first, BeliefType const &second) const {
    if (first.size() != second.size()) {
        return false;
    }
    auto secondIt = second.begin();
    for (auto const &firstEntry : first) {
        if (firstEntry.first != secondIt->first) {
            return false;
        }
        if (!cc.isEqual(firstEntry.second, secondIt->second)) {
            return false;
        }
        ++secondIt;
    }
    return true;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
bool BeliefManager<PomdpType, BeliefValueType, StateType>::assertBelief(BeliefType const &belief) const {
    auto sum = storm::utility::zero<BeliefValueType>();
    std::optional<uint32_t> observation;
    for (auto const &entry : belief) {
        if (entry.first >= pomdp.getNumberOfStates()) {
            STORM_LOG_ERROR("Belief does refer to non-existing pomdp state " << entry.first << ".");
            return false;
        }
        uint64_t entryObservation = pomdp.getObservation(entry.first);
        if (observation) {
            if (observation.value() != entryObservation) {
                STORM_LOG_ERROR("Beliefsupport contains different observations.");
                return false;
            }
        } else {
            observation = entryObservation;
        }
        // Don't use cc for these checks, because computations with zero are usually fine
        if (storm::utility::isZero(entry.second)) {
            // We assume that beliefs only consider their support.
            STORM_LOG_ERROR("Zero belief probability.");
            return false;
        }
        if (entry.second < storm::utility::zero<BeliefValueType>()) {
            STORM_LOG_ERROR("Negative belief probability.");
            return false;
        }
        if (cc.isLess(storm::utility::one<BeliefValueType>(), entry.second)) {
            STORM_LOG_ERROR("Belief probability greater than one.");
            return false;
        }
        sum += entry.second;
    }
    if (!cc.isOne(sum)) {
        STORM_LOG_ERROR("Belief does not sum up to one. (" << sum << " instead).");
        return false;
    }
    return true;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
bool BeliefManager<PomdpType, BeliefValueType, StateType>::assertTriangulation(BeliefType const &belief, Triangulation const &triangulation) const {
    if (triangulation.weights.size() != triangulation.gridPoints.size()) {
        STORM_LOG_ERROR("Number of weights and points in triangulation does not match.");
        return false;
    }
    if (triangulation.size() == 0) {
        STORM_LOG_ERROR("Empty triangulation.");
        return false;
    }
    BeliefType triangulatedBelief;
    auto weightSum = storm::utility::zero<BeliefValueType>();
    for (uint64_t i = 0; i < triangulation.weights.size(); ++i) {
        if (cc.isZero(triangulation.weights[i])) {
            STORM_LOG_ERROR("Zero weight in triangulation.");
            return false;
        }
        if (cc.isLess(triangulation.weights[i], storm::utility::zero<BeliefValueType>())) {
            STORM_LOG_ERROR("Negative weight in triangulation.");
            return false;
        }
        if (cc.isLess(storm::utility::one<BeliefValueType>(), triangulation.weights[i])) {
            STORM_LOG_ERROR("Weight greater than one in triangulation.");
        }
        weightSum += triangulation.weights[i];
        BeliefType const &gridPoint = getBelief(triangulation.gridPoints[i]);
        for (auto const &pointEntry : gridPoint) {
            BeliefValueType &triangulatedValue = triangulatedBelief.emplace(pointEntry.first, storm::utility::zero<BeliefValueType>()).first->second;
            triangulatedValue += triangulation.weights[i] * pointEntry.second;
        }
    }
    if (!cc.isOne(weightSum)) {
        STORM_LOG_ERROR("Triangulation weights do not sum up to one.");
        return false;
    }
    if (!assertBelief(triangulatedBelief)) {
        STORM_LOG_ERROR("Triangulated belief is not a belief.");
    }
    if (!isEqual(belief, triangulatedBelief)) {
        STORM_LOG_ERROR("Belief:\n\t" << toString(belief) << "\ndoes not match triangulated belief:\n\t" << toString(triangulatedBelief) << ".");
        return false;
    }
    return true;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
uint32_t BeliefManager<PomdpType, BeliefValueType, StateType>::getBeliefObservation(BeliefType belief) const {
    STORM_LOG_ASSERT(assertBelief(belief), "Invalid belief.");
    return pomdp.getObservation(belief.begin()->first);
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
void BeliefManager<PomdpType, BeliefValueType, StateType>::triangulateBeliefFreudenthal(BeliefType const &belief, BeliefValueType const &resolution,
                                                                                        Triangulation &result) {
    STORM_LOG_ASSERT(resolution != 0, "Invalid resolution: 0");
    STORM_LOG_ASSERT(storm::utility::isInteger(resolution), "Expected an integer resolution");
    StateType numEntries = belief.size();
    // This is the Freudenthal Triangulation as described in Lovejoy (a whole lotta math)
    // Probabilities will be triangulated to values in 0/N, 1/N, 2/N, ..., N/N
    // Variable names are mostly based on the paper
    // However, we speed this up a little by exploiting that belief states usually have sparse support (i.e. numEntries is much smaller than
    // pomdp.getNumberOfStates()). Initialize diffs and the first row of the 'qs' matrix (aka v)
    std::set<FreudenthalDiff, std::greater<>> sorted_diffs;  // d (and p?) in the paper
    std::vector<BeliefValueType> qsRow;                      // Row of the 'qs' matrix from the paper (initially corresponds to v
    qsRow.reserve(numEntries);
    std::vector<StateType> toOriginalIndicesMap;  // Maps 'local' indices to the original pomdp state indices
    toOriginalIndicesMap.reserve(numEntries);
    BeliefValueType x = resolution;
    for (auto const &entry : belief) {
        qsRow.push_back(storm::utility::floor(x));                            // v
        sorted_diffs.emplace(toOriginalIndicesMap.size(), x - qsRow.back());  // x-v
        toOriginalIndicesMap.push_back(entry.first);
        x -= entry.second * resolution;
    }
    // Insert a dummy 0 column in the qs matrix so the loops below are a bit simpler
    qsRow.push_back(storm::utility::zero<BeliefValueType>());

    result.weights.reserve(numEntries);
    result.gridPoints.reserve(numEntries);
    auto currentSortedDiff = sorted_diffs.begin();
    auto previousSortedDiff = sorted_diffs.end();
    --previousSortedDiff;
    for (StateType i = 0; i < numEntries; ++i) {
        // Compute the weight for the grid points
        BeliefValueType weight = previousSortedDiff->diff - currentSortedDiff->diff;
        if (i == 0) {
            // The first weight is a bit different
            weight += storm::utility::one<BeliefValueType>();
        } else {
            // 'compute' the next row of the qs matrix
            qsRow[previousSortedDiff->dimension] += storm::utility::one<BeliefValueType>();
        }
        if (!cc.isZero(weight)) {
            result.weights.push_back(weight);
            // Compute the grid point
            BeliefType gridPoint;
            for (StateType j = 0; j < numEntries; ++j) {
                BeliefValueType gridPointEntry = qsRow[j] - qsRow[j + 1];
                if (!cc.isZero(gridPointEntry)) {
                    gridPoint[toOriginalIndicesMap[j]] = gridPointEntry / resolution;
                }
            }
            result.gridPoints.push_back(getOrAddBeliefId(gridPoint));
        }
        previousSortedDiff = currentSortedDiff++;
    }
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
void BeliefManager<PomdpType, BeliefValueType, StateType>::triangulateBeliefDynamic(BeliefType const &belief, BeliefValueType const &resolution,
                                                                                    Triangulation &result) {
    // Find the best resolution for this belief, i.e., N such that the largest distance between one of the belief values to a value in {i/N | 0 ≤ i ≤ N} is
    // minimal
    STORM_LOG_ASSERT(storm::utility::isInteger(resolution), "Expected an integer resolution");
    BeliefValueType finalResolution = resolution;
    uint64_t finalResolutionMisses = belief.size() + 1;
    // We don't need to check resolutions that are smaller than the maximal resolution divided by 2 (as we already checked multiples of these)
    for (BeliefValueType currResolution = resolution; currResolution > resolution / 2; --currResolution) {
        uint64_t currResMisses = 0;
        bool continueWithNextResolution = false;
        for (auto const &belEntry : belief) {
            BeliefValueType product = belEntry.second * currResolution;
            if (!cc.isZero(product - storm::utility::round(product))) {
                ++currResMisses;
                if (currResMisses >= finalResolutionMisses) {
                    // This resolution is not better than a previous resolution
                    continueWithNextResolution = true;
                    break;
                }
            }
        }
        if (!continueWithNextResolution) {
            STORM_LOG_ASSERT(currResMisses < finalResolutionMisses, "Distance for this resolution should not be larger than a previously checked one.");
            finalResolution = currResolution;
            finalResolutionMisses = currResMisses;
            if (currResMisses == 0) {
                break;
            }
        }
    }

    STORM_LOG_TRACE("Picking resolution " << finalResolution << " for belief " << toString(belief));

    // do standard freudenthal with the found resolution
    triangulateBeliefFreudenthal(belief, finalResolution, result);
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::Triangulation BeliefManager<PomdpType, BeliefValueType, StateType>::triangulateBelief(
    BeliefType const &belief, BeliefValueType const &resolution) {
    STORM_LOG_ASSERT(assertBelief(belief), "Input belief for triangulation is not valid.");
    Triangulation result;
    // Quickly triangulate Dirac beliefs
    if (belief.size() == 1u) {
        result.weights.push_back(storm::utility::one<BeliefValueType>());
        result.gridPoints.push_back(getOrAddBeliefId(belief));
    } else {
        auto ceiledResolution = storm::utility::ceil<BeliefValueType>(resolution);
        switch (triangulationMode) {
            case TriangulationMode::Static:
                triangulateBeliefFreudenthal(belief, ceiledResolution, result);
                break;
            case TriangulationMode::Dynamic:
                triangulateBeliefDynamic(belief, ceiledResolution, result);
                break;
            default:
                STORM_LOG_ASSERT(false, "Invalid triangulation mode.");
        }
    }
    STORM_LOG_ASSERT(assertTriangulation(belief, result), "Incorrect triangulation: " << toString(result));
    return result;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::vector<std::pair<typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefId,
                      typename BeliefManager<PomdpType, BeliefValueType, StateType>::ValueType>>
BeliefManager<PomdpType, BeliefValueType, StateType>::expandInternal(BeliefId const &beliefId, uint64_t actionIndex,
                                                                     std::optional<std::vector<BeliefValueType>> const &observationTriangulationResolutions,
                                                                     std::optional<std::vector<uint64_t>> const &observationGridClippingResolutions) {
    std::vector<std::pair<BeliefId, ValueType>> destinations;

    BeliefType belief = getBelief(beliefId);

    // Find the probability we go to each observation
    BeliefType successorObs;  // This is actually not a belief but has the same type
    for (auto const &pointEntry : belief) {
        uint64_t state = pointEntry.first;
        for (auto const &pomdpTransition : pomdp.getTransitionMatrix().getRow(state, actionIndex)) {
            if (!storm::utility::isZero(pomdpTransition.getValue())) {
                auto obs = pomdp.getObservation(pomdpTransition.getColumn());
                addToDistribution(successorObs, obs, pointEntry.second * storm::utility::convertNumber<BeliefValueType>(pomdpTransition.getValue()));
            }
        }
    }
    adjustDistribution(successorObs);

    // Now for each successor observation we find and potentially triangulate the successor belief
    for (auto const &successor : successorObs) {
        BeliefType successorBelief;
        for (auto const &pointEntry : belief) {
            uint64_t state = pointEntry.first;
            for (auto const &pomdpTransition : pomdp.getTransitionMatrix().getRow(state, actionIndex)) {
                if (pomdp.getObservation(pomdpTransition.getColumn()) == successor.first) {
                    BeliefValueType prob = pointEntry.second * storm::utility::convertNumber<BeliefValueType>(pomdpTransition.getValue()) / successor.second;
                    addToDistribution(successorBelief, pomdpTransition.getColumn(), prob);
                }
            }
        }
        adjustDistribution(successorBelief);
        STORM_LOG_ASSERT(assertBelief(successorBelief), "Invalid successor belief.");

        // Insert the destination. We know that destinations have to be disjoint since they have different observations
        if (observationTriangulationResolutions) {
            Triangulation triangulation = triangulateBelief(successorBelief, observationTriangulationResolutions.value()[successor.first]);
            for (size_t j = 0; j < triangulation.size(); ++j) {
                // Here we additionally assume that triangulation.gridPoints does not contain the same point multiple times
                BeliefValueType a = triangulation.weights[j] * successor.second;
                destinations.emplace_back(triangulation.gridPoints[j], storm::utility::convertNumber<ValueType>(a));
            }
        } else if (observationGridClippingResolutions) {
            BeliefClipping clipping = clipBeliefToGrid(successorBelief, observationGridClippingResolutions.value()[successor.first],
                                                       storm::storage::BitVector(pomdp.getNumberOfStates()));
            if (clipping.isClippable) {
                BeliefValueType a = (storm::utility::one<BeliefValueType>() - clipping.delta) * successor.second;
                destinations.emplace_back(clipping.targetBelief, storm::utility::convertNumber<ValueType>(a));
            } else {
                // Belief on Grid
                destinations.emplace_back(getOrAddBeliefId(successorBelief), storm::utility::convertNumber<ValueType>(successor.second));
            }
        } else {
            destinations.emplace_back(getOrAddBeliefId(successorBelief), storm::utility::convertNumber<ValueType>(successor.second));
        }
    }

    return destinations;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefClipping BeliefManager<PomdpType, BeliefValueType, StateType>::clipBeliefToGrid(
    BeliefId const &beliefId, uint64_t resolution, storm::storage::BitVector isInfinite) {
    auto res = clipBeliefToGrid(getBelief(beliefId), resolution, isInfinite);
    res.startingBelief = beliefId;
    return res;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefClipping BeliefManager<PomdpType, BeliefValueType, StateType>::clipBeliefToGrid(
    BeliefType const &belief, uint64_t resolution, const storm::storage::BitVector &isInfinite) {
    uint32_t obs = getBeliefObservation(belief);
    STORM_LOG_ASSERT(obs < beliefToIdMap.size(), "Belief has unknown observation.");
    if (!lpSolver) {
        lpSolver = storm::utility::solver::getLpSolver<BeliefValueType>("POMDP LP Solver");
    } else {
        lpSolver->pop();
    }
    lpSolver->push();

    std::vector<BeliefValueType> helper(belief.size(), storm::utility::zero<BeliefValueType>());
    helper[0] = storm::utility::convertNumber<BeliefValueType>(resolution);
    bool done = false;
    // Set-up Variables
    std::vector<storm::expressions::Expression> decisionVariables;
    // Add variable for the clipping value, it is to be minimized
    auto bigDelta = lpSolver->addBoundedContinuousVariable("D", storm::utility::zero<BeliefValueType>(), storm::utility::one<BeliefValueType>(),
                                                           storm::utility::one<BeliefValueType>());
    // State clipping values
    std::vector<storm::expressions::Expression> deltas;
    uint64_t i = 0;
    for (auto const &state : belief) {
        // This is a quite dirty fix to enable GLPK for the TACAS '22 implementation without substantially changing the implementation for Gurobi.
        if (typeid(*lpSolver) == typeid(storm::solver::GlpkLpSolver<ValueType>) && !isInfinite.empty()) {
            if (isInfinite[state.first]) {
                auto localDelta = lpSolver->addBoundedContinuousVariable("d_" + std::to_string(i), storm::utility::zero<BeliefValueType>(), state.second);
                auto deltaExpr = storm::expressions::Expression(localDelta);
                deltas.push_back(deltaExpr);
                lpSolver->addConstraint("state_val_inf_" + std::to_string(i), deltaExpr == lpSolver->getConstant(storm::utility::zero<BeliefValueType>()));
            }
        } else {
            BeliefValueType bound = state.second;
            if (!isInfinite.empty()) {
                bound = isInfinite[state.first] ? storm::utility::zero<BeliefValueType>() : state.second;
            }
            auto localDelta = lpSolver->addBoundedContinuousVariable("d_" + std::to_string(i), storm::utility::zero<BeliefValueType>(), bound);
            deltas.push_back(storm::expressions::Expression(localDelta));
        }
        ++i;
    }
    lpSolver->update();
    std::vector<BeliefType> gridCandidates;
    while (!done) {
        BeliefType candidate;
        auto belIter = belief.begin();
        for (uint64_t j = 0; j < belief.size() - 1; ++j) {
            if (!cc.isEqual(helper[j] - helper[j + 1], storm::utility::zero<BeliefValueType>())) {
                candidate[belIter->first] = (helper[j] - helper[j + 1]) / storm::utility::convertNumber<BeliefValueType>(resolution);
            }
            belIter++;
        }
        if (!cc.isEqual(helper[belief.size() - 1], storm::utility::zero<BeliefValueType>())) {
            candidate[belIter->first] = helper[belief.size() - 1] / storm::utility::convertNumber<BeliefValueType>(resolution);
        }
        if (isEqual(candidate, belief)) {
            // TODO Improve handling of successors which are already on the grid
            return BeliefClipping{false, noId(), noId(), storm::utility::zero<BeliefValueType>(), {}, true};
        } else {
            gridCandidates.push_back(candidate);

            // Add variables a_j
            auto decisionVar = lpSolver->addBinaryVariable("a_" + std::to_string(gridCandidates.size() - 1));
            decisionVariables.push_back(storm::expressions::Expression(decisionVar));
            lpSolver->update();

            i = 0;
            for (auto const &state : belief) {
                // Add the constraint to describe the transformation between the state values in the beliefs
                // d_i
                storm::expressions::Expression leftSide = deltas[i];
                storm::expressions::Expression targetValue = lpSolver->getConstant(candidate[i]);
                if (candidate.find(state.first) != candidate.end()) {
                    targetValue = lpSolver->getConstant(candidate.at(state.first));
                } else {
                    targetValue = lpSolver->getConstant(storm::utility::zero<BeliefValueType>());
                }

                // b(s_i) - b_j(s_i) + D * b_j(s_i) - 1 + a_j
                storm::expressions::Expression rightSide =
                    lpSolver->getConstant(state.second) - targetValue + storm::expressions::Expression(bigDelta) * targetValue -
                    lpSolver->getConstant(storm::utility::one<BeliefValueType>()) + storm::expressions::Expression(decisionVar);

                // Add left >= right
                lpSolver->addConstraint("state_eq_" + std::to_string(i) + "_" + std::to_string(gridCandidates.size() - 1), leftSide >= rightSide);
                ++i;
                lpSolver->update();
            }
        }
        if (helper.back() == storm::utility::convertNumber<BeliefValueType>(resolution)) {
            // If the last entry of helper is the gridResolution, we have enumerated all necessary distributions
            done = true;
        } else {
            // Update helper by finding the index to increment
            auto helperIt = helper.end() - 1;
            while (*helperIt == *(helperIt - 1)) {
                --helperIt;
            }
            STORM_LOG_ASSERT(helperIt != helper.begin(), "Error in grid clipping - index wrong");
            // Increment the value at the index
            *helperIt += 1;
            // Reset all indices greater than the changed one to 0
            ++helperIt;
            while (helperIt != helper.end()) {
                *helperIt = 0;
                ++helperIt;
            }
        }
    }

    // Only one target belief should be chosen
    lpSolver->addConstraint("choice", storm::expressions::sum(decisionVariables) == lpSolver->getConstant(storm::utility::one<BeliefValueType>()));
    // Link D and d_i
    lpSolver->addConstraint("delta", storm::expressions::Expression(bigDelta) == storm::expressions::sum(deltas));
    // Exclude D = 0 (self-loop)
    lpSolver->addConstraint("not_zero", storm::expressions::Expression(bigDelta) > lpSolver->getConstant(storm::utility::zero<BeliefValueType>()));

    lpSolver->update();

    lpSolver->optimize();
    // Get the optimal belief for clipping
    BeliefId targetBelief = noId();
    // Not a belief but has the same type
    BeliefType deltaValues;
    auto optDelta = storm::utility::zero<BeliefValueType>();
    auto deltaSum = storm::utility::zero<BeliefValueType>();
    if (lpSolver->isOptimal()) {
        optDelta = lpSolver->getObjectiveValue();
        for (uint64_t dist = 0; dist < gridCandidates.size(); ++dist) {
            if (lpSolver->getBinaryValue(lpSolver->getManager().getVariable("a_" + std::to_string(dist)))) {
                targetBelief = getOrAddBeliefId(gridCandidates[dist]);
                break;
            }
        }
        i = 0;
        for (auto const &state : belief) {
            auto val = lpSolver->getContinuousValue(lpSolver->getManager().getVariable("d_" + std::to_string(i)));
            if (cc.isLess(storm::utility::zero<BeliefValueType>(), val)) {
                deltaValues.emplace(state.first, val);
                deltaSum += val;
            }
            ++i;
        }

        if (cc.isEqual(optDelta, storm::utility::zero<BeliefValueType>())) {
            // If we get an optimal value of 0, the LP solver considers two beliefs to be equal, possibly due to numerical instability
            // For a sound result, we consider the state to not be clippable
            STORM_LOG_WARN("LP solver returned an optimal value of 0. This should definitely not happen when using a grid");
            STORM_LOG_WARN("Origin" << toString(belief));
            STORM_LOG_WARN("Target [Bel " << targetBelief << "] " << toString(targetBelief));
            return BeliefClipping{false, noId(), noId(), storm::utility::zero<BeliefValueType>(), {}, false};
        }

        if (optDelta == storm::utility::one<BeliefValueType>()) {
            STORM_LOG_WARN("LP solver returned an optimal value of 1. Sum of state clipping values is " << deltaSum);
            // If we get an optimal value of 1, we cannot clip the belief as by definition this would correspond to a division by 0.
            STORM_LOG_DEBUG("Origin" << toString(belief));
            STORM_LOG_DEBUG("Target [Bel " << targetBelief << "] " << toString(targetBelief));

            if (deltaSum == storm::utility::one<BeliefValueType>()) {
                return BeliefClipping{false, noId(), noId(), storm::utility::zero<BeliefValueType>(), {}, false};
            }
            optDelta = deltaSum;
        }
    }
    return BeliefClipping{lpSolver->isOptimal(), noId(), targetBelief, optDelta, deltaValues, false};
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefId BeliefManager<PomdpType, BeliefValueType, StateType>::computeInitialBelief() {
    STORM_LOG_ASSERT(pomdp.getInitialStates().getNumberOfSetBits() < 2, "POMDP contains more than one initial state");
    STORM_LOG_ASSERT(pomdp.getInitialStates().getNumberOfSetBits() == 1, "POMDP does not contain an initial state");
    BeliefType belief;
    belief[*pomdp.getInitialStates().begin()] = storm::utility::one<BeliefValueType>();

    STORM_LOG_ASSERT(assertBelief(belief), "Invalid initial belief.");
    return getOrAddBeliefId(belief);
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
typename BeliefManager<PomdpType, BeliefValueType, StateType>::BeliefId BeliefManager<PomdpType, BeliefValueType, StateType>::getOrAddBeliefId(
    BeliefType const &belief) {
    uint32_t obs = getBeliefObservation(belief);
    STORM_LOG_ASSERT(obs < beliefToIdMap.size(), "Belief has unknown observation.");
    auto insertioRes = beliefToIdMap[obs].emplace(belief, beliefs.size());
    if (insertioRes.second) {
        // There actually was an insertion, so add the new belief
        STORM_LOG_TRACE("Add Belief " << beliefs.size() << " " << toString(belief));
        beliefs.push_back(belief);
    }
    // Return the id
    return insertioRes.first->second;
}
template<typename PomdpType, typename BeliefValueType, typename StateType>
uint64_t BeliefManager<PomdpType, BeliefValueType, StateType>::getRepresentativeState(BeliefId const &beliefId) {
    return getBelief(beliefId).begin()->first;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::string BeliefManager<PomdpType, BeliefValueType, StateType>::getObservationLabel(BeliefId const &beliefId) {
    if (pomdp.hasObservationValuations()) {
        return pomdp.getObservationValuations().toString(getBeliefObservation(beliefId));
    } else {
        STORM_LOG_TRACE("Cannot get observation labels as no observation valuation has been defined for the POMDP. Return empty label instead.");
        return "";
    }
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::vector<BeliefValueType> BeliefManager<PomdpType, BeliefValueType, StateType>::getBeliefAsVector(BeliefId const &beliefId) {
    return getBeliefAsVector(getBelief(beliefId));
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::vector<BeliefValueType> BeliefManager<PomdpType, BeliefValueType, StateType>::getBeliefAsVector(const BeliefType &belief) {
    std::vector<BeliefValueType> res(pomdp.getNumberOfStates(), storm::utility::zero<BeliefValueType>());
    for (auto const &stateprob : belief) {
        res[stateprob.first] = stateprob.second;
    }
    return res;
}

template<typename PomdpType, typename BeliefValueType, typename StateType>
std::vector<BeliefValueType> BeliefManager<PomdpType, BeliefValueType, StateType>::computeMatrixBeliefProduct(
    const BeliefId &beliefId, storm::storage::SparseMatrix<BeliefValueType> &matrix) {
    std::vector<BeliefValueType> beliefAsVector = getBeliefAsVector(beliefId);
    std::vector<BeliefValueType> res(matrix.getRowCount());
    matrix.multiplyWithVector(beliefAsVector, res);
    return res;
}

template class BeliefManager<storm::models::sparse::Pomdp<double>>;
template class BeliefManager<storm::models::sparse::Pomdp<double>, storm::RationalNumber>;

template class BeliefManager<storm::models::sparse::Pomdp<storm::RationalNumber>, double>;
template class BeliefManager<storm::models::sparse::Pomdp<storm::RationalNumber>>;
}  // namespace storage
}  // namespace storm
