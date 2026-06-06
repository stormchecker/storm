#pragma once

#include <boost/optional.hpp>
#include <unordered_map>
#include <vector>

#include "storm/adapters/RationalFunctionForward.h"
#include "storm/generator/ArrayVariableReplacementInformation.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/OutOfRangeException.h"

namespace storm {

namespace jani {
class Model;
class Automaton;
struct ArrayEliminatorData;
class VariableSet;
}  // namespace jani

namespace expressions {
template<typename ValueType>
class ExpressionEvaluator;
}

namespace umb {
class Valuations;
}

namespace generator {

template<typename ValueType>
struct TransientVariableInformation;

// A structure storing information about a transient variable
template<typename VariableType>
struct TransientVariableData {
    TransientVariableData(storm::expressions::Variable const& variable, boost::optional<VariableType> const& lowerBound,
                          boost::optional<VariableType> const& upperBound, VariableType const& defaultValue, bool global = false);
    TransientVariableData(storm::expressions::Variable const& variable, VariableType const& defaultValue, bool global = false);

    // The integer variable.
    storm::expressions::Variable variable;

    // The lower bound of its range.
    boost::optional<VariableType> lowerBound;

    // The upper bound of its range.
    boost::optional<VariableType> upperBound;

    // Its default value
    VariableType defaultValue;

    // A flag indicating whether the variable is a global one.
    bool global;
};

template<typename ValueType>
struct TransientVariableValuation {
    std::vector<std::pair<TransientVariableData<bool> const*, bool>> booleanValues;
    std::vector<std::pair<TransientVariableData<int64_t> const*, int64_t>> integerValues;
    std::vector<std::pair<TransientVariableData<ValueType> const*, ValueType>> rationalValues;

    void clear();

    bool empty() const;

    void setInEvaluator(storm::expressions::ExpressionEvaluator<ValueType>& evaluator, bool explorationChecks) const;

    void setInUmbValuations(uint64_t const stateIndex, TransientVariableInformation<ValueType> const& info, storm::umb::Valuations& valuations) const;
};

// A structure storing information about the used variables of the program.
template<typename ValueType>
struct TransientVariableInformation {
    TransientVariableInformation(storm::jani::Model const& model, std::vector<std::reference_wrapper<storm::jani::Automaton const>> const& parallelAutomata);

    TransientVariableInformation() = default;

    void registerArrayVariableReplacements(storm::jani::ArrayEliminatorData const& arrayEliminatorData);
    TransientVariableData<bool> const& getBooleanArrayVariableReplacement(storm::expressions::Variable const& arrayVariable,
                                                                          std::vector<uint64_t> const& arrayIndexVector) const;
    TransientVariableData<int64_t> const& getIntegerArrayVariableReplacement(storm::expressions::Variable const& arrayVariable,
                                                                             std::vector<uint64_t> const& arrayIndexVector) const;
    TransientVariableData<ValueType> const& getRationalArrayVariableReplacement(storm::expressions::Variable const& arrayVariable,
                                                                                std::vector<uint64_t> const& arrayIndexVector) const;

    void setDefaultValuesInEvaluator(storm::expressions::ExpressionEvaluator<ValueType>& evaluator) const;

    std::vector<TransientVariableData<bool>> booleanVariableInformation;
    std::vector<TransientVariableData<int64_t>> integerVariableInformation;
    std::vector<TransientVariableData<ValueType>> rationalVariableInformation;

    /// Replacements for each array variable
    std::unordered_map<storm::expressions::Variable, ArrayVariableReplacementInformation> arrayVariableToElementInformations;

   private:
    /*!
     * Sorts the variables to establish a known ordering.
     */
    void sortVariables();

    /*!
     * Creates all necessary variables for a JANI automaton.
     */
    void createVariablesForAutomaton(storm::jani::Automaton const& automaton);

    /*!
     * Creates all non-transient variables from the given set
     */
    void createVariablesForVariableSet(storm::jani::VariableSet const& variableSet, bool global);
};

}  // namespace generator
}  // namespace storm
