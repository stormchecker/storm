#include "storm/transformer/zeroWeight/GlobalZeroWeightActionTransformer.h"

#include <algorithm>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <type_traits>
#include <utility>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/geometry/NativePolytope.h"
#include "storm/transformer/zeroWeight/MixedWeightStateSplitter.h"
#include "storm/transformer/zeroWeight/ZeroWeightAnalysis.h"
#include "storm/transformer/zeroWeight/ZeroWeightComponentLp.h"
#include "storm/utility/ConstantsComparator.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::transformer {

namespace {

/*!
 * Grows a hull from feasible exit laws and checks each facet with an LP support query.
 * Reconstructs floating-point sample vertices with exact geometry to avoid rounding in the halfspaces.
 * Based on inner-approximation method: https://doi.org/10.48550/arXiv.1808.01786.
 */
template<typename ValueType>
std::vector<std::vector<ValueType>> enumerateExitLaws(ZeroWeightComponentLp<ValueType>& componentLp, uint64_t entryState) {
    uint64_t const boundarySize = componentLp.getBoundaryStates().size();
    auto const zero = storm::utility::zero<ValueType>();
    auto const one = storm::utility::one<ValueType>();
    ValueType precision = zero;
    if constexpr (std::is_same_v<ValueType, double>) {
        precision = 1e-9;
    }
    storm::utility::ConstantsComparator<ValueType> comparator(precision);
    std::vector<ValueType> direction(boundarySize, zero);
    if (boundarySize == 1) {
        return {componentLp.maximize(entryState, direction).boundaryProbabilities};
    }

    uint64_t const dimension = boundarySize - 1;
    std::vector<std::vector<ValueType>> samples;
    std::vector<std::vector<ValueType>> projectedPoints;
    auto addSample = [&](std::vector<ValueType> const& distribution) {
        std::vector<ValueType> point(distribution.begin(), distribution.begin() + dimension);
        for (auto const& existing : projectedPoints) {
            bool equal = true;
            for (uint64_t coordinate = 0; coordinate < dimension; ++coordinate) {
                equal &= comparator.isEqual(existing[coordinate], point[coordinate]);
            }
            if (equal) {
                return false;
            }
        }
        projectedPoints.push_back(std::move(point));
        samples.push_back(distribution);
        return true;
    };

    for (uint64_t coordinate = 0; coordinate < dimension; ++coordinate) {
        direction[coordinate] = one;
        addSample(componentLp.maximize(entryState, direction).boundaryProbabilities);
        direction[coordinate] = -one;
        addSample(componentLp.maximize(entryState, direction).boundaryProbabilities);
        direction[coordinate] = zero;
    }

    while (true) {
        storm::storage::geometry::NativePolytope<ValueType> hull(projectedPoints);
        bool expanded = false;
        for (auto const& halfspace : hull.getHalfspaces()) {
            ValueType scale = zero;
            for (auto const& coefficient : halfspace.normalVector()) {
                ValueType const magnitude = coefficient < zero ? -coefficient : coefficient;
                if (scale < magnitude) {
                    scale = magnitude;
                }
            }
            STORM_LOG_THROW(!storm::utility::isZero(scale), storm::exceptions::UnexpectedException, "The exit-law hull has a zero normal vector.");
            for (uint64_t coordinate = 0; coordinate < dimension; ++coordinate) {
                direction[coordinate] = halfspace.normalVector()[coordinate] / scale;
            }
            auto const optimum = componentLp.maximize(entryState, direction);
            if (comparator.isLess(halfspace.offset() / scale, optimum.objectiveValue)) {
                STORM_LOG_THROW(addSample(optimum.boundaryProbabilities), storm::exceptions::UnexpectedException,
                                "A violated exit-law facet produced an existing point.");
                expanded = true;
                break;
            }
        }
        if (expanded) {
            continue;
        }

        std::vector<std::vector<ValueType>> projectedVertices;
        if constexpr (std::is_same_v<ValueType, double>) {
            std::vector<std::vector<storm::RationalNumber>> exactPoints;
            exactPoints.reserve(projectedPoints.size());
            for (auto const& point : projectedPoints) {
                std::vector<storm::RationalNumber> exactPoint;
                exactPoint.reserve(point.size());
                for (auto const& coordinate : point) {
                    exactPoint.push_back(storm::utility::convertNumber<storm::RationalNumber>(coordinate));
                }
                exactPoints.push_back(std::move(exactPoint));
            }
            storm::storage::geometry::NativePolytope<storm::RationalNumber> exactHull(exactPoints);
            for (auto const& vertex : exactHull.getVertices()) {
                std::vector<ValueType> point;
                point.reserve(vertex.size());
                for (auto const& coordinate : vertex) {
                    point.push_back(storm::utility::convertNumber<ValueType>(coordinate));
                }
                projectedVertices.push_back(std::move(point));
            }
        } else {
            projectedVertices = hull.getVertices();
        }
        std::vector<std::vector<ValueType>> vertices;
        for (auto const& vertex : projectedVertices) {
            bool found = false;
            for (uint64_t index = 0; index < projectedPoints.size(); ++index) {
                bool equal = true;
                for (uint64_t coordinate = 0; coordinate < dimension; ++coordinate) {
                    equal &= comparator.isEqual(vertex[coordinate], projectedPoints[index][coordinate]);
                }
                if (equal) {
                    vertices.push_back(samples[index]);
                    found = true;
                    break;
                }
            }
            STORM_LOG_THROW(found, storm::exceptions::UnexpectedException, "An exit-law vertex has no LP witness.");
        }
        std::sort(vertices.begin(), vertices.end());
        vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
        return vertices;
    }
}

template<typename ValueType>
struct EntrySummary {
    uint64_t component;
    std::vector<std::vector<ValueType>> laws;
};

template<typename ValueType>
struct ComposedRow {
    uint64_t splitRow;
    std::map<uint64_t, ValueType> transitions;
    std::vector<typename GlobalZeroWeightActionTransformer<ValueType>::EntryLawSelection> selections;
};

}  // namespace

template<typename ValueType>
typename GlobalZeroWeightActionTransformer<ValueType>::Result GlobalZeroWeightActionTransformer<ValueType>::collapse(
    storm::Environment const& environment, storm::storage::SparseMatrix<ValueType> const& transitionMatrix, std::vector<ValueType> const& actionWeights,
    storm::storage::BitVector const& targetStates, storm::storage::BitVector const& initialStates) {
    uint64_t const invalidIndex = std::numeric_limits<uint64_t>::max();
    auto split = MixedWeightStateSplitter<ValueType>::split(transitionMatrix, actionWeights, targetStates, initialStates);
    ZeroWeightAnalysis<ValueType>::analyzeComponentInterfaces(split.transitionMatrix, split.analysis);
    uint64_t const numberOfSplitStates = split.transitionMatrix.getRowGroupCount();
    auto const& rowGroupIndices = split.transitionMatrix.getRowGroupIndices();

    storm::storage::BitVector removedStates(numberOfSplitStates, false);
    for (auto const& component : split.analysis.weakComponents) {
        for (uint64_t state : component.states) {
            removedStates.set(state);
        }
    }

    std::vector<std::vector<uint64_t>> requiredEntries(split.analysis.weakComponents.size());
    for (uint64_t component = 0; component < requiredEntries.size(); ++component) {
        requiredEntries[component] = split.analysis.weakComponents[component].entryStates;
    }
    for (uint64_t state = 0; state < numberOfSplitStates; ++state) {
        if (removedStates.get(state)) {
            continue;
        }
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            for (auto const& transition : split.transitionMatrix.getRow(row)) {
                if (storm::utility::isPositive(transition.getValue()) && removedStates.get(transition.getColumn())) {
                    uint64_t const component = split.analysis.stateToWeakComponent[transition.getColumn()];
                    STORM_LOG_ASSERT(component < requiredEntries.size(), "Invalid entry component.");
                    requiredEntries[component].push_back(transition.getColumn());
                }
            }
        }
    }

    std::vector<std::optional<EntrySummary<ValueType>>> summaries(numberOfSplitStates);
    for (uint64_t component = 0; component < requiredEntries.size(); ++component) {
        auto& entries = requiredEntries[component];
        std::sort(entries.begin(), entries.end());
        entries.erase(std::unique(entries.begin(), entries.end()), entries.end());
        auto lpComponent = split.analysis.weakComponents[component];
        lpComponent.entryStates = entries;
        ZeroWeightComponentLp<ValueType> componentLp(environment, split.transitionMatrix, split.actionWeights, lpComponent);
        for (uint64_t entry : entries) {
            summaries[entry].emplace(EntrySummary<ValueType>{component, enumerateExitLaws(componentLp, entry)});
        }
    }

    std::vector<std::vector<ComposedRow<ValueType>>> rowsByState(numberOfSplitStates);
    for (uint64_t state = 0; state < numberOfSplitStates; ++state) {
        if (removedStates.get(state)) {
            continue;
        }
        for (uint64_t row = rowGroupIndices[state]; row < rowGroupIndices[state + 1]; ++row) {
            ComposedRow<ValueType> base{row, {}, {}};
            std::map<uint64_t, ValueType> entryMass;
            for (auto const& transition : split.transitionMatrix.getRow(row)) {
                if (!storm::utility::isPositive(transition.getValue())) {
                    continue;
                }
                uint64_t const successor = transition.getColumn();
                auto& destination = removedStates.get(successor) ? entryMass[successor] : base.transitions[successor];
                destination = (ValueType)(destination + transition.getValue());
            }

            std::vector<ComposedRow<ValueType>> variants{std::move(base)};
            for (auto const& [entry, mass] : entryMass) {
                auto const& summary = summaries[entry];
                STORM_LOG_ASSERT(summary.has_value(), "A removed state has no entry summary.");
                auto const& boundary = split.analysis.weakComponents[summary->component].boundaryStates;
                std::vector<ComposedRow<ValueType>> expanded;
                for (auto const& variant : variants) {
                    for (uint64_t vertex = 0; vertex < summary->laws.size(); ++vertex) {
                        ComposedRow<ValueType> replacement = variant;
                        auto const& law = summary->laws[vertex];
                        STORM_LOG_ASSERT(law.size() == boundary.size(), "The exit law does not match its boundary.");
                        for (uint64_t coordinate = 0; coordinate < boundary.size(); ++coordinate) {
                            ValueType const probability = (ValueType)(mass * law[coordinate]);
                            if (storm::utility::isPositive(probability)) {
                                auto& destination = replacement.transitions[boundary[coordinate]];
                                destination = (ValueType)(destination + probability);
                            }
                        }
                        replacement.selections.push_back({split.newToOldStateMapping[entry], vertex});
                        expanded.push_back(std::move(replacement));
                    }
                }
                variants = std::move(expanded);
            }
            auto& stateRows = rowsByState[state];
            stateRows.insert(stateRows.end(), std::make_move_iterator(variants.begin()), std::make_move_iterator(variants.end()));
        }
    }

    Result result;
    std::vector<uint64_t> splitToNewState(numberOfSplitStates, invalidIndex);
    for (uint64_t state = 0; state < numberOfSplitStates; ++state) {
        if (!removedStates.get(state)) {
            splitToNewState[state] = result.newToOldStateMapping.size();
            result.newToOldStateMapping.push_back(split.newToOldStateMapping[state]);
        }
    }
    result.oldToNewStateMapping.assign(transitionMatrix.getRowGroupCount(), invalidIndex);
    for (uint64_t state = 0; state < result.oldToNewStateMapping.size(); ++state) {
        uint64_t splitState = state;
        if (removedStates.get(splitState) && split.positiveShells[state] != invalidIndex) {
            splitState = split.positiveShells[state];
        }
        result.oldToNewStateMapping[state] = splitToNewState[splitState];
    }
    for (uint64_t entry = 0; entry < summaries.size(); ++entry) {
        if (!summaries[entry].has_value()) {
            continue;
        }
        auto& summary = *summaries[entry];
        auto const& boundary = split.analysis.weakComponents[summary.component].boundaryStates;
        typename GlobalZeroWeightActionTransformer<ValueType>::EntryExitLaws exitLaws;
        exitLaws.entryState = split.newToOldStateMapping[entry];
        exitLaws.boundaryStates.reserve(boundary.size());
        for (uint64_t state : boundary) {
            STORM_LOG_ASSERT(!removedStates.get(state), "An entry boundary state was removed.");
            exitLaws.boundaryStates.push_back(splitToNewState[state]);
        }
        exitLaws.laws = std::move(summary.laws);
        result.entryExitLaws.push_back(std::move(exitLaws));
    }

    uint64_t numberOfRows = 0;
    uint64_t numberOfEntries = 0;
    for (uint64_t state = 0; state < numberOfSplitStates; ++state) {
        if (removedStates.get(state)) {
            continue;
        }
        for (auto const& row : rowsByState[state]) {
            ++numberOfRows;
            numberOfEntries += row.transitions.size();
        }
    }
    uint64_t const newNumberOfStates = result.newToOldStateMapping.size();
    storm::storage::SparseMatrixBuilder<ValueType> builder(numberOfRows, newNumberOfStates, numberOfEntries, true, true, newNumberOfStates);
    result.actionWeights.reserve(numberOfRows);
    result.oldToNewRowMapping.resize(transitionMatrix.getRowCount());
    result.newToOldRowMapping.reserve(numberOfRows);
    result.entryLawSelections.reserve(numberOfRows);
    result.targetStates = storm::storage::BitVector(newNumberOfStates, false);
    result.initialStates = storm::storage::BitVector(newNumberOfStates, false);
    uint64_t newRow = 0;
    for (uint64_t state = 0; state < numberOfSplitStates; ++state) {
        if (removedStates.get(state)) {
            continue;
        }
        builder.newRowGroup(newRow);
        uint64_t const newState = splitToNewState[state];
        if (split.targetStates.get(state)) {
            result.targetStates.set(newState);
        }
        if (split.initialStates.get(state)) {
            result.initialStates.set(newState);
        }
        for (auto const& row : rowsByState[state]) {
            for (auto const& [successor, probability] : row.transitions) {
                STORM_LOG_ASSERT(!removedStates.get(successor), "A collapsed row still reaches a removed state.");
                builder.addNextValue(newRow, splitToNewState[successor], probability);
            }
            uint64_t const oldRow = split.newToOldRowMapping[row.splitRow];
            STORM_LOG_ASSERT(oldRow != invalidIndex, "A retained row has no input origin.");
            result.actionWeights.push_back(split.actionWeights[row.splitRow]);
            result.oldToNewRowMapping[oldRow].push_back(newRow);
            result.newToOldRowMapping.push_back(oldRow);
            result.entryLawSelections.push_back(row.selections);
            ++newRow;
        }
    }
    result.transitionMatrix = builder.build();
    return result;
}

template class GlobalZeroWeightActionTransformer<double>;
template class GlobalZeroWeightActionTransformer<storm::RationalNumber>;

}  // namespace storm::transformer
