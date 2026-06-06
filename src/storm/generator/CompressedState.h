#ifndef STORM_GENERATOR_COMPRESSEDSTATE_H_
#define STORM_GENERATOR_COMPRESSEDSTATE_H_

#include <map>
#include <unordered_map>
#include "storm/adapters/JsonForward.h"
#include "storm/storage/BitVector.h"

namespace storm {
namespace umb {
class Valuations;
}
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
 * Appends the values of the variables in the given state to the valuations object.
 * Assumes that the order of variables in the variableInformation and valuations objects are aligned.
 * @param state The state.
 * @param variableInformation The variables.
 * @param valuations the valuations to which the variable values should be appended.
 */
void unpackStateAppendToUmbValuations(CompressedState const& state, VariableInformation const& variableInformation, storm::umb::Valuations& valuations);

/*!
 * Sets the values of observable variables and observation expressions to the given observationClassIndex of the given valuations.
 */
void unpackObservationClassIntoUmbValuations(CompressedState const& observationClass, uint64_t const observationClassIndex,
                                             VariableInformation const& variableInformation, storm::umb::Valuations& valuations);

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

#endif /* STORM_GENERATOR_COMPRESSEDSTATE_H_ */
