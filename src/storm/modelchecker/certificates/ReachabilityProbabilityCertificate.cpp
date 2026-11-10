#include "ReachabilityProbabilityCertificate.h"

#include <algorithm>
#include <sstream>

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/modelchecker/certificates/utility/CertificateJsonUtility.h"

namespace storm::modelchecker {

template<typename ValueType>
ReachabilityProbabilityCertificate<ValueType>::ReachabilityProbabilityCertificate(std::optional<storm::OptimizationDirection> dir,
                                                                                  storm::storage::BitVector targetStates, std::string targetLabel)
    : Certificate<ValueType>(CertificateKind::ReachabilityProbability), targetStates(std::move(targetStates)), targetLabel(targetLabel), dir(dir) {}

template<typename ValueType>
ReachabilityProbabilityCertificate<ValueType>::ReachabilityProbabilityCertificate(std::optional<storm::OptimizationDirection> dir,
                                                                                  storm::storage::BitVector targetStates,
                                                                                  storm::storage::BitVector constraintStates, std::string targetLabel,
                                                                                  std::string constraintLabel)
    : Certificate<ValueType>(CertificateKind::ReachabilityProbability),
      targetStates(std::move(targetStates)),
      constraintStates(std::move(constraintStates)),
      targetLabel(targetLabel),
      constraintLabel(std::move(constraintLabel)),
      dir(dir) {
    STORM_LOG_ASSERT(this->constraintStates->size() == this->targetStates.size(), "Constraint set and target set consider a different state count.");
}

template<typename ValueType>
bool ReachabilityProbabilityCertificate<ValueType>::checkValidity(storm::models::Model<ValueType> const& model) const {
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
                                storm::OptionalRef<storm::storage::BitVector const> constraintStates, std::vector<ValueType> const& values) {
    for (uint64_t state = 0; state < targetStates.size(); ++state) {
        if (targetStates.get(state)) {
            // Handle target state
            if (values[state] < storm::utility::one<ValueType>()) {
                STORM_LOG_WARN("Certificate invalid because target state " << state << " has upper bound " << values[state] << " < 1.");
                return false;
            }
            continue;
        }
        // Probabilities are non-negative, so are upper bounds on probabilities
        if (values[state] < storm::utility::zero<ValueType>()) {
            STORM_LOG_WARN("Certificate invalid because state " << state << " has upper bound " << values[state] << " < 0.");
            return false;
        }
        // Handle states that are not in the constraint set
        if (constraintStates && !constraintStates->get(state)) {
            continue;
        }
        // Apply Bellman operator for non-target state
        storm::utility::Extremum<Dir, ValueType> stateValue;
        for (auto choice : transitionProbabilityMatrix.getRowGroupIndices(state)) {
            stateValue &= transitionProbabilityMatrix.multiplyRowWithVector(choice, values);
        }
        STORM_LOG_ASSERT(!stateValue.empty(), "Bellman operator failed to compute value for state " << state << " since the row group is empty.");
        if (*stateValue > values[state]) {
            double const approxDiff = storm::utility::convertNumber<double, ValueType>(*stateValue - values[state]);
            STORM_LOG_WARN("Certificate invalid because upper bound is not inductive. At state " << state << " the Bellman operator yields " << *stateValue
                                                                                                 << " but the certificate has smaller value " << values[state]
                                                                                                 << ". Approx. diff is " << approxDiff << ".");
            return false;
        }
    }
    return true;
}

template<typename ValueType, OptimizationDirection Dir>
bool checkLowerBoundCertificate(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix, storm::storage::BitVector const& targetStates,
                                storm::OptionalRef<storm::storage::BitVector const> constraintStates, std::vector<ValueType> const& values,
                                std::vector<RankingType> const& ranks) {
    for (uint64_t state = 0; state < targetStates.size(); ++state) {
        // Handle target state
        if (targetStates.get(state)) {
            if (values[state] > storm::utility::one<ValueType>()) {
                STORM_LOG_WARN("Certificate invalid because target state " << state << " has lower bound " << values[state] << " > 1.");
                return false;
            }
            continue;
        }
        // Handle states that are not in the constraint set
        if (constraintStates && !constraintStates->get(state)) {
            if (values[state] > storm::utility::zero<ValueType>()) {
                STORM_LOG_WARN("Certificate invalid because state " << state << " is not in the constraint set but has lower bound " << values[state]
                                                                    << " > 0.");
                return false;
            }
            if (ranks[state] != InfRank) {
                STORM_LOG_WARN("Certificate invalid because state " << state << " is not in the constraint set but has finite rank " << ranks[state] << ".");
                return false;
            }
            continue;
        }
        // Check that states with non-zero lower bound have finite rank
        if (values[state] > storm::utility::zero<ValueType>() && ranks[state] == InfRank) {
            STORM_LOG_WARN("Certificate invalid because infinite rank is assigned to a state " << state << " with non-zero lower bound " << values[state]
                                                                                               << ".");
            return false;
        }
        // Apply ranking- and Bellman operators for non-target state
        // Apply Bellman operator for non-target state
        storm::utility::Extremum<Dir, ValueType> stateValue;
        storm::utility::Extremum<storm::solver::invert(Dir), RankingType> stateRankMinusOne;

        for (auto choice : transitionProbabilityMatrix.getRowGroupIndices(state)) {
            auto currentChoiceValue = transitionProbabilityMatrix.multiplyRowWithVector(choice, values);
            stateValue &= currentChoiceValue;
            // Compute rank for this choice if we're minimizing or if the choice is inductive
            if (storm::solver::minimize(Dir) || currentChoiceValue >= values[state]) {
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
        STORM_LOG_ASSERT(!stateValue.empty(), "Bellman operator failed to compute value for state " << state << " since the row group is empty.");
        if (*stateValue < values[state]) {
            double const approxDiff = storm::utility::convertNumber<double, ValueType>(values[state] - *stateValue);
            STORM_LOG_WARN("Certificate invalid because lower bound is not inductive. At state " << state << " the Bellman operator yields " << *stateValue
                                                                                                 << " but the certificate has larger value " << values[state]
                                                                                                 << ". Approx. diff is " << approxDiff << ".");
            return false;
        }
        if (stateRankMinusOne.empty()) {
            STORM_LOG_WARN("Certificate invalid because there is no valid (i.e. inductive) action at state " << state << ", where the certificate has rank "
                                                                                                             << ranks[state] << ".");
            return false;
        }
        STORM_LOG_ASSERT(!stateRankMinusOne.empty(), "Ranking operator failed to compute rank for state " << state << ".");
        RankingType const stateRank = *stateRankMinusOne == InfRank ? InfRank : *stateRankMinusOne + 1;  // avoid integer overflow!
        if (stateRank > ranks[state]) {
            STORM_LOG_WARN("Certificate invalid because ranks for lower bound is not inductive. At state "
                           << state << " the rank operator yields " << stateRank << " but the certificate has smaller rank " << ranks[state] << ".");
            return false;
        }
    }
    return true;
}

template<typename ValueType>
bool ReachabilityProbabilityCertificate<ValueType>::checkValidity(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix) const {
    if (!dir.has_value() && !transitionProbabilityMatrix.hasTrivialRowGrouping()) {
        STORM_LOG_WARN("Certificate invalid because Matrix has non-trivial row grouping but no optimization direction given.");
        return false;
    }
    if (transitionProbabilityMatrix.getRowGroupCount() != targetStates.size() || transitionProbabilityMatrix.getColumnCount() != targetStates.size()) {
        STORM_LOG_WARN("Certificate invalid because Matrix has invalid dimensions.");
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
bool ReachabilityProbabilityCertificate<ValueType>::checkValidityInternal(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix) const {
    storm::OptionalRef<storm::storage::BitVector const> constraintStatesRef;
    if (hasConstraintStates()) {
        constraintStatesRef.reset(*constraintStates);
    }
    if (hasUpperBoundsCertificate() &&
        !checkUpperBoundCertificate<ValueType, Dir>(transitionProbabilityMatrix, targetStates, constraintStatesRef, upperBoundsCertificate.values)) {
        STORM_LOG_WARN("Certificate invalid because upper bound certificate is violated.");
        return false;
    }
    if (hasLowerBoundsCertificate() && !checkLowerBoundCertificate<ValueType, Dir>(transitionProbabilityMatrix, targetStates, constraintStatesRef,
                                                                                   lowerBoundsCertificate.values, lowerBoundsCertificate.ranks)) {
        STORM_LOG_WARN("Certificate invalid because lower bound certificate is violated.");
        return false;
    }
    return true;
}

template<typename ValueType>
storm::json<ValueType> ReachabilityProbabilityCertificate<ValueType>::toJson() const {
    storm::json<ValueType> result;
    result["kind"] = "reachability-probability";
    if (dir.has_value()) {
        result["direction"] = storm::solver::minimize(*dir) ? "min" : "max";
    }
    result["target"] = detail::stateSetToJson<ValueType>(targetLabel, targetStates);
    if (hasConstraintStates()) {
        result["constraint"] = detail::stateSetToJson<ValueType>(constraintLabel, *constraintStates);
    }
    if (hasLowerBoundsCertificate()) {
        result["lower"]["values"] = detail::valuesToJson<ValueType, ValueType>(lowerBoundsCertificate.values);
        result["lower"]["ranks"] = detail::ranksToJson<ValueType>(lowerBoundsCertificate.ranks);
    }
    if (hasUpperBoundsCertificate()) {
        result["upper"]["values"] = detail::valuesToJson<ValueType, ValueType>(upperBoundsCertificate.values);
    }
    return result;
}

template<typename ValueType>
std::string ReachabilityProbabilityCertificate<ValueType>::summaryString(storm::storage::BitVector const& relevantStates) const {
    std::stringstream ss;
    auto const numStates = relevantStates.getNumberOfSetBits();
    STORM_LOG_THROW(numStates > 0, storm::exceptions::InvalidOperationException,
                    "Tried to summarize reachability probability certificate but no state is relevant.");

    auto getLowerUpperBound = [this](uint64_t state) -> std::pair<ValueType, ValueType> {
        return {hasLowerBoundsCertificate() ? lowerBoundsCertificate.values[state] : storm::utility::zero<ValueType>(),
                hasUpperBoundsCertificate() ? upperBoundsCertificate.values[state] : storm::utility::one<ValueType>()};
    };
    auto const [lowerBound, upperBound] = getLowerUpperBound(relevantStates.getNextSetIndex(0));
    ss << "Certified reachability probability: [" << lowerBound << ", " << upperBound << "]";
    if (storm::NumberTraits<ValueType>::IsExact) {
        ss << " (approx. [" << storm::utility::convertNumber<double>(lowerBound) << ", " << storm::utility::convertNumber<double>(upperBound) << "])";
    }
    if (numStates > 1) {
        ss << " (only showing first of " << numStates << " relevant states)";
    }
    storm::utility::Maximum<ValueType> maxAbsDiff, maxRelDiff;
    for (auto const state : relevantStates) {
        auto const [lower, upper] = getLowerUpperBound(state);
        ValueType absDiff = upper - lower;
        ValueType relDiff;
        if (storm::utility::isZero(lower)) {
            relDiff = storm::utility::isZero(upper) ? storm::utility::zero<ValueType>() : storm::utility::one<ValueType>();
        } else {
            relDiff = absDiff / lower;
        }
        maxAbsDiff &= absDiff;
        maxRelDiff &= relDiff;
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
    printIntervalWidth(*maxAbsDiff, "Absolute");
    printIntervalWidth(*maxRelDiff, "Relative");
    return ss.str();
}

template<typename ValueType>
std::unique_ptr<Certificate<ValueType>> ReachabilityProbabilityCertificate<ValueType>::clone() const {
    auto cloned = hasConstraintStates()
                      ? std::make_unique<ReachabilityProbabilityCertificate<ValueType>>(dir, targetStates, *constraintStates, targetLabel, constraintLabel)
                      : std::make_unique<ReachabilityProbabilityCertificate<ValueType>>(dir, targetStates, targetLabel);
    if (hasLowerBoundsCertificate()) {
        auto v = lowerBoundsCertificate.values;
        auto r = lowerBoundsCertificate.ranks;
        cloned->setLowerBoundsCertificate(std::move(v), std::move(r));
    }
    if (hasUpperBoundsCertificate()) {
        auto v = upperBoundsCertificate.values;
        cloned->setUpperBoundsCertificate(std::move(v));
    }
    return cloned;
}

template<typename ValueType>
void ReachabilityProbabilityCertificate<ValueType>::setLowerBoundsCertificate(std::vector<ValueType>&& values, std::vector<RankingType>&& ranks) {
    STORM_LOG_ASSERT(values.size() == targetStates.size(), "Values for lower bound certificate have incorrect dimension.");
    STORM_LOG_ASSERT(ranks.size() == targetStates.size(), "Ranks for lower bound certificate have incorrect dimension.");
    lowerBoundsCertificate.values = std::move(values);
    lowerBoundsCertificate.ranks = std::move(ranks);
}

template<typename ValueType>
void ReachabilityProbabilityCertificate<ValueType>::setUpperBoundsCertificate(std::vector<ValueType>&& values) {
    STORM_LOG_ASSERT(values.size() == targetStates.size(), "Values for upper bound certificate have incorrect dimension.");
    upperBoundsCertificate.values = std::move(values);
}

template<typename ValueType>
bool ReachabilityProbabilityCertificate<ValueType>::hasLowerBoundsCertificate() const {
    return !lowerBoundsCertificate.values.empty();
}

template<typename ValueType>
bool ReachabilityProbabilityCertificate<ValueType>::hasUpperBoundsCertificate() const {
    return !upperBoundsCertificate.values.empty();
}

template<typename ValueType>
bool ReachabilityProbabilityCertificate<ValueType>::hasConstraintStates() const {
    return constraintStates.has_value();
}

template class ReachabilityProbabilityCertificate<double>;
template class ReachabilityProbabilityCertificate<storm::RationalNumber>;

}  // namespace storm::modelchecker