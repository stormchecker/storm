#include "storm/generator/NextStateGenerator.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/logic/Formulas.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/SimpleValuation.h"
#include "storm/utility/macros.h"

namespace storm {
namespace generator {

template<typename ValueType, typename StateType>
StateValuationFunctionMask<ValueType, StateType>::StateValuationFunctionMask(std::function<bool(storm::expressions::SimpleValuation const&, uint64_t)> const& f)
    : func(f) {
    // Intentionally left empty
}

template<typename ValueType, typename StateType>
bool StateValuationFunctionMask<ValueType, StateType>::query(storm::generator::NextStateGenerator<ValueType, StateType> const& generator,
                                                             uint64_t actionIndex) {
    auto val = generator.currentStateToSimpleValuation();
    bool res = func(val, actionIndex);
    return res;
}

template<typename ValueType, typename StateType>
NextStateGenerator<ValueType, StateType>::NextStateGenerator(storm::expressions::ExpressionManager const& expressionManager,
                                                             VariableInformation const& variableInformation, NextStateGeneratorOptions const& options,
                                                             std::shared_ptr<ActionMask<ValueType, StateType>> const& mask)
    : options(options),
      expressionManager(expressionManager.getSharedPointer()),
      variableInformation(variableInformation),
      evaluator(nullptr),
      state(nullptr),
      comparator(storm::utility::convertNumber<ValueType>(options.getStochasticTolerance())),
      actionMask(mask) {
    initializeSpecialStates();
}

template<typename ValueType, typename StateType>
NextStateGenerator<ValueType, StateType>::NextStateGenerator(storm::expressions::ExpressionManager const& expressionManager,
                                                             NextStateGeneratorOptions const& options,
                                                             std::shared_ptr<ActionMask<ValueType, StateType>> const& mask)
    : options(options),
      expressionManager(expressionManager.getSharedPointer()),
      variableInformation(),
      evaluator(nullptr),
      state(nullptr),
      comparator(storm::utility::convertNumber<ValueType>(options.getStochasticTolerance())),
      actionMask(mask) {}

template<typename ValueType, typename StateType>
NextStateGenerator<ValueType, StateType>::~NextStateGenerator() = default;

template<typename ValueType, typename StateType>
NextStateGeneratorOptions const& NextStateGenerator<ValueType, StateType>::getOptions() const {
    return options;
}

template<typename ValueType, typename StateType>
uint64_t NextStateGenerator<ValueType, StateType>::getStateSize() const {
    return variableInformation.getTotalBitOffset(true);
}

template<typename ValueType, typename StateType>
void NextStateGenerator<ValueType, StateType>::initializeSpecialStates() {
    if (variableInformation.hasOutOfBoundsBit()) {
        outOfBoundsState = createOutOfBoundsState(variableInformation);
    }
    if (options.isAddOverlappingGuardLabelSet()) {
        overlappingGuardStates = std::vector<uint64_t>();
    }
}

template<typename ValueType, typename StateType>
storm::storage::sparse::StateValuationsBuilder NextStateGenerator<ValueType, StateType>::initializeStateValuationsBuilder() const {
    storm::storage::sparse::StateValuationsBuilder result;
    for (auto const& v : variableInformation.locationVariables) {
        result.addVariable(v.variable);
    }
    for (auto const& v : variableInformation.booleanVariables) {
        result.addVariable(v.variable);
    }
    for (auto const& v : variableInformation.integerVariables) {
        result.addVariable(v.variable);
    }
    return result;
}

template<typename ValueType, typename StateType>
storm::storage::sparse::StateValuationsBuilder NextStateGenerator<ValueType, StateType>::initializeObservationValuationsBuilder() const {
    storm::storage::sparse::StateValuationsBuilder result;
    for (auto const& v : variableInformation.booleanVariables) {
        if (v.observable) {
            result.addVariable(v.variable);
        }
    }
    for (auto const& v : variableInformation.integerVariables) {
        if (v.observable) {
            result.addVariable(v.variable);
        }
    }
    for (auto const& l : variableInformation.observationLabels) {
        result.addObservationLabel(l.name);
    }
    return result;
}

template<typename ValueType, typename StateType>
void NextStateGenerator<ValueType, StateType>::load(CompressedState const& state) {
    // Since almost all subsequent operations are based on the evaluator, we load the state into it now.
    unpackStateIntoEvaluator(state, variableInformation, *evaluator);

    // Also, we need to store a pointer to the state itself, because we need to be able to access it when expanding it.
    this->state = &state;
}

template<typename ValueType, typename StateType>
bool NextStateGenerator<ValueType, StateType>::satisfies(storm::expressions::Expression const& expression) const {
    if (expression.isTrue()) {
        return true;
    }
    return evaluator->asBool(expression);
}

template<typename ValueType, typename StateType>
VariableInformation const& NextStateGenerator<ValueType, StateType>::getVariableInformation() const {
    return variableInformation;
}

template<typename ValueType, typename StateType>
void NextStateGenerator<ValueType, StateType>::addStateValuation(storm::storage::sparse::state_type const& currentStateIndex,
                                                                 storm::storage::sparse::StateValuationsBuilder& valuationsBuilder) const {
    std::vector<bool> booleanValues;
    booleanValues.reserve(variableInformation.booleanVariables.size());
    std::vector<int64_t> integerValues;
    integerValues.reserve(variableInformation.locationVariables.size() + variableInformation.integerVariables.size());
    extractVariableValues(*this->state, variableInformation, integerValues, booleanValues, integerValues);
    valuationsBuilder.addState(currentStateIndex, std::move(booleanValues), std::move(integerValues));
}

template<typename ValueType, typename StateType>
storm::storage::sparse::StateValuations NextStateGenerator<ValueType, StateType>::makeObservationValuation() const {
    storm::storage::sparse::StateValuationsBuilder valuationsBuilder = initializeObservationValuationsBuilder();
    for (auto const& observationEntry : observabilityMap) {
        std::vector<bool> booleanValues;
        booleanValues.reserve(variableInformation.booleanVariables.size());  // TODO: use number of observable boolean variables
        std::vector<int64_t> integerValues;
        integerValues.reserve(variableInformation.locationVariables.size() +
                              variableInformation.integerVariables.size());  // TODO: use number of observable integer variables
        std::vector<int64_t> observationLabelValues;
        observationLabelValues.reserve(variableInformation.observationLabels.size());
        expressions::SimpleValuation val = unpackStateIntoValuation(observationEntry.first, variableInformation, *expressionManager);
        for (auto const& v : variableInformation.booleanVariables) {
            if (v.observable) {
                booleanValues.push_back(val.getBooleanValue(v.variable));
            }
        }
        for (auto const& v : variableInformation.integerVariables) {
            if (v.observable) {
                integerValues.push_back(val.getIntegerValue(v.variable));
            }
        }
        for (uint64_t labelStart = variableInformation.getTotalBitOffset(true); labelStart < observationEntry.first.size(); labelStart += 64) {
            observationLabelValues.push_back(observationEntry.first.getAsInt(labelStart, 64));
        }
        valuationsBuilder.addState(observationEntry.second, std::move(booleanValues), std::move(integerValues), {}, std::move(observationLabelValues));
    }
    return valuationsBuilder.build();
}

template<typename ValueType, typename StateType>
storm::models::sparse::StateLabeling NextStateGenerator<ValueType, StateType>::label(
    storm::storage::sparse::StateStorage<StateType> const& stateStorage, std::vector<StateType> const& initialStateIndices,
    std::vector<StateType> const& deadlockStateIndices, std::vector<StateType> const& unexploredStateIndices,
    std::vector<std::pair<std::string, storm::expressions::Expression>> labelsAndExpressions) {
    labelsAndExpressions.insert(labelsAndExpressions.end(), this->options.getExpressionLabels().begin(), this->options.getExpressionLabels().end());

    // Make the labels unique.
    std::sort(labelsAndExpressions.begin(), labelsAndExpressions.end(),
              [](std::pair<std::string, storm::expressions::Expression> const& a, std::pair<std::string, storm::expressions::Expression> const& b) {
                  return a.first < b.first;
              });
    auto it = std::unique(labelsAndExpressions.begin(), labelsAndExpressions.end(),
                          [](std::pair<std::string, storm::expressions::Expression> const& a, std::pair<std::string, storm::expressions::Expression> const& b) {
                              return a.first == b.first;
                          });
    labelsAndExpressions.resize(std::distance(labelsAndExpressions.begin(), it));

    // Prepare result.
    storm::models::sparse::StateLabeling result(stateStorage.getNumberOfStates());

    // Initialize labeling.
    for (auto const& label : labelsAndExpressions) {
        result.addLabel(label.first);
    }

    auto const& states = stateStorage.stateToId;
    for (auto const& stateIndexPair : states) {
        unpackStateIntoEvaluator(stateIndexPair.first, variableInformation, *this->evaluator);
        unpackTransientVariableValuesIntoEvaluator(stateIndexPair.first, *this->evaluator);

        for (auto const& label : labelsAndExpressions) {
            // Add label to state, if the corresponding expression is true.
            if (evaluator->asBool(label.second)) {
                result.addLabelToState(label.first, stateIndexPair.second);
            }
        }
    }

    auto addSpecialLabel = [&result](std::string const& label, auto const& indices) {
        if (!result.containsLabel(label)) {
            result.addLabel(label);
            for (auto index : indices) {
                result.addLabelToState(label, index);
            }
        }
    };
    addSpecialLabel("init", initialStateIndices);
    addSpecialLabel("deadlock", deadlockStateIndices);
    if (!unexploredStateIndices.empty()) {
        addSpecialLabel("unexplored", unexploredStateIndices);
    }
    if (this->options.isAddOverlappingGuardLabelSet()) {
        STORM_LOG_THROW(!result.containsLabel("overlap_guards"), storm::exceptions::WrongFormatException,
                        "Label 'overlap_guards' is reserved when adding overlapping guard labels");
        addSpecialLabel("overlap_guards", overlappingGuardStates.get());
    }
    if (this->options.isAddOutOfBoundsStateSet() && stateStorage.stateToId.contains(outOfBoundsState)) {
        STORM_LOG_THROW(!result.containsLabel("out_of_bounds"), storm::exceptions::WrongFormatException,
                        "Label 'out_of_bounds' is reserved when adding out of bounds states.");
        addSpecialLabel("out_of_bounds", std::vector{stateStorage.stateToId.getValue(outOfBoundsState)});
    }

    return result;
}

template<typename ValueType, typename StateType>
bool NextStateGenerator<ValueType, StateType>::isSpecialLabel(std::string const& label) const {
    return label == "init" || label == "deadlock" || label == "unexplored" || label == "overlap_guards" || label == "out_of_bounds";
}

template<typename ValueType, typename StateType>
void NextStateGenerator<ValueType, StateType>::unpackTransientVariableValuesIntoEvaluator(CompressedState const&,
                                                                                          storm::expressions::ExpressionEvaluator<BaseValueType>&) const {
    // Intentionally left empty.
    // This method should be overwritten in case there are transient variables (e.g. JANI).
}

template<typename ValueType, typename StateType>
void NextStateGenerator<ValueType, StateType>::postprocess(StateBehavior<ValueType, StateType>& result) {
    // If the model we build is a Markov Automaton, we postprocess the choices to sum all Markovian choices
    // and make the Markovian choice the very first one (if there is any).
    bool foundPreviousMarkovianChoice = false;
    if (this->getModelType() == ModelType::MA) {
        uint64_t numberOfChoicesToDelete = 0;

        for (uint_fast64_t index = 0; index + numberOfChoicesToDelete < result.getNumberOfChoices();) {
            Choice<ValueType>& choice = result.getChoices()[index];

            if (choice.isMarkovian()) {
                if (foundPreviousMarkovianChoice) {
                    // If there was a previous Markovian choice, we need to sum them. Note that we can assume
                    // that the previous Markovian choice is the very first one in the choices vector.
                    result.getChoices().front().add(choice);

                    // Swap the choice to the end to indicate it can be removed (if it's not already there).
                    if (index != result.getNumberOfChoices() - 1 - numberOfChoicesToDelete) {
                        choice = std::move(result.getChoices()[result.getNumberOfChoices() - 1 - numberOfChoicesToDelete]);
                    }
                    ++numberOfChoicesToDelete;
                } else {
                    // If there is no previous Markovian choice, just move the Markovian choice to the front.
                    if (index != 0) {
                        std::swap(result.getChoices().front(), choice);
                    }
                    foundPreviousMarkovianChoice = true;
                    ++index;
                }
            } else {
                ++index;
            }
        }

        // Finally remove the choices that were added to other Markovian choices.
        if (numberOfChoicesToDelete > 0) {
            result.getChoices().resize(result.getChoices().size() - numberOfChoicesToDelete);
        }
    }
}

template<typename ValueType, typename StateType>
std::string NextStateGenerator<ValueType, StateType>::stateToString(CompressedState const& state) const {
    return toString(state, variableInformation);
}

template<typename ValueType, typename StateType>
storm::json<ValueType> NextStateGenerator<ValueType, StateType>::currentStateToJson(bool onlyObservable) const {
    storm::json<BaseValueType> result = unpackStateIntoJson<BaseValueType>(*state, variableInformation, onlyObservable);
    extendStateInformation(result);
    return result;
}

template<typename ValueType, typename StateType>
storm::expressions::SimpleValuation NextStateGenerator<ValueType, StateType>::currentStateToSimpleValuation() const {
    return unpackStateIntoValuation(*state, variableInformation, *expressionManager);
}

template<typename ValueType, typename StateType>
void NextStateGenerator<ValueType, StateType>::extendStateInformation(storm::json<BaseValueType>&) const {
    // Intentionally left empty.
}

template<typename ValueType, typename StateType>
std::shared_ptr<storm::storage::sparse::ChoiceOrigins> NextStateGenerator<ValueType, StateType>::generateChoiceOrigins(
    std::vector<boost::any>& /*dataForChoiceOrigins*/) const {
    STORM_LOG_ERROR_COND(!options.isBuildChoiceOriginsSet(), "Generating choice origins is not supported for the considered model format.");
    return nullptr;
}

template<typename ValueType, typename StateType>
uint32_t NextStateGenerator<ValueType, StateType>::observabilityClass(CompressedState const& state) const {
    if (this->mask.size() == 0) {
        this->mask = computeObservabilityMask(variableInformation);
    }
    uint32_t classId = unpackStateToObservabilityClass(state, evaluateObservationLabels(state), observabilityMap, mask);
    return classId;
}

template<typename ValueType, typename StateType>
std::map<std::string, storm::storage::PlayerIndex> NextStateGenerator<ValueType, StateType>::getPlayerNameToIndexMap() const {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Generating player mappings is not supported for this model input format");
}

template<typename ValueType, typename StateType>
void NextStateGenerator<ValueType, StateType>::remapStateIds(std::function<StateType(StateType const&)> const& /*remapping*/) {
    if (overlappingGuardStates != boost::none) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                        "Remapping of Ids during model building is not supported for overlapping guard statements.");
    }
    // Nothing to be done.
}

template class ActionMask<double>;
template class StateValuationFunctionMask<double>;
template class NextStateGenerator<double>;

template class ActionMask<storm::RationalNumber>;
template class StateValuationFunctionMask<storm::RationalNumber>;
template class NextStateGenerator<storm::RationalNumber>;

template class ActionMask<storm::RationalFunction>;
template class StateValuationFunctionMask<storm::RationalFunction>;
template class NextStateGenerator<storm::RationalFunction>;

template class ActionMask<storm::Interval>;
template class StateValuationFunctionMask<storm::Interval>;
template class NextStateGenerator<storm::Interval>;

template class ActionMask<storm::RationalInterval>;
template class StateValuationFunctionMask<storm::RationalInterval>;
template class NextStateGenerator<storm::RationalInterval>;
}  // namespace generator
}  // namespace storm
