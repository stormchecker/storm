#pragma once

#include <map>
#include <unordered_map>
#include "storm/adapters/JsonForward.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace expressions {
template<typename ValueType>
class ExpressionEvaluator;

class ExpressionManager;
class SimpleValuation;
class Variable;
class Expression;
}  // namespace expressions

namespace generator {
typedef storm::storage::BitVector CompressedState;

struct VariableInformation;

/*!
 * Unpacks the compressed state into the evaluator.
 *
 * @param state The state to unpack.
 * @param variableInformation The information about how the variables are packed within the state.
 * @param evaluator The evaluator into which to load the state.
 */
template<typename ValueType>
void unpackStateIntoEvaluator(CompressedState const& state, VariableInformation const& variableInformation,
                              storm::expressions::ExpressionEvaluator<ValueType>& evaluator);

/*!
 * Converts the compressed state into an explicit representation in the form of a valuation.
 *
 * @param state The state to unpack.
 * @param variableInformation The information about how the variables are packed within the state.
 * @param manager The manager responsible for the variables.
 * @return A valuation that corresponds to the compressed state.
 */
storm::expressions::SimpleValuation unpackStateIntoValuation(CompressedState const& state, VariableInformation const& variableInformation,
                                                             storm::expressions::ExpressionManager const& manager);

/**
 *
 * @tparam ValueType  (The ValueType does not matter for the string representation.)
 * @param state The state to export
 * @param variableInformation Variable information to extract from the state
 * @param onlyObservable Should we only export the observable information
 * @return
 */
template<typename ValueType>
storm::json<ValueType> unpackStateIntoJson(CompressedState const& state, VariableInformation const& variableInformation, bool onlyObservable);

/*!
 * Appends the values of the given variables in the given state to the corresponding result vectors.
 * locationValues are inserted before integerValues (relevant if both, locationValues and integerValues actually refer to the same vector)
 * @param state The state
 * @param variableInformation The variables
 * @param locationValues
 * @param booleanValues
 * @param integerValues
 */
void extractVariableValues(CompressedState const& state, VariableInformation const& variableInformation, std::vector<int64_t>& locationValues,
                           std::vector<bool>& booleanValues, std::vector<int64_t>& integerValues);

/*!
 * Returns a (human readable) string representation of the variable valuation encoded by the given state
 */
std::string toString(CompressedState const& state, VariableInformation const& variableInformation);

/*!
 *
 * @param variableInformation
 * @return
 */
storm::storage::BitVector computeObservabilityMask(VariableInformation const& variableInformation);
/*!
 *
 * @param state
 * @param observabilityMap
 * @param mask
 * @return
 */
uint32_t unpackStateToObservabilityClass(CompressedState const& state, storm::storage::BitVector const& observationVector,
                                         std::unordered_map<storm::storage::BitVector, uint32_t>& observabilityMap, storm::storage::BitVector const& mask);
/*!
 *
 * @param varInfo
 * @param roundTo64Bit
 * @return
 */
CompressedState createOutOfBoundsState(VariableInformation const& varInfo, bool roundTo64Bit = true);

CompressedState createCompressedState(VariableInformation const& varInfo,
                                      std::map<storm::expressions::Variable, storm::expressions::Expression> const& stateDescription, bool checkOutOfBounds);

CompressedState packStateFromValuation(expressions::SimpleValuation const& valuation, VariableInformation const& variableInformation,
                                       bool checkOutOfBounds = false);

}  // namespace generator
}  // namespace storm
