#include "ReachabilityRewardCertificate.h"

#include <algorithm>
#include <sstream>

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/modelchecker/certificates/utility/ExtendedValueUtility.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/modelchecker/certificates/utility/CertificateJsonUtility.h"

namespace storm::modelchecker {

template<typename ValueType>
ReachabilityRewardCertificate<ValueType>::ReachabilityRewardCertificate(std::optional<storm::OptimizationDirection> dir, storm::storage::BitVector targetStates,
                                                                        std::vector<ValueType> stateActionRewardVector, std::string targetLabel,
                                                                        std::string rewardModelName)
    : Certificate<ValueType>(CertificateKind::ReachabilityRewards),
      targetStates(std::move(targetStates)),
      stateActionRewardVector(std::move(stateActionRewardVector)),
      targetLabel(targetLabel),
      rewardModelName(rewardModelName),
      dir(dir) {}

template<typename ValueType>
bool ReachabilityRewardCertificate<ValueType>::checkValidity(storm::models::Model<ValueType> const& model) const {
    if (!model.isSparseModel()) {
        STORM_LOG_WARN("Certificate invalid because the given model is not sparse.");
        return false;
    }
    if (model.isOfType(storm::models::ModelType::Ctmc)) {
        if (auto const* ctmcPtr = dynamic_cast<storm::models::sparse::Ctmc<ValueType> const*>(&model)) {
            return checkValidity(ctmcPtr->computeProbabilityMatrix());
        }
    } else if (model.isOfType(storm::models::ModelType::Dtmc) || model.isOfType(storm::models::ModelType::Mdp) ||
               model.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        if (auto const* sparseModelPtr = dynamic_cast<storm::models::sparse::Model<ValueType> const*>(&model)) {
            return checkValidity(sparseModelPtr->getTransitionMatrix());
        }
    }
    STORM_LOG_WARN("Certificate invalid because the given model is of unsupported type.");
    return false;
}

template<typename ValueType, OptimizationDirection Dir>
bool checkUpperBoundCertificate(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix, storm::storage::BitVector const& targetStates,
                                std::vector<ValueType> const& stateActionRewardVector, std::vector<storm::utility::ExtendedValueType<ValueType>> const& values,
                                std::vector<RankingType> const& ranks) {
    for (uint64_t state = 0; state < targetStates.size(); ++state) {
        if (storm::utility::isNegativeInfinity(values[state])) {
            STORM_LOG_WARN("Certificate invalid because state " << state << " has negative infinity as value.");
            return false;
        }
        // Handle states that do not almost surely reach target
        if (storm::utility::isFinite(values[state]) && ranks[state] == InfRank) {
            STORM_LOG_WARN("Certificate invalid because state " << state << " has infinite rank but finite value " << values[state] << ".");
            return false;
        }
        if (targetStates.get(state)) {
            // Handle target state
            if (values[state] < storm::utility::zero<ValueType>()) {
                STORM_LOG_WARN("Certificate invalid because target state " << state << " has negative upper bound " << values[state]);
                return false;
            }
            // The rank of a target state must be non-negative, too.
            // However, a negative rank (which is an unsigned integer) is not possible, anyway.
            // So, we skip checking for the rank
            continue;
        }

        // Apply Bellman and Distance operator for non-target states
        // Following Propositions 8 and 9 of https://doi.org/10.1007/978-3-031-90653-4_7 (see also the arXiv version), the rank only considers
        // the x-decreasing (i.e. inductive) choices if we minimize and all choices if we maximize.
        auto stateValue = storm::solver::maximize(Dir) ? storm::utility::negativeInfinity<ValueType>() : storm::utility::positiveInfinity<ValueType>();
        storm::utility::Extremum<Dir, RankingType> stateRankMinusOne;
        for (auto choice : transitionProbabilityMatrix.getRowGroupIndices(state)) {
            auto const currentChoiceValue =
                detail::multiplyRowWithVectorAddNumber(transitionProbabilityMatrix, choice, values, stateActionRewardVector[choice]);
            stateValue = storm::solver::maximize(Dir) ? std::max(stateValue, currentChoiceValue) : std::min(stateValue, currentChoiceValue);
            // Compute rank for this choice if we're maximizing or if the choice is inductive
            if (storm::solver::maximize(Dir) || currentChoiceValue <= values[state]) {
                storm::utility::Minimum<RankingType> choiceRank;
                for (auto const& entry : transitionProbabilityMatrix.getRow(choice)) {
                    if (storm::utility::isZero(entry.getValue())) {
                        continue;
                    }
                    choiceRank &= ranks[entry.getColumn()];
                }
                STORM_LOG_ASSERT(!choiceRank.empty(),
                                 "Ranking operator failed to compute rank for state " << state << " since the row " << choice << " is empty.");
                stateRankMinusOne &= *choiceRank;
            }
        }
        if (values[state] < stateValue) {
            double const approxDiff = storm::utility::convertNumber<double>(stateValue - values[state]);
            STORM_LOG_WARN("Certificate invalid because upper bound is not inductive. At state " << state << " the Bellman operator yields " << stateValue
                                                                                                 << " but the certificate has smaller value " << values[state]
                                                                                                 << ". Approx. diff is " << approxDiff << ".");
            return false;
        }
        if (stateRankMinusOne.empty()) {
            STORM_LOG_WARN("Certificate invalid because there is no valid (i.e. decreasing) action at state " << state << ", where the certificate has rank "
                                                                                                              << ranks[state] << ".");
            return false;
        }
        RankingType const stateRank = *stateRankMinusOne == InfRank ? InfRank : *stateRankMinusOne + 1;  // avoid integer overflow!
        if (stateRank > ranks[state]) {
            STORM_LOG_WARN("Certificate invalid because ranks for upper bound is not inductive. At state "
                           << state << " the rank operator yields " << stateRank << " but the certificate has smaller rank " << ranks[state] << ".");
            return false;
        }
    }
    return true;
}

template<typename ValueType, OptimizationDirection Dir>
bool checkLowerBoundCertificate(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix, storm::storage::BitVector const& targetStates,
                                std::vector<ValueType> const& stateActionRewardVector, std::vector<storm::utility::ExtendedValueType<ValueType>> const& values,
                                std::vector<RankingType> const& ranks) {
    for (uint64_t state = 0; state < targetStates.size(); ++state) {
        // Handle target state
        if (targetStates.get(state)) {
            if (!storm::utility::isFinite(values[state]) || !storm::utility::isZero(storm::utility::getFinite(values[state]))) {
                STORM_LOG_WARN("Certificate invalid because target state " << state << " has non-zero lower bound " << values[state] << ".");
                return false;
            }
            if (ranks[state] != InfRank) {
                STORM_LOG_WARN("Certificate invalid because target state " << state << " has finite rank" << ranks[state] << ".");
                return false;
            }
            continue;
        }
        if (storm::utility::isInfinity(values[state]) && ranks[state] == InfRank) {
            STORM_LOG_WARN("Certificate invalid because state " << state << " has infinite rank and value.");
            return false;
        }
        // Apply ranking- and Bellman operators for non-target states
        // Following Proposition 7 of https://doi.org/10.1007/978-3-031-90653-4_7 (see also the arXiv version), the (complementary distance) rank
        // considers all choices, no matter whether we minimize or maximize. Unlike for lower bounds on maximal reachability probabilities, there is no
        // restriction to inductive choices.
        auto stateValue = storm::solver::maximize(Dir) ? storm::utility::negativeInfinity<ValueType>() : storm::utility::positiveInfinity<ValueType>();
        storm::utility::Extremum<storm::solver::invert(Dir), RankingType> stateRank;

        for (auto choice : transitionProbabilityMatrix.getRowGroupIndices(state)) {
            auto const currentChoiceValue =
                detail::multiplyRowWithVectorAddNumber(transitionProbabilityMatrix, choice, values, stateActionRewardVector[choice]);
            stateValue = storm::solver::maximize(Dir) ? std::max(stateValue, currentChoiceValue) : std::min(stateValue, currentChoiceValue);
            // Compute rank for this choice
            storm::utility::Minimum<RankingType> minSuccessorRank;
            bool allSuccessorsEqual = true;
            for (auto const& entry : transitionProbabilityMatrix.getRow(choice)) {
                if (storm::utility::isZero(entry.getValue())) {
                    continue;
                }
                if (!minSuccessorRank.empty() && ranks[entry.getColumn()] != *minSuccessorRank) {
                    allSuccessorsEqual = false;
                }
                minSuccessorRank &= ranks[entry.getColumn()];
            }
            STORM_LOG_ASSERT(!minSuccessorRank.empty(),
                             "Ranking operator failed to compute rank for state " << state << " since the row " << choice << " is empty.");
            auto choiceRank = *minSuccessorRank;
            if (choiceRank != InfRank && !allSuccessorsEqual) {
                ++choiceRank;
            }
            stateRank &= choiceRank;
        }

        if (stateValue < values[state]) {
            double const approxDiff = storm::utility::convertNumber<double>(values[state] - stateValue);
            STORM_LOG_WARN("Certificate invalid because lower bound is not inductive. At state " << state << " the Bellman operator yields " << stateValue
                                                                                                 << " but the certificate has larger value " << values[state]
                                                                                                 << ". Approx. diff is " << approxDiff << ".");
            return false;
        }
        STORM_LOG_ASSERT(!stateRank.empty(), "Failed to compute ranks for state " << state << " since the row group is empty.");
        if (*stateRank > ranks[state]) {
            STORM_LOG_WARN("Certificate invalid because ranks for lower bound is not inductive. At state "
                           << state << " the rank operator yields " << *stateRank << " but the certificate has smaller rank " << ranks[state] << ".");
            return false;
        }
    }
    return true;
}

template<typename ValueType>
bool ReachabilityRewardCertificate<ValueType>::checkValidity(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix) const {
    if (!dir.has_value() && !transitionProbabilityMatrix.hasTrivialRowGrouping()) {
        STORM_LOG_WARN("Certificate invalid because Matrix has non-trivial row grouping but no optimization direction given.");
        return false;
    }
    if (transitionProbabilityMatrix.getRowGroupCount() != targetStates.size() || transitionProbabilityMatrix.getColumnCount() != targetStates.size()) {
        STORM_LOG_WARN("Certificate invalid because Matrix has invalid dimensions (target states).");
        return false;
    }
    if (transitionProbabilityMatrix.getRowCount() != stateActionRewardVector.size()) {
        STORM_LOG_WARN("Certificate invalid because Matrix has invalid dimensions (reward vector).");
        return false;
    }

    if (dir.has_value() && storm::solver::maximize(*dir)) {
        return checkValidityInternal<storm::OptimizationDirection::Maximize>(transitionProbabilityMatrix);
    } else {
        return checkValidityInternal<storm::OptimizationDirection::Minimize>(transitionProbabilityMatrix);
    }
}

template<typename ValueType>
template<storm::OptimizationDirection Dir>
bool ReachabilityRewardCertificate<ValueType>::checkValidityInternal(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix) const {
    if (hasUpperBoundsCertificate() && !checkUpperBoundCertificate<ValueType, Dir>(transitionProbabilityMatrix, targetStates, stateActionRewardVector,
                                                                                   upperBoundsCertificate.values, upperBoundsCertificate.ranks)) {
        STORM_LOG_WARN("Certificate invalid because upper bound certificate is violated.");
        return false;
    }
    if (hasLowerBoundsCertificate() && !checkLowerBoundCertificate<ValueType, Dir>(transitionProbabilityMatrix, targetStates, stateActionRewardVector,
                                                                                   lowerBoundsCertificate.values, lowerBoundsCertificate.ranks)) {
        STORM_LOG_WARN("Certificate invalid because lower bound certificate is violated.");
        return false;
    }
    return true;
}

template<typename ValueType>
storm::json<ValueType> ReachabilityRewardCertificate<ValueType>::toJson() const {
    storm::json<ValueType> result;
    result["kind"] = "reachability-reward";
    if (dir.has_value()) {
        result["direction"] = storm::solver::minimize(*dir) ? "min" : "max";
    }
    result["reward-model"] = rewardModelName;
    result["target"] = detail::stateSetToJson<ValueType>(targetLabel, targetStates);
    result["choice-rewards"] = detail::valuesToJson<ValueType, ValueType>(stateActionRewardVector);
    if (hasLowerBoundsCertificate()) {
        result["lower"]["values"] = detail::valuesToJson<ValueType, ExtendedValueType>(lowerBoundsCertificate.values);
        result["lower"]["ranks"] = detail::ranksToJson<ValueType>(lowerBoundsCertificate.ranks);
    }
    if (hasUpperBoundsCertificate()) {
        result["upper"]["values"] = detail::valuesToJson<ValueType, ExtendedValueType>(upperBoundsCertificate.values);
        result["upper"]["ranks"] = detail::ranksToJson<ValueType>(upperBoundsCertificate.ranks);
    }
    return result;
}

template<typename ValueType>
std::string ReachabilityRewardCertificate<ValueType>::summaryString(storm::storage::BitVector const& relevantStates) const {
    std::stringstream ss;
    auto const numStates = relevantStates.getNumberOfSetBits();
    STORM_LOG_THROW(numStates > 0, storm::exceptions::InvalidOperationException,
                    "Tried to summarize reachability reward certificate but no state is relevant.");

    auto getLowerUpperBound = [this](uint64_t state) -> std::pair<ExtendedValueType, ExtendedValueType> {
        return {hasLowerBoundsCertificate() ? lowerBoundsCertificate.values[state] : storm::utility::negativeInfinity<ValueType>(),
                hasUpperBoundsCertificate() ? upperBoundsCertificate.values[state] : storm::utility::positiveInfinity<ValueType>()};
    };
    auto const [lowerBound, upperBound] = getLowerUpperBound(relevantStates.getNextSetIndex(0));
    ss << "Certified reachability reward: [" << lowerBound << ", " << upperBound << "]";
    if (storm::NumberTraits<ValueType>::IsExact) {
        ss << " (approx. [" << storm::utility::convertNumber<double>(lowerBound) << ", " << storm::utility::convertNumber<double>(upperBound) << "])";
    }
    if (numStates > 1) {
        ss << " (only showing first of " << numStates << " relevant states)";
    }
    ExtendedValueType const zero(storm::utility::zero<ValueType>());
    ExtendedValueType maxAbsDiff(zero), maxRelDiff(zero);
    for (auto const state : relevantStates) {
        auto const [lower, upper] = getLowerUpperBound(state);
        ExtendedValueType absDiff = (upper == lower) ? zero : (upper - lower);  // inf-inf undefined!
        ExtendedValueType relDiff;
        if (absDiff == zero) {
            relDiff = zero;
        } else if (lower == zero || storm::utility::isInfinity(absDiff)) {
            relDiff = storm::utility::positiveInfinity<ValueType>();
        } else {
            ValueType relDiffValue = storm::utility::getFinite(absDiff) / storm::utility::getFinite(lower);
            relDiff = ExtendedValueType(relDiffValue < storm::utility::zero<ValueType>() ? -relDiffValue : relDiffValue);
        }
        maxAbsDiff = std::max(maxAbsDiff, absDiff);
        maxRelDiff = std::max(maxRelDiff, relDiff);
    }
    auto printIntervalWidth([&](auto const& value, auto const& name) {
        ss << "\n\t" << name << " interval width: " << value;
        if (storm::NumberTraits<ValueType>::IsExact) {
            ss << " (approx. " << storm::utility::convertNumber<double>(value) << ")";
        }
        if (numStates > 1) {
            ss << " (showing maximum over " << numStates << " relevant states)";
        }
    });
    printIntervalWidth(maxAbsDiff, "Absolute");
    printIntervalWidth(maxRelDiff, "Relative");
    return ss.str();
}

template<typename ValueType>
std::unique_ptr<Certificate<ValueType>> ReachabilityRewardCertificate<ValueType>::clone() const {
    auto cloned = std::make_unique<ReachabilityRewardCertificate<ValueType>>(dir, targetStates, stateActionRewardVector, targetLabel, rewardModelName);
    if (hasLowerBoundsCertificate()) {
        auto v = lowerBoundsCertificate.values;
        auto r = lowerBoundsCertificate.ranks;
        cloned->setLowerBoundsCertificate(std::move(v), std::move(r));
    }
    if (hasUpperBoundsCertificate()) {
        auto v = upperBoundsCertificate.values;
        auto r = upperBoundsCertificate.ranks;
        cloned->setUpperBoundsCertificate(std::move(v), std::move(r));
    }
    return cloned;
}

template<typename ValueType>
void ReachabilityRewardCertificate<ValueType>::setLowerBoundsCertificate(std::vector<ExtendedValueType>&& values, std::vector<RankingType>&& ranks) {
    STORM_LOG_ASSERT(values.size() == targetStates.size(), "Values for lower bound certificate have incorrect dimension.");
    STORM_LOG_ASSERT(ranks.size() == targetStates.size(), "Ranks for lower bound certificate have incorrect dimension.");
    lowerBoundsCertificate.values = std::move(values);
    lowerBoundsCertificate.ranks = std::move(ranks);
}

template<typename ValueType>
void ReachabilityRewardCertificate<ValueType>::setUpperBoundsCertificate(std::vector<ExtendedValueType>&& values, std::vector<RankingType>&& ranks) {
    STORM_LOG_ASSERT(values.size() == targetStates.size(), "Values for upper bound certificate have incorrect dimension.");
    STORM_LOG_ASSERT(ranks.size() == targetStates.size(), "Ranks for lower bound certificate have incorrect dimension.");
    upperBoundsCertificate.values = std::move(values);
    upperBoundsCertificate.ranks = std::move(ranks);
}

template<typename ValueType>
bool ReachabilityRewardCertificate<ValueType>::hasLowerBoundsCertificate() const {
    return !lowerBoundsCertificate.values.empty();
}

template<typename ValueType>
bool ReachabilityRewardCertificate<ValueType>::hasUpperBoundsCertificate() const {
    return !upperBoundsCertificate.values.empty();
}

template class ReachabilityRewardCertificate<double>;
template class ReachabilityRewardCertificate<storm::RationalNumber>;

}  // namespace storm::modelchecker