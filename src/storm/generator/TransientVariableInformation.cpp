#include "storm/generator/TransientVariableInformation.h"

#include "storm/storage/jani/Model.h"

#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/Automaton.h"
#include "storm/storage/jani/AutomatonComposition.h"
#include "storm/storage/jani/ParallelComposition.h"
#include "storm/storage/jani/eliminator/ArrayEliminator.h"
#include "storm/storage/umb/model/Valuations.h"

#include "storm/exceptions/OutOfRangeException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

#include <cmath>

namespace storm {
namespace generator {

template<>
TransientVariableData<storm::RationalFunction>::TransientVariableData(storm::expressions::Variable const& variable,
                                                                      boost::optional<storm::RationalFunction> const& lowerBound,
                                                                      boost::optional<storm::RationalFunction> const& upperBound,
                                                                      storm::RationalFunction const& defaultValue, bool global)
    : variable(variable), lowerBound(lowerBound), upperBound(upperBound), defaultValue(defaultValue), global(global) {
    // There is no '<=' for rational functions. Therefore, do not check the bounds for this ValueType
}

template<typename VariableType>
TransientVariableData<VariableType>::TransientVariableData(storm::expressions::Variable const& variable, boost::optional<VariableType> const& lowerBound,
                                                           boost::optional<VariableType> const& upperBound, VariableType const& defaultValue, bool global)
    : variable(variable), lowerBound(lowerBound), upperBound(upperBound), defaultValue(defaultValue), global(global) {
    STORM_LOG_THROW(!lowerBound.is_initialized() || lowerBound.get() <= defaultValue, storm::exceptions::OutOfRangeException,
                    "The default value for transient variable " << variable.getName() << " is smaller than its lower bound.");
    STORM_LOG_THROW(!upperBound.is_initialized() || defaultValue <= upperBound.get(), storm::exceptions::OutOfRangeException,
                    "The default value for transient variable " << variable.getName() << " is higher than its upper bound.");
}

template<typename VariableType>
TransientVariableData<VariableType>::TransientVariableData(storm::expressions::Variable const& variable, VariableType const& defaultValue, bool global)
    : variable(variable), defaultValue(defaultValue), global(global) {
    // Intentionally left empty.
}

template<typename ValueType>
void TransientVariableValuation<ValueType>::clear() {
    booleanValues.clear();
    integerValues.clear();
    rationalValues.clear();
}

template<typename ValueType>
bool TransientVariableValuation<ValueType>::empty() const {
    return booleanValues.empty() && integerValues.empty() && rationalValues.empty();
}

template<typename ValueType>
void TransientVariableValuation<ValueType>::setInEvaluator(storm::expressions::ExpressionEvaluator<ValueType>& evaluator, bool explorationChecks) const {
    for (auto const& varValue : booleanValues) {
        evaluator.setBooleanValue(varValue.first->variable, varValue.second);
    }
    for (auto const& varValue : integerValues) {
        if (explorationChecks) {
            STORM_LOG_THROW(!varValue.first->lowerBound.is_initialized() || varValue.first->lowerBound.get() <= varValue.second,
                            storm::exceptions::OutOfRangeException,
                            "The assigned value for transient variable " << varValue.first->variable.getName() << " is smaller than its lower bound.");
            STORM_LOG_THROW(!varValue.first->upperBound.is_initialized() || varValue.second <= varValue.first->upperBound.get(),
                            storm::exceptions::OutOfRangeException,
                            "The assigned value for transient variable " << varValue.first->variable.getName() << " is higher than its upper bound.");
        }
        evaluator.setIntegerValue(varValue.first->variable, varValue.second);
    }
    for (auto const& varValue : rationalValues) {
        evaluator.setRationalValue(varValue.first->variable, varValue.second);
    }
}

template<typename ValueType>
void TransientVariableValuation<ValueType>::setInUmbValuations(uint64_t const stateIndex, TransientVariableInformation<ValueType> const& info,
                                                               storm::umb::Valuations& valuations) const {
    auto writeValues = [stateIndex, &valuations](auto const& varInfos, auto const& varValues) {
        auto varIt = varValues.begin();
        auto const varIte = varValues.end();
        for (auto const& varInfo : varInfos) {
            bool const hasValue = varIt != varIte && varIt->first->variable == varInfo.variable;
            auto const& value = hasValue ? varIt->second : varInfo.defaultValue;
            if (hasValue) {
                ++varIt;
            }
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>, storm::RationalFunction>) {
                STORM_LOG_THROW(
                    storm::utility::isConstant(value), storm::exceptions::NotSupportedException,
                    "Non-constant variable valuations are not supported. Got value " << value << " for variable " << varInfo.variable.getName() << ".");
                valuations.writeValue(stateIndex, varInfo.variable, storm::utility::convertNumber<storm::RationalNumber>(value));
            } else {
                valuations.writeValue(stateIndex, varInfo.variable, value);
            }
        }
    };
    writeValues(info.booleanVariableInformation, booleanValues);
    writeValues(info.integerVariableInformation, integerValues);
    writeValues(info.rationalVariableInformation, rationalValues);
}

template<typename ValueType>
TransientVariableInformation<ValueType>::TransientVariableInformation(
    storm::jani::Model const& model, std::vector<std::reference_wrapper<storm::jani::Automaton const>> const& parallelAutomata) {
    createVariablesForVariableSet(model.getGlobalVariables(), true);

    for (auto const& automatonRef : parallelAutomata) {
        createVariablesForAutomaton(automatonRef.get());
    }

    sortVariables();
}

template<typename ValueType>
void TransientVariableInformation<ValueType>::registerArrayVariableReplacements(storm::jani::ArrayEliminatorData const& arrayEliminatorData) {
    arrayVariableToElementInformations.clear();
    // Find for each replaced array variable the corresponding references in this variable information
    for (auto const& arrayVariable : arrayEliminatorData.eliminatedArrayVariables) {
        if (arrayVariable->isTransient()) {
            auto findRes = arrayEliminatorData.replacements.find(arrayVariable->getExpressionVariable());
            STORM_LOG_ASSERT(findRes != arrayEliminatorData.replacements.end(), "No replacement for array variable.");
            auto const& replacements = findRes->second;
            auto const& innerType = arrayVariable->getType().asArrayType().getBaseTypeRecursive();
            if (innerType.isBasicType() && innerType.asBasicType().isBooleanType()) {
                auto replInfo = convertArrayReplacement(replacements, booleanVariableInformation);
                this->arrayVariableToElementInformations.emplace(arrayVariable->getExpressionVariable(), std::move(replInfo));
            } else if ((innerType.isBasicType() && innerType.asBasicType().isIntegerType()) ||
                       (innerType.isBoundedType() && innerType.asBoundedType().isIntegerType())) {
                auto replInfo = convertArrayReplacement(replacements, integerVariableInformation);
                this->arrayVariableToElementInformations.emplace(arrayVariable->getExpressionVariable(), std::move(replInfo));
            } else if ((innerType.isBasicType() && innerType.asBasicType().isRealType()) ||
                       (innerType.isBoundedType() && innerType.asBoundedType().isRealType())) {
                auto replInfo = convertArrayReplacement(replacements, rationalVariableInformation);
                this->arrayVariableToElementInformations.emplace(arrayVariable->getExpressionVariable(), std::move(replInfo));
            } else {
                STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unhandled type of base variable.");
            }
        }
    }
}

template<typename ValueType>
TransientVariableData<bool> const& TransientVariableInformation<ValueType>::getBooleanArrayVariableReplacement(
    storm::expressions::Variable const& arrayVariable, std::vector<uint64_t> const& arrayIndexVector) const {
    return booleanVariableInformation[arrayVariableToElementInformations.at(arrayVariable).getVariableInformationIndex(arrayIndexVector)];
}

template<typename ValueType>
TransientVariableData<int64_t> const& TransientVariableInformation<ValueType>::getIntegerArrayVariableReplacement(
    storm::expressions::Variable const& arrayVariable, std::vector<uint64_t> const& arrayIndexVector) const {
    return integerVariableInformation[arrayVariableToElementInformations.at(arrayVariable).getVariableInformationIndex(arrayIndexVector)];
}

template<typename ValueType>
TransientVariableData<ValueType> const& TransientVariableInformation<ValueType>::getRationalArrayVariableReplacement(
    storm::expressions::Variable const& arrayVariable, std::vector<uint64_t> const& arrayIndexVector) const {
    return rationalVariableInformation[arrayVariableToElementInformations.at(arrayVariable).getVariableInformationIndex(arrayIndexVector)];
}

template<typename ValueType>
void TransientVariableInformation<ValueType>::createVariablesForAutomaton(storm::jani::Automaton const& automaton) {
    createVariablesForVariableSet(automaton.getVariables(), false);
}

template<typename ValueType>
void TransientVariableInformation<ValueType>::createVariablesForVariableSet(storm::jani::VariableSet const& variableSet, bool global) {
    for (auto const& variable : variableSet.getBooleanVariables()) {
        if (variable.isTransient()) {
            booleanVariableInformation.emplace_back(variable.getExpressionVariable(), variable.getInitExpression().evaluateAsBool(), global);
        }
    }
    for (auto const& variable : variableSet.getBoundedIntegerVariables()) {
        if (variable.isTransient()) {
            boost::optional<int64_t> lowerBound;
            boost::optional<int64_t> upperBound;
            auto const& type = variable.getType().asBoundedType();
            if (type.hasLowerBound()) {
                lowerBound = type.getLowerBound().evaluateAsInt();
            }
            if (type.hasUpperBound()) {
                upperBound = type.getUpperBound().evaluateAsInt();
            }
            integerVariableInformation.emplace_back(variable.getExpressionVariable(), lowerBound, upperBound, variable.getInitExpression().evaluateAsInt(),
                                                    global);
        }
    }
    for (auto const& variable : variableSet.getUnboundedIntegerVariables()) {
        if (variable.isTransient()) {
            integerVariableInformation.emplace_back(variable.getExpressionVariable(), variable.getInitExpression().evaluateAsInt(), global);
        }
    }
    for (auto const& variable : variableSet.getRealVariables()) {
        if (variable.isTransient()) {
            rationalVariableInformation.emplace_back(variable.getExpressionVariable(),
                                                     storm::utility::convertNumber<ValueType>(variable.getInitExpression().evaluateAsRational()), global);
        }
    }
}

template<typename ValueType>
void TransientVariableInformation<ValueType>::sortVariables() {
    // Sort the variables so we can make some assumptions when iterating over them (in the next-state generators).
    std::sort(booleanVariableInformation.begin(), booleanVariableInformation.end(),
              [](TransientVariableData<bool> const& a, TransientVariableData<bool> const& b) { return a.variable < b.variable; });
    std::sort(integerVariableInformation.begin(), integerVariableInformation.end(),
              [](TransientVariableData<int64_t> const& a, TransientVariableData<int64_t> const& b) { return a.variable < b.variable; });
    std::sort(rationalVariableInformation.begin(), rationalVariableInformation.end(),
              [](TransientVariableData<ValueType> const& a, TransientVariableData<ValueType> const& b) { return a.variable < b.variable; });
}

template<typename ValueType>
void TransientVariableInformation<ValueType>::setDefaultValuesInEvaluator(storm::expressions::ExpressionEvaluator<ValueType>& evaluator) const {
    for (auto const& variableData : booleanVariableInformation) {
        evaluator.setBooleanValue(variableData.variable, variableData.defaultValue);
    }
    for (auto const& variableData : integerVariableInformation) {
        evaluator.setIntegerValue(variableData.variable, variableData.defaultValue);
    }
    for (auto const& variableData : rationalVariableInformation) {
        evaluator.setRationalValue(variableData.variable, variableData.defaultValue);
    }
}

template struct TransientVariableValuation<double>;
template struct TransientVariableValuation<storm::RationalNumber>;
template struct TransientVariableValuation<storm::RationalFunction>;
template struct TransientVariableInformation<double>;
template struct TransientVariableInformation<storm::RationalNumber>;
template struct TransientVariableInformation<storm::RationalFunction>;

}  // namespace generator
}  // namespace storm
