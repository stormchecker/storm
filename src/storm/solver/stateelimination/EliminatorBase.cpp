#include "storm/solver/stateelimination/EliminatorBase.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidStateException.h"
#include "storm/solver/stateelimination/StateEliminationUtility.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace solver {
namespace stateelimination {

template<typename ValueType, ScalingMode Mode>
EliminatorBase<ValueType, Mode>::EliminatorBase(storm::storage::FlexibleSparseMatrix<ValueType>& matrix,
                                                storm::storage::FlexibleSparseMatrix<ValueType>& transposedMatrix)
    : matrix(matrix), transposedMatrix(transposedMatrix) {
    // Intentionally left empty.
}

template<typename ValueType, ScalingMode Mode>
void EliminatorBase<ValueType, Mode>::eliminate(uint64_t row, uint64_t column, bool clearRow) {
    using MatrixEntry = storm::storage::MatrixEntry<typename storm::storage::FlexibleSparseMatrix<ValueType>::index_type,
                                                    typename storm::storage::FlexibleSparseMatrix<ValueType>::value_type>;

    // Start by finding the entry in the given column.
    bool hasEntryInColumn = false;
    ValueType columnValue = storm::utility::zero<ValueType>();
    FlexibleRowType& entriesInRow = matrix.getRow(row);
    for (auto entryIt = entriesInRow.begin(), entryIte = entriesInRow.end(); entryIt != entryIte; ++entryIt) {
        if (entryIt->getColumn() >= column) {
            if (entryIt->getColumn() == column) {
                columnValue = entryIt->getValue();
                hasEntryInColumn = true;

                // If we do not clear the row completely, we need to remove the entry in the requested column.
                // All other elements are scaled with the entry anyway.
                if (!clearRow) {
                    entriesInRow.erase(entryIt);
                }
            }
            break;
        }
    }

    // Scale all entries in this row.
    // Depending on the scaling mode, we scale the other entries of the row.
    STORM_LOG_TRACE((hasEntryInColumn ? "State has entry in column." : "State does not have entry in column."));
    if (Mode == ScalingMode::Divide) {
        STORM_LOG_ASSERT(hasEntryInColumn, "The scaling mode 'divide' requires an element in the given column.");
        STORM_LOG_ASSERT(storm::utility::isZero(columnValue), "The scaling mode 'divide' requires a non-zero element in the given column.");
        columnValue = storm::utility::one<ValueType>() / columnValue;
    } else if (Mode == ScalingMode::DivideOneMinus) {
        if (hasEntryInColumn) {
            STORM_LOG_ASSERT(columnValue != storm::utility::one<ValueType>(),
                             "The scaling mode 'divide-one-minus' requires a non-one value in the given column.");
            columnValue = storm::utility::one<ValueType>() / (storm::utility::one<ValueType>() - columnValue);
            columnValue = storm::utility::simplify(columnValue);
        }
    }

    if (hasEntryInColumn) {
        for (auto entryIt = entriesInRow.begin(), entryIte = entriesInRow.end(); entryIt != entryIte; ++entryIt) {
            // Only scale the entries in a different column.
            if (entryIt->getColumn() != column) {
                entryIt->setValue(storm::utility::simplify((ValueType)(entryIt->getValue() * columnValue)));
            }
        }
        updateValue(row, columnValue);
    }

    // Now substitute the row entries in all other rows that contain an element whose column is the current row.
    FlexibleRowType& elementsWithEntryInColumnEqualRow = transposedMatrix.getRow(column);

    // In case we have a constrained elimination, we need to keep track of the rows that keep their value
    // in the column equal to the current row.
    FlexibleRowType rowsKeepingEntryInColumnEqualRow;

    // For each entry in the row d, we need to build a list of other rows that will contain an element in the
    // column d.
    std::vector<FlexibleRowType> newBackwardEntries(entriesInRow.size());
    for (auto& backwardEntry : newBackwardEntries) {
        backwardEntry.reserve(elementsWithEntryInColumnEqualRow.size());
    }

    // Now go through the rows with an entry in the column corresponding to the current row and substitute
    // the elements of this row unless the elimination is filtered.
    for (auto const& predecessorEntry : elementsWithEntryInColumnEqualRow) {
        uint_fast64_t predecessor = predecessorEntry.getColumn();
        STORM_LOG_TRACE("Found predecessor " << predecessor << ".");

        // Skip the row itself.
        if (predecessor == row) {
            assert(hasEntryInColumn);
            continue;
        }

        // Skip the state if the elimination is constrained, but the predecessor is not in the constraint.
        if (isFilterPredecessor() && !filterPredecessor(predecessor)) {
            rowsKeepingEntryInColumnEqualRow.emplace_back(predecessorEntry);
            STORM_LOG_TRACE("Not eliminating predecessor " << predecessor << ", because it does not fit the filter.");
            continue;
        }
        STORM_LOG_TRACE("Eliminating predecessor " << predecessor << ".");

        // First, find the probability with which the predecessor can move to the current state, because
        // the forward probabilities of the state to be eliminated need to be scaled with this factor.
        FlexibleRowType& predecessorForwardTransitions = matrix.getRow(predecessor);
        FlexibleRowIterator multiplyElement = std::find_if(predecessorForwardTransitions.begin(), predecessorForwardTransitions.end(),
                                                           [&](MatrixEntry const& a) { return a.getColumn() == column; });

        // Make sure we have found the probability and set it to zero.
        STORM_LOG_THROW(multiplyElement != predecessorForwardTransitions.end(), storm::exceptions::InvalidStateException,
                        "No probability for successor found.");
        ValueType multiplyFactor = multiplyElement->getValue();
        multiplyElement->setValue(storm::utility::zero<ValueType>());

        // At this point, we need to update the (forward) transitions of the predecessor.
        FlexibleRowIterator first1 = predecessorForwardTransitions.begin();
        FlexibleRowIterator last1 = predecessorForwardTransitions.end();
        FlexibleRowIterator first2 = entriesInRow.begin();
        FlexibleRowIterator last2 = entriesInRow.end();

        FlexibleRowType newSuccessors;
        newSuccessors.reserve((last1 - first1) + (last2 - first2));
        std::insert_iterator<FlexibleRowType> result(newSuccessors, newSuccessors.end());

        uint_fast64_t successorOffsetInNewBackwardTransitions = 0;
        // Now we merge the two successor lists. (Code taken from std::set_union and modified to suit our needs).
        for (; first1 != last1; ++result) {
            // Skip the transitions to the state that is currently being eliminated.
            if (first1->getColumn() == column || (first2 != last2 && first2->getColumn() == column)) {
                if (first1->getColumn() == column) {
                    ++first1;
                }
                if (first2 != last2 && first2->getColumn() == column) {
                    ++first2;
                }
                continue;
            }

            if (first2 == last2) {
                std::copy_if(first1, last1, result, [&](MatrixEntry const& a) { return a.getColumn() != column; });
                break;
            }
            if (first2->getColumn() < first1->getColumn()) {
                ValueType successorValue = storm::utility::simplify<ValueType>((first2->getValue() * multiplyFactor));
                *result = MatrixEntry(first2->getColumn(), successorValue);
                newBackwardEntries[successorOffsetInNewBackwardTransitions].emplace_back(predecessor, successorValue);
                ++first2;
                ++successorOffsetInNewBackwardTransitions;
            } else if (first1->getColumn() < first2->getColumn()) {
                *result = *first1;
                ++first1;
            } else {
                ValueType sprod = multiplyFactor * first2->getValue();
                ValueType sum = first1->getValue() + storm::utility::simplify(sprod);
                auto probability = storm::utility::simplify(sum);
                *result = MatrixEntry(first1->getColumn(), probability);
                newBackwardEntries[successorOffsetInNewBackwardTransitions].emplace_back(predecessor, probability);
                ++first1;
                ++first2;
                ++successorOffsetInNewBackwardTransitions;
            }
        }
        for (; first2 != last2; ++first2) {
            if (first2->getColumn() != column) {
                ValueType probability = storm::utility::simplify<ValueType>(first2->getValue() * multiplyFactor);
                *result = MatrixEntry(first2->getColumn(), probability);
                newBackwardEntries[successorOffsetInNewBackwardTransitions].emplace_back(predecessor, probability);
                ++successorOffsetInNewBackwardTransitions;
            }
        }

        // Now move the new transitions in place.
        predecessorForwardTransitions = std::move(newSuccessors);
        STORM_LOG_TRACE("Fixed new next-state probabilities of predecessor state " << predecessor << ".");

        updatePredecessor(predecessor, multiplyFactor, row);

        STORM_LOG_TRACE("Updating priority of predecessor.");
        updatePriority(predecessor);
    }

    // Finally, we need to add the predecessor to the set of predecessors of every successor.
    uint_fast64_t successorOffsetInNewBackwardTransitions = 0;
    for (auto const& successorEntry : entriesInRow) {
        if (successorEntry.getColumn() == column) {
            continue;
        }

        FlexibleRowType& successorBackwardTransitions = transposedMatrix.getRow(successorEntry.getColumn());

        // Delete the current state as a predecessor of the successor state only if we are going to remove the
        // current state's forward transitions.
        if (clearRow) {
            FlexibleRowIterator elimIt = std::find_if(successorBackwardTransitions.begin(), successorBackwardTransitions.end(),
                                                      [&](MatrixEntry const& a) { return a.getColumn() == row; });
            STORM_LOG_ASSERT(elimIt != successorBackwardTransitions.end(),
                             "Expected a proper backward transition from " << successorEntry.getColumn() << " to " << column << ", but found none.");
            successorBackwardTransitions.erase(elimIt);
        }

        FlexibleRowIterator first1 = successorBackwardTransitions.begin();
        FlexibleRowIterator last1 = successorBackwardTransitions.end();
        FlexibleRowIterator first2 = newBackwardEntries[successorOffsetInNewBackwardTransitions].begin();
        FlexibleRowIterator last2 = newBackwardEntries[successorOffsetInNewBackwardTransitions].end();

        FlexibleRowType newPredecessors;
        newPredecessors.reserve((last1 - first1) + (last2 - first2));
        std::insert_iterator<FlexibleRowType> result(newPredecessors, newPredecessors.end());

        for (; first1 != last1; ++result) {
            if (first2 == last2) {
                std::copy(first1, last1, result);
                break;
            }
            if (first2->getColumn() < first1->getColumn()) {
                if (first2->getColumn() != row) {
                    *result = *first2;
                }
                ++first2;
            } else if (first1->getColumn() == first2->getColumn()) {
                if (estimateComplexity(first1->getValue()) > estimateComplexity(first2->getValue())) {
                    *result = *first1;
                } else {
                    *result = *first2;
                }
                ++first1;
                ++first2;
            } else {
                *result = *first1;
                ++first1;
            }
        }
        if (isFilterPredecessor()) {
            std::copy_if(first2, last2, result, [&](MatrixEntry const& a) { return a.getColumn() != row && filterPredecessor(a.getColumn()); });
        } else {
            std::copy_if(first2, last2, result, [&](MatrixEntry const& a) { return a.getColumn() != row; });
        }
        // Now move the new predecessors in place.
        successorBackwardTransitions = std::move(newPredecessors);
        ++successorOffsetInNewBackwardTransitions;
    }
    STORM_LOG_TRACE("Fixed predecessor lists of successor states.");

    // Clear the row if requested.
    if (clearRow) {
        entriesInRow.clear();
        entriesInRow.shrink_to_fit();
    }

    // If the substitution was filtered, we need to store the new rows that have an entry in column equal to this row.
    if (isFilterPredecessor()) {
        elementsWithEntryInColumnEqualRow = std::move(rowsKeepingEntryInColumnEqualRow);
    } else {
        elementsWithEntryInColumnEqualRow.clear();
        elementsWithEntryInColumnEqualRow.shrink_to_fit();
    }
}

template<typename ValueType, ScalingMode Mode>
void EliminatorBase<ValueType, Mode>::eliminateLoop(uint64_t state) {
    // Start by finding value of the selfloop.
    bool hasEntryInColumn = false;
    ValueType columnValue = storm::utility::zero<ValueType>();
    FlexibleRowType& entriesInRow = matrix.getRow(state);
    for (auto entryIt = entriesInRow.begin(), entryIte = entriesInRow.end(); entryIt != entryIte; ++entryIt) {
        if (entryIt->getColumn() == state) {
            columnValue = entryIt->getValue();
            hasEntryInColumn = true;
        }
    }

    // Scale all entries in this row.
    // Depending on the scaling mode, we scale the other entries of the row.
    STORM_LOG_TRACE((hasEntryInColumn ? "State has entry in column." : "State does not have entry in column."));
    if (Mode == ScalingMode::Divide) {
        STORM_LOG_ASSERT(hasEntryInColumn, "The scaling mode 'divide' requires an element in the given column.");
        STORM_LOG_ASSERT(storm::utility::isZero(columnValue), "The scaling mode 'divide' requires a non-zero element in the given column.");
        columnValue = storm::utility::one<ValueType>() / columnValue;
    } else if (Mode == ScalingMode::DivideOneMinus) {
        if (hasEntryInColumn) {
            STORM_LOG_ASSERT(columnValue != storm::utility::one<ValueType>(),
                             "The scaling mode 'divide-one-minus' requires a non-one value in the given column.");
            columnValue = storm::utility::one<ValueType>() / (storm::utility::one<ValueType>() - columnValue);
            columnValue = storm::utility::simplify(columnValue);
        }
    }

    if (hasEntryInColumn) {
        for (auto entryIt = entriesInRow.begin(), entryIte = entriesInRow.end(); entryIt != entryIte; ++entryIt) {
            // Scale the entries in a different column, set state transition probability to 0.
            if (entryIt->getColumn() != state) {
                entryIt->setValue(storm::utility::simplify((ValueType)(entryIt->getValue() * columnValue)));
            } else {
                entryIt->setValue(storm::utility::zero<ValueType>());
            }
        }
    }
}

template<typename ValueType, ScalingMode Mode>
void EliminatorBase<ValueType, Mode>::updateValue(storm::storage::sparse::state_type const&, ValueType const&) {
    // Intentionally left empty.
}

template<typename ValueType, ScalingMode Mode>
void EliminatorBase<ValueType, Mode>::updatePredecessor(storm::storage::sparse::state_type const&, ValueType const&,
                                                        storm::storage::sparse::state_type const&) {
    // Intentionally left empty.
}

template<typename ValueType, ScalingMode Mode>
void EliminatorBase<ValueType, Mode>::updatePriority(storm::storage::sparse::state_type const&) {
    // Intentionally left empty.
}

template<typename ValueType, ScalingMode Mode>
bool EliminatorBase<ValueType, Mode>::filterPredecessor(storm::storage::sparse::state_type const&) {
    STORM_LOG_ASSERT(false, "Must not filter predecessors.");
    return false;
}

template<typename ValueType, ScalingMode Mode>
bool EliminatorBase<ValueType, Mode>::isFilterPredecessor() const {
    return false;
}

template class EliminatorBase<double, ScalingMode::Divide>;
template class EliminatorBase<double, ScalingMode::DivideOneMinus>;

template class EliminatorBase<storm::RationalNumber, ScalingMode::Divide>;
template class EliminatorBase<storm::RationalFunction, ScalingMode::Divide>;

template class EliminatorBase<storm::RationalNumber, ScalingMode::DivideOneMinus>;
template class EliminatorBase<storm::RationalFunction, ScalingMode::DivideOneMinus>;
}  // namespace stateelimination
}  // namespace solver
}  // namespace storm
