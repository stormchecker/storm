#include "storm-pars/transformer/ParameterLifter.h"

#include "storm-pars/storage/ParameterRegion.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace transformer {

template<typename ParametricType, typename ConstantType>
ParameterLifter<ParametricType, ConstantType>::ParameterLifter(storm::storage::SparseMatrix<ParametricType> const& pMatrix,
                                                               std::vector<ParametricType> const& pVector, storm::storage::BitVector const& selectedRows,
                                                               storm::storage::BitVector const& selectedColumns, bool generateRowLabels,
                                                               bool useMonotonicityInFuture) {
    // get a mapping from old column indices to new ones
    oldToNewColumnIndexMapping = std::vector<uint_fast64_t>(selectedColumns.size(), selectedColumns.size());
    uint_fast64_t newIndex = 0;
    for (auto oldColumn : selectedColumns) {
        oldToNewColumnIndexMapping[oldColumn] = newIndex++;
    }

    // create vector, such that the occuringVariables for all states can be stored
    occurringVariablesAtState = std::vector<std::set<VariableType>>(pMatrix.getColumnCount());

    // Stores which entries of the original matrix/vector are non-constant. Entries for non-selected rows/columns are omitted
    auto nonConstMatrixEntries = storm::storage::BitVector(pMatrix.getEntryCount(), false);  // this vector has to be resized later
    auto nonConstVectorEntries = storm::storage::BitVector(selectedRows.getNumberOfSetBits(), false);
    // Counters for selected entries in the pMatrix and the pVector
    uint_fast64_t pMatrixEntryCount = 0;
    uint_fast64_t pVectorEntryCount = 0;

    // The matrix builder for the new matrix. The correct number of rows and entries is not known yet.
    storm::storage::SparseMatrixBuilder<ConstantType> builder(0, selectedColumns.getNumberOfSetBits(), 0, true, true, selectedRows.getNumberOfSetBits());
    rowGroupToStateNumber = std::vector<uint_fast64_t>();
    uint_fast64_t newRowIndex = 0;
    for (auto rowIndex : selectedRows) {
        builder.newRowGroup(newRowIndex);
        rowGroupToStateNumber.push_back(rowIndex);

        // Gather the occurring variables within this row and set which entries are non-constant
        std::set<VariableType> occurringVariables;
        for (auto const& entry : pMatrix.getRow(rowIndex)) {
            if (selectedColumns.get(entry.getColumn())) {
                if (!storm::utility::isConstant(entry.getValue())) {
                    storm::utility::parametric::gatherOccurringVariables(entry.getValue(), occurringVariables);
                    nonConstMatrixEntries.set(pMatrixEntryCount, true);
                }
                ++pMatrixEntryCount;
            } else {
                if (!storm::utility::isConstant(entry.getValue())) {
                    storm::utility::parametric::gatherOccurringVariables(entry.getValue(), occurringVariables);
                }
            }
        }

        ParametricType const& pVectorEntry = pVector[rowIndex];
        std::set<VariableType> vectorEntryVariables;
        if (!storm::utility::isConstant(pVectorEntry)) {
            storm::utility::parametric::gatherOccurringVariables(pVectorEntry, vectorEntryVariables);
            if (generateRowLabels) {
                // If row labels are to be generated, we do not allow unspecified valuations.
                // Therefore, we also 'lift' parameters that only occurr on a vector.
                occurringVariables.insert(vectorEntryVariables.begin(), vectorEntryVariables.end());
            }
            nonConstVectorEntries.set(pVectorEntryCount, true);
        }
        ++pVectorEntryCount;
        // Compute the (abstract) valuation for each row
        auto rowValuations = getVerticesOfAbstractRegion(occurringVariables);
        for (auto const& val : rowValuations) {
            if (generateRowLabels) {
                rowLabels.push_back(val);
            }

            // Insert matrix entries for each valuation. For non-constant entries, a dummy value is inserted and the function and the valuation are collected.
            // The placeholder for the collected function/valuation are stored in the matrixAssignment. The matrixAssignment is completed after the matrix is
            // finished
            for (auto const& entry : pMatrix.getRow(rowIndex)) {
                if (selectedColumns.get(entry.getColumn())) {
                    if (storm::utility::isConstant(entry.getValue())) {
                        builder.addNextValue(newRowIndex, oldToNewColumnIndexMapping[entry.getColumn()],
                                             storm::utility::convertNumber<ConstantType>(entry.getValue()));
                    } else {
                        builder.addNextValue(newRowIndex, oldToNewColumnIndexMapping[entry.getColumn()], storm::utility::one<ConstantType>());
                        ConstantType& placeholder = functionValuationCollector.add(entry.getValue(), val);
                        matrixAssignment.push_back(std::pair<typename storm::storage::SparseMatrix<ConstantType>::iterator, ConstantType&>(
                            typename storm::storage::SparseMatrix<ConstantType>::iterator(), placeholder));
                    }
                }
            }

            // Insert the vector entry for this row
            if (storm::utility::isConstant(pVectorEntry)) {
                vector.push_back(storm::utility::convertNumber<ConstantType>(pVectorEntry));
            } else {
                vector.push_back(storm::utility::one<ConstantType>());
                AbstractValuation vectorVal(val);
                for (auto const& vectorVar : vectorEntryVariables) {
                    if (occurringVariables.find(vectorVar) == occurringVariables.end()) {
                        assert(!generateRowLabels);
                        vectorVal.addParameterUnspecified(vectorVar);
                    }
                }
                ConstantType& placeholder = functionValuationCollector.add(pVectorEntry, vectorVal);
                vectorAssignment.push_back(
                    std::pair<typename std::vector<ConstantType>::iterator, ConstantType&>(typename std::vector<ConstantType>::iterator(), placeholder));
            }

            ++newRowIndex;
        }
        if (useMonotonicityInFuture) {
            // Save the occuringVariables of a state, needed if we want to use monotonicity
            for (auto& var : occurringVariables) {
                occuringStatesAtVariable[var].insert(rowIndex);
            }
            occurringVariablesAtState[rowIndex] = std::move(occurringVariables);
        }
    }

    // Matrix and vector are now filled with constant results from constant functions and place holders for non-constant functions.
    matrix = builder.build(newRowIndex);
    vector.shrink_to_fit();
    matrixAssignment.shrink_to_fit();
    vectorAssignment.shrink_to_fit();
    nonConstMatrixEntries.resize(pMatrixEntryCount);

    // Now insert the correct iterators for the matrix and vector assignment
    auto matrixAssignmentIt = matrixAssignment.begin();
    uint_fast64_t startEntryOfRow = 0;
    for (uint_fast64_t group = 0; group < matrix.getRowGroupCount(); ++group) {
        uint_fast64_t startEntryOfNextRow = startEntryOfRow + matrix.getRow(group, 0).getNumberOfEntries();
        for (uint_fast64_t matrixRow = matrix.getRowGroupIndices()[group]; matrixRow < matrix.getRowGroupIndices()[group + 1]; ++matrixRow) {
            auto matrixEntryIt = matrix.getRow(matrixRow).begin();
            for (uint_fast64_t nonConstEntryIndex = nonConstMatrixEntries.getNextSetIndex(startEntryOfRow); nonConstEntryIndex < startEntryOfNextRow;
                 nonConstEntryIndex = nonConstMatrixEntries.getNextSetIndex(nonConstEntryIndex + 1)) {
                matrixAssignmentIt->first = matrixEntryIt + (nonConstEntryIndex - startEntryOfRow);
                ++matrixAssignmentIt;
            }
        }
        startEntryOfRow = startEntryOfNextRow;
    }
    STORM_LOG_ASSERT(matrixAssignmentIt == matrixAssignment.end(), "Unexpected number of entries in the matrix assignment.");

    auto vectorAssignmentIt = vectorAssignment.begin();
    for (auto nonConstVectorEntry : nonConstVectorEntries) {
        for (uint_fast64_t vectorIndex = matrix.getRowGroupIndices()[nonConstVectorEntry]; vectorIndex != matrix.getRowGroupIndices()[nonConstVectorEntry + 1];
             ++vectorIndex) {
            vectorAssignmentIt->first = vector.begin() + vectorIndex;
            ++vectorAssignmentIt;
        }
    }
    STORM_LOG_ASSERT(vectorAssignmentIt == vectorAssignment.end(), "Unexpected number of entries in the vector assignment.");
}

template<typename ParametricType, typename ConstantType>
void ParameterLifter<ParametricType, ConstantType>::specifyRegion(storm::storage::ParameterRegion<ParametricType> const& region,
                                                                  storm::solver::OptimizationDirection const& dirForParameters) {
    // write the evaluation result of each function,evaluation pair into the placeholders
    functionValuationCollector.evaluateCollectedFunctions(region, dirForParameters);

    // apply the matrix and vector assignments to write the contents of the placeholder into the matrix/vector
    for (auto& assignment : matrixAssignment) {
        STORM_LOG_WARN_COND(
            !storm::utility::isZero(assignment.second),
            "Parameter lifting on region "
                << region.toString()
                << " affects the underlying graph structure (the region is not strictly well defined). The result for this region might be incorrect.");
        assignment.first->setValue(assignment.second);
    }

    for (auto& assignment : vectorAssignment) {
        *assignment.first = assignment.second;
    }
}

template<typename ParametricType, typename ConstantType>
uint_fast64_t ParameterLifter<ParametricType, ConstantType>::getRowGroupIndex(uint_fast64_t originalState) const {
    return matrix.getRowGroupIndices()[oldToNewColumnIndexMapping[originalState]];
}

template<typename ParametricType, typename ConstantType>
uint_fast64_t ParameterLifter<ParametricType, ConstantType>::getOriginalStateNumber(uint_fast64_t newState) const {
    return rowGroupToStateNumber[newState];
}

template<typename ParametricType, typename ConstantType>
uint_fast64_t ParameterLifter<ParametricType, ConstantType>::getRowGroupSize(uint_fast64_t originalState) const {
    return matrix.getRowGroupSize(oldToNewColumnIndexMapping[originalState]);
}
template<typename ParametricType, typename ConstantType>
uint_fast64_t ParameterLifter<ParametricType, ConstantType>::getRowGroupCount() const {
    return matrix.getRowGroupCount();
}

template<typename ParametricType, typename ConstantType>
storm::storage::SparseMatrix<ConstantType> const& ParameterLifter<ParametricType, ConstantType>::getMatrix() const {
    return matrix;
}

template<typename ParametricType, typename ConstantType>
std::vector<ConstantType> const& ParameterLifter<ParametricType, ConstantType>::getVector() const {
    return vector;
}

template<typename ParametricType, typename ConstantType>
std::vector<typename ParameterLifter<ParametricType, ConstantType>::AbstractValuation> const& ParameterLifter<ParametricType, ConstantType>::getRowLabels()
    const {
    return rowLabels;
}

template<typename ParametricType, typename ConstantType>
std::vector<typename ParameterLifter<ParametricType, ConstantType>::AbstractValuation>
ParameterLifter<ParametricType, ConstantType>::getVerticesOfAbstractRegion(std::set<VariableType> const& variables) const {
    std::size_t const numOfVertices = std::pow(2, variables.size());
    std::vector<AbstractValuation> result(numOfVertices);

    for (uint_fast64_t vertexId = 0; vertexId < numOfVertices; ++vertexId) {
        // interprete vertexId as a bit sequence
        // the consideredVariables.size() least significant bits of vertex will always represent the next vertex
        //(00...0 = lower boundaries for all variables, 11...1 = upper boundaries for all variables)
        uint_fast64_t variableIndex = 0;
        for (auto const& variable : variables) {
            if ((vertexId >> variableIndex) % 2 == 0) {
                result[vertexId].addParameterLower(variable);
            } else {
                result[vertexId].addParameterUpper(variable);
            }
            ++variableIndex;
        }
    }
    return result;
}

template<typename ParametricType, typename ConstantType>
const std::vector<std::set<typename ParameterLifter<ParametricType, ConstantType>::VariableType>>&
ParameterLifter<ParametricType, ConstantType>::getOccurringVariablesAtState() const {
    return occurringVariablesAtState;
}

template<typename ParametricType, typename ConstantType>
std::map<typename ParameterLifter<ParametricType, ConstantType>::VariableType, std::set<uint_fast64_t>> const&
ParameterLifter<ParametricType, ConstantType>::getOccuringStatesAtVariable() const {
    return occuringStatesAtVariable;
}

template<typename ParametricType, typename ConstantType>
bool ParameterLifter<ParametricType, ConstantType>::AbstractValuation::operator==(AbstractValuation const& other) const {
    return this->lowerPars == other.lowerPars && this->upperPars == other.upperPars && this->unspecifiedPars == other.unspecifiedPars;
}

template<typename ParametricType, typename ConstantType>
void ParameterLifter<ParametricType, ConstantType>::AbstractValuation::addParameterLower(VariableType const& var) {
    lowerPars.insert(var);
}

template<typename ParametricType, typename ConstantType>
void ParameterLifter<ParametricType, ConstantType>::AbstractValuation::addParameterUpper(VariableType const& var) {
    upperPars.insert(var);
}

template<typename ParametricType, typename ConstantType>
void ParameterLifter<ParametricType, ConstantType>::AbstractValuation::addParameterUnspecified(VariableType const& var) {
    unspecifiedPars.insert(var);
}

template<typename ParametricType, typename ConstantType>
std::size_t ParameterLifter<ParametricType, ConstantType>::AbstractValuation::getHashValue() const {
    std::size_t seed = 0;
    for (auto const& p : lowerPars) {
        carl::hash_add(seed, p);
    }
    for (auto const& p : upperPars) {
        carl::hash_add(seed, p);
    }
    for (auto const& p : unspecifiedPars) {
        carl::hash_add(seed, p);
    }
    return seed;
}

template<typename ParametricType, typename ConstantType>
typename ParameterLifter<ParametricType, ConstantType>::AbstractValuation ParameterLifter<ParametricType, ConstantType>::AbstractValuation::getSubValuation(
    std::set<VariableType> const& pars) const {
    AbstractValuation result;
    for (auto const& p : pars) {
        if (std::find(lowerPars.begin(), lowerPars.end(), p) != lowerPars.end()) {
            result.addParameterLower(p);
        } else if (std::find(upperPars.begin(), upperPars.end(), p) != upperPars.end()) {
            result.addParameterUpper(p);
        } else if (std::find(unspecifiedPars.begin(), unspecifiedPars.end(), p) != unspecifiedPars.end()) {
            result.addParameterUnspecified(p);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                            "Tried to obtain a subvaluation for parameters that are not specified by this valuation");
        }
    }
    return result;
}

template<typename ParametricType, typename ConstantType>
std::set<typename ParameterLifter<ParametricType, ConstantType>::VariableType> const&
ParameterLifter<ParametricType, ConstantType>::AbstractValuation::getLowerParameters() const {
    return lowerPars;
}

template<typename ParametricType, typename ConstantType>
std::set<typename ParameterLifter<ParametricType, ConstantType>::VariableType> const&
ParameterLifter<ParametricType, ConstantType>::AbstractValuation::getUpperParameters() const {
    return upperPars;
}

template<typename ParametricType, typename ConstantType>
std::set<typename ParameterLifter<ParametricType, ConstantType>::VariableType> const&
ParameterLifter<ParametricType, ConstantType>::AbstractValuation::getUnspecifiedParameters() const {
    return unspecifiedPars;
}

template<typename ParametricType, typename ConstantType>
std::vector<storm::utility::parametric::Valuation<ParametricType>> ParameterLifter<ParametricType, ConstantType>::AbstractValuation::getConcreteValuations(
    storm::storage::ParameterRegion<ParametricType> const& region) const {
    auto result = region.getVerticesOfRegion(unspecifiedPars);
    for (auto& valuation : result) {
        for (auto const& lowerPar : lowerPars) {
            valuation.insert(std::pair<VariableType, CoefficientType>(lowerPar, region.getLowerBoundary(lowerPar)));
        }
        for (auto const& upperPar : upperPars) {
            valuation.insert(std::pair<VariableType, CoefficientType>(upperPar, region.getUpperBoundary(upperPar)));
        }
    }
    return result;
}

template<typename ParametricType, typename ConstantType>
ConstantType& ParameterLifter<ParametricType, ConstantType>::FunctionValuationCollector::add(ParametricType const& function,
                                                                                             AbstractValuation const& valuation) {
    ParametricType simplifiedFunction = function;
    storm::utility::simplify(simplifiedFunction);
    std::set<VariableType> variablesInFunction;
    storm::utility::parametric::gatherOccurringVariables(simplifiedFunction, variablesInFunction);
    AbstractValuation simplifiedValuation = valuation.getSubValuation(variablesInFunction);
    // insert the function and the valuation
    // Note that references to elements of an unordered map remain valid after calling unordered_map::insert.
    auto insertionRes = collectedFunctions.insert(std::pair<FunctionValuation, ConstantType>(
        FunctionValuation(std::move(simplifiedFunction), std::move(simplifiedValuation)), storm::utility::one<ConstantType>()));
    return insertionRes.first->second;
}

template<typename ParametricType, typename ConstantType>
void ParameterLifter<ParametricType, ConstantType>::FunctionValuationCollector::evaluateCollectedFunctions(
    storm::storage::ParameterRegion<ParametricType> const& region, storm::solver::OptimizationDirection const& dirForUnspecifiedParameters) {
    for (auto& collectedFunctionValuationPlaceholder : collectedFunctions) {
        ParametricType const& function = collectedFunctionValuationPlaceholder.first.first;
        AbstractValuation const& abstrValuation = collectedFunctionValuationPlaceholder.first.second;
        ConstantType& placeholder = collectedFunctionValuationPlaceholder.second;
        auto concreteValuations = abstrValuation.getConcreteValuations(region);
        auto concreteValuationIt = concreteValuations.begin();
        placeholder = storm::utility::parametric::evaluate<ConstantType>(function, *concreteValuationIt);
        for (++concreteValuationIt; concreteValuationIt != concreteValuations.end(); ++concreteValuationIt) {
            ConstantType currentResult = storm::utility::parametric::evaluate<ConstantType>(function, *concreteValuationIt);
            if (storm::solver::minimize(dirForUnspecifiedParameters)) {
                placeholder = std::min(placeholder, currentResult);
            } else {
                placeholder = std::max(placeholder, currentResult);
            }
        }
    }
}

template class ParameterLifter<storm::RationalFunction, double>;
template class ParameterLifter<storm::RationalFunction, storm::RationalNumber>;
}  // namespace transformer
}  // namespace storm
