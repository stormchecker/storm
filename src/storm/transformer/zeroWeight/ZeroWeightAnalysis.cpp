#include "storm/transformer/zeroWeight/ZeroWeightAnalysis.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::transformer {

template<typename ValueType>
typename ZeroWeightAnalysis<ValueType>::Result ZeroWeightAnalysis<ValueType>::analyze(storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                                                                      std::vector<ValueType> const& actionWeights,
                                                                                      storm::storage::BitVector const& targetStates) {
    STORM_LOG_THROW(actionWeights.size() == transitionMatrix.getRowCount(), storm::exceptions::InvalidArgumentException,
                    "Expected the action weight count to match the transition matrix row count (received " << actionWeights.size() << ", expected "
                                                                                                           << transitionMatrix.getRowCount() << ").");
    STORM_LOG_THROW(targetStates.size() == transitionMatrix.getRowGroupCount(), storm::exceptions::InvalidArgumentException,
                    "Expected the target-state bit count to match the transition matrix row-group count (received "
                        << targetStates.size() << ", expected " << transitionMatrix.getRowGroupCount() << ").");

    for (auto const& actionWeight : actionWeights) {
        STORM_LOG_THROW(storm::utility::isNonNegative(actionWeight), storm::exceptions::InvalidArgumentException,
                        "Zero-weight action analysis requires nonnegative action weights.");
    }

    Result result;
    result.zeroWeightChoices = storm::storage::BitVector(transitionMatrix.getRowCount(), false);
    result.statesWithZeroWeightChoices = storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false);
    result.pureZeroWeightStates = storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false);
    result.mixedZeroWeightStates = storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false);
    result.positiveOnlyStates = storm::storage::BitVector(transitionMatrix.getRowGroupCount(), false);

    auto const& rowGroupIndices = transitionMatrix.getRowGroupIndices();
    for (uint64_t state = 0; state < transitionMatrix.getRowGroupCount(); ++state) {
        if (targetStates.get(state)) {
            continue;
        }

        bool hasZeroWeightChoice = false;
        bool hasPositiveWeightChoice = false;
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            if (storm::utility::isZero(actionWeights[row])) {
                result.zeroWeightChoices.set(row);
                hasZeroWeightChoice = true;
            } else {
                hasPositiveWeightChoice = true;
            }
        }

        if (hasZeroWeightChoice) {
            result.statesWithZeroWeightChoices.set(state);
            if (hasPositiveWeightChoice) {
                result.mixedZeroWeightStates.set(state);
            } else {
                result.pureZeroWeightStates.set(state);
            }
        } else if (hasPositiveWeightChoice) {
            result.positiveOnlyStates.set(state);
        }
    }

    uint64_t const numberOfStates = transitionMatrix.getRowGroupCount();
    uint64_t const invalidComponent = std::numeric_limits<uint64_t>::max();
    result.stateToWeakComponent.assign(numberOfStates, invalidComponent);
    if (result.statesWithZeroWeightChoices.empty()) {
        return result;
    }

    std::vector<uint64_t> parents(numberOfStates);
    std::iota(parents.begin(), parents.end(), uint64_t{0});
    std::vector<uint64_t> componentSizes(numberOfStates, 1);

    auto findRoot = [&parents](uint64_t state) {
        while (parents[state] != state) {
            parents[state] = parents[parents[state]];
            state = parents[state];
        }
        return state;
    };
    auto unite = [&componentSizes, &findRoot, &parents](uint64_t firstState, uint64_t secondState) {
        uint64_t firstRoot = findRoot(firstState);
        uint64_t secondRoot = findRoot(secondState);
        if (firstRoot == secondRoot) {
            return;
        }
        if (componentSizes[firstRoot] < componentSizes[secondRoot]) {
            std::swap(firstRoot, secondRoot);
        }
        parents[secondRoot] = firstRoot;
        componentSizes[firstRoot] += componentSizes[secondRoot];
    };

    for (uint64_t state : result.statesWithZeroWeightChoices) {
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            if (!result.zeroWeightChoices.get(row)) {
                continue;
            }
            for (auto const& entry : transitionMatrix.getRow(row)) {
                if (storm::utility::isPositive(entry.getValue()) && result.statesWithZeroWeightChoices.get(entry.getColumn())) {
                    unite(state, entry.getColumn());
                }
            }
        }
    }

    std::vector<uint64_t> rootToComponent(numberOfStates, invalidComponent);
    for (uint64_t state : result.statesWithZeroWeightChoices) {
        uint64_t const root = findRoot(state);
        if (rootToComponent[root] == invalidComponent) {
            rootToComponent[root] = result.weakComponents.size();
            result.weakComponents.emplace_back();
        }
        uint64_t const component = rootToComponent[root];
        result.stateToWeakComponent[state] = component;
        result.weakComponents[component].states.push_back(state);
    }

    return result;
}

template<typename ValueType>
void ZeroWeightAnalysis<ValueType>::analyzeComponentInterfaces(storm::storage::SparseMatrix<ValueType> const& transitionMatrix, Result& analysis) {
    uint64_t const numberOfStates = transitionMatrix.getRowGroupCount();
    STORM_LOG_THROW(transitionMatrix.getColumnCount() == numberOfStates && analysis.stateToWeakComponent.size() == numberOfStates &&
                        analysis.positiveOnlyStates.size() == numberOfStates && analysis.mixedZeroWeightStates.size() == numberOfStates,
                    storm::exceptions::InvalidArgumentException, "Expected matching transition matrix and action analysis.");
    STORM_LOG_THROW(analysis.mixedZeroWeightStates.empty(), storm::exceptions::InvalidArgumentException,
                    "Split mixed states before analyzing component interfaces.");

    auto& components = analysis.weakComponents;
    if (components.empty()) {
        return;
    }
    for (auto& component : components) {
        component.entryStates.clear();
        component.positivePredecessorRows.clear();
        component.boundaryStates.clear();
    }

    uint64_t const invalidIndex = std::numeric_limits<uint64_t>::max();
    auto const& rowGroupIndices = transitionMatrix.getRowGroupIndices();
    storm::storage::BitVector entryStates(numberOfStates, false);
    std::vector<uint64_t> lastPredecessorRow(components.size(), invalidIndex);
    for (uint64_t state : analysis.positiveOnlyStates) {
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            for (auto const& entry : transitionMatrix.getRow(row)) {
                if (!storm::utility::isPositive(entry.getValue())) {
                    continue;
                }
                uint64_t const component = analysis.stateToWeakComponent[entry.getColumn()];
                if (component != invalidIndex) {
                    STORM_LOG_ASSERT(component < components.size(), "Invalid zero-weight component index.");
                    entryStates.set(entry.getColumn());
                    if (lastPredecessorRow[component] != row) {
                        components[component].positivePredecessorRows.push_back(row);
                        lastPredecessorRow[component] = row;
                    }
                }
            }
        }
    }
    for (uint64_t state : entryStates) {
        components[analysis.stateToWeakComponent[state]].entryStates.push_back(state);
    }

    std::vector<uint64_t> lastBoundaryComponent(numberOfStates, invalidIndex);
    for (uint64_t component = 0; component < components.size(); ++component) {
        for (uint64_t state : components[component].states) {
            STORM_LOG_ASSERT(state < numberOfStates && analysis.stateToWeakComponent[state] == component, "Invalid zero-weight component membership.");
            for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
                for (auto const& entry : transitionMatrix.getRow(row)) {
                    if (!storm::utility::isPositive(entry.getValue())) {
                        continue;
                    }
                    uint64_t const successor = entry.getColumn();
                    uint64_t const successorComponent = analysis.stateToWeakComponent[successor];
                    STORM_LOG_ASSERT(successorComponent == invalidIndex || successorComponent == component,
                                     "Zero-weight transitions must not connect different weak components.");
                    if (successorComponent == invalidIndex && lastBoundaryComponent[successor] != component) {
                        components[component].boundaryStates.push_back(successor);
                        lastBoundaryComponent[successor] = component;
                    }
                }
            }
        }
        std::sort(components[component].boundaryStates.begin(), components[component].boundaryStates.end());
    }
}

template class ZeroWeightAnalysis<double>;
template class ZeroWeightAnalysis<storm::RationalNumber>;

}  // namespace storm::transformer
