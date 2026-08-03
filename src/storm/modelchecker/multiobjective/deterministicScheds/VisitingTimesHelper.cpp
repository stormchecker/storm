#include "storm/modelchecker/multiobjective/deterministicScheds/VisitingTimesHelper.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/constants.h"

namespace storm::modelchecker::multiobjective {

template<typename ValueType>
ValueType VisitingTimesHelper<ValueType>::computeMecTraversalLowerBound(storm::storage::MaximalEndComponent const& mec,
                                                                        storm::storage::SparseMatrix<ValueType> const& transitions,
                                                                        bool assumeOptimalTransitionProbabilities) {
    STORM_LOG_ASSERT(mec.size() > 0, "empty mec not expected");
    auto res = storm::utility::one<ValueType>();
    if (mec.size() == 1) {
        return res;
    }
    // Whenever s' is reachable from s, there is at least one finite path that visits each state of the mec at most once.
    // We compute a naive lower bound for the probability of such a single path using the product of the smallest probabilities occurring at each state
    // (Excluding self-loop probabilities)
    for (auto const& stateChoices : mec) {
        storm::utility::Minimum<ValueType> v;
        auto const& s = stateChoices.first;
        for (auto const& c : stateChoices.second) {
            auto row = transitions.getRow(c);
            if (assumeOptimalTransitionProbabilities) {
                // Count the number of non-selfloop entries and assume a uniform distribution (which gives us the largest minimal probability)
                auto numEntries = row.getNumberOfEntries();
                for (auto const& entry : row) {
                    if (entry.getColumn() > s) {
                        if (entry.getColumn() == s) {
                            --numEntries;
                        }
                        break;
                    }
                }
                if (numEntries > 0) {
                    v &= storm::utility::one<ValueType>() / storm::utility::convertNumber<ValueType>(numEntries);
                }
            } else {
                // actually determine the minimal probability
                for (auto const& entry : row) {
                    if (entry.getColumn() != s && !storm::utility::isZero(entry.getValue())) {
                        v &= entry.getValue();
                    }
                }
            }
        }
        STORM_LOG_ASSERT(!v.empty(), "self-loop state in non-singleton mec?");
        res *= *v;
    }
    return res;
}

template<typename ValueType>
ValueType VisitingTimesHelper<ValueType>::computeMecVisitsUpperBound(storm::storage::MaximalEndComponent const& mec,
                                                                     storm::storage::SparseMatrix<ValueType> const& transitions,
                                                                     bool assumeOptimalTransitionProbabilities) {
    auto const one = storm::utility::one<ValueType>();
    auto const traversalLowerBound = computeMecTraversalLowerBound(mec, transitions, assumeOptimalTransitionProbabilities);
    if (assumeOptimalTransitionProbabilities) {
        // We assume that the probability to go back to the MEC using an exiting choice is zero.
        return one / traversalLowerBound;
    } else {
        // compute the largest probability to go back to the MEC when using an exiting choice
        storm::utility::Maximum<ValueType> q(storm::utility::zero<ValueType>());
        for (auto const& stateChoices : mec) {
            for (auto c : transitions.getRowGroupIndices(stateChoices.first)) {
                if (stateChoices.second.contains(c)) {
                    continue;  // not an exit choice!
                }
                auto choiceValue = storm::utility::zero<ValueType>();
                for (auto const& entry : transitions.getRow(c)) {
                    if (mec.containsState(entry.getColumn())) {
                        choiceValue += entry.getValue();
                    }
                }
                q &= choiceValue;
            }
        }
        return one / (traversalLowerBound * (one - *q));
    }
}

template<typename ValueType>
std::vector<ValueType> VisitingTimesHelper<ValueType>::computeUpperBoundsOnExpectedVisitingTimes(
    storm::storage::BitVector const& subsystem, storm::storage::SparseMatrix<ValueType> const& transitions,
    storm::storage::SparseMatrix<ValueType> const& backwardTransitions) {
    storm::storage::MaximalEndComponentDecomposition<ValueType> mecs(transitions, backwardTransitions, subsystem);
    storm::storage::BitVector allStates(subsystem.size(), true);
    auto quotientData = storm::transformer::EndComponentEliminator<ValueType>::transform(transitions, mecs, subsystem, subsystem);
    auto notSubsystem = ~subsystem;

    // map collapsed states in the quotient to the value l that is to be multiplied to the transitions of those states
    std::map<uint64_t, ValueType> collapsedStateToValueMap;
    for (auto const& mec : mecs) {
        collapsedStateToValueMap.emplace(quotientData.oldToNewStateMapping.at(mec.begin()->first), computeMecTraversalLowerBound(mec, transitions));
    }

    // Create a modified quotient with scaled transitions
    storm::storage::SparseMatrixBuilder<ValueType> modifiedQuotientBuilder(quotientData.matrix.getRowCount(), quotientData.matrix.getColumnCount(), 0, true,
                                                                           true, quotientData.matrix.getRowGroupCount());
    std::vector<ValueType> toSinkProbabilities;
    toSinkProbabilities.reserve(quotientData.matrix.getRowGroupCount());
    for (uint64_t state = 0; state < quotientData.matrix.getRowGroupCount(); ++state) {
        modifiedQuotientBuilder.newRowGroup(quotientData.matrix.getRowGroupIndices()[state]);
        ValueType scalingFactor = storm::utility::one<ValueType>();
        if (auto findRes = collapsedStateToValueMap.find(state); findRes != collapsedStateToValueMap.end()) {
            scalingFactor = findRes->second;
        }
        for (auto const choice : quotientData.matrix.getRowGroupIndices(state)) {
            if (!storm::utility::isOne(scalingFactor)) {
                modifiedQuotientBuilder.addDiagonalEntry(choice, storm::utility::one<ValueType>() - scalingFactor);
            }
            for (auto const& entry : quotientData.matrix.getRow(choice)) {
                modifiedQuotientBuilder.addNextValue(choice, entry.getColumn(), scalingFactor * entry.getValue());
            }
            if (quotientData.sinkRows.get(choice)) {
                toSinkProbabilities.push_back(scalingFactor);
            } else {
                toSinkProbabilities.push_back(scalingFactor * transitions.getConstrainedRowSum(quotientData.newToOldRowMapping.at(choice), notSubsystem));
            }
        }
    }
    auto modifiedQuotient = modifiedQuotientBuilder.build();
    STORM_LOG_ASSERT(modifiedQuotient.getRowCount() == toSinkProbabilities.size(), "Unexpected row count");

    // compute upper visiting time bounds
    auto quotientUpperBounds =
        storm::modelchecker::helper::BaierUpperRewardBoundsComputer<ValueType>::computeUpperBoundOnExpectedVisitingTimes(modifiedQuotient, toSinkProbabilities);

    std::vector<ValueType> result;
    result.reserve(subsystem.size());
    for (uint64_t state = 0; state < subsystem.size(); ++state) {
        auto const& quotientState = quotientData.oldToNewStateMapping[state];
        if (quotientState < quotientData.matrix.getRowGroupCount()) {
            result.push_back(quotientUpperBounds.at(quotientState));
        } else {
            result.push_back(-storm::utility::one<ValueType>());  // not in given subsystem;
        }
    }
    return result;
}

template class VisitingTimesHelper<double>;
template class VisitingTimesHelper<storm::RationalNumber>;

}  // namespace storm::modelchecker::multiobjective