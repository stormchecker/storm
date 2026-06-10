#include "storm/generator/PrismNextStateGenerator.h"

#include <boost/any.hpp>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/generator/Distribution.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/storage/sparse/PrismChoiceOrigins.h"
#include "storm/utility/combinatorics.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace generator {

template<typename ValueType, typename StateType>
PrismNextStateGenerator<ValueType, StateType>::PrismNextStateGenerator(storm::prism::Program const& program, NextStateGeneratorOptions const& options,
                                                                       std::shared_ptr<ActionMask<ValueType, StateType>> const& mask)
    : PrismNextStateGenerator<ValueType, StateType>(program.substituteConstantsFormulas(), options, mask, false) {
    // Intentionally left empty.
}

template<typename ValueType, typename StateType>
PrismNextStateGenerator<ValueType, StateType>::PrismNextStateGenerator(storm::prism::Program const& program, NextStateGeneratorOptions const& options,
                                                                       std::shared_ptr<ActionMask<ValueType, StateType>> const& mask, bool)
    : NextStateGenerator<ValueType, StateType>(program.getManager(), options, mask), program(program), rewardModels(), hasStateActionRewards(false) {
    STORM_LOG_TRACE("Creating next-state generator for PRISM program: " << program);
    STORM_LOG_THROW(!this->program.specifiesSystemComposition(), storm::exceptions::WrongFormatException,
                    "The explicit next-state generator currently does not support custom system compositions.");
    STORM_LOG_THROW(storm::IsIntervalType<ValueType> || !this->program.hasIntervalUpdates(), storm::exceptions::WrongFormatException,
                    "The provided PRISM program contains interval updates, which are not supported by the chosen value type.");

    // Only after checking validity of the program, we initialize the variable information.
    this->checkValid();
    this->variableInformation = VariableInformation(program, options.getReservedBitsForUnboundedVariables(), options.isAddOutOfBoundsStateSet());
    this->initializeSpecialStates();

    // Create a proper evaluator.
    this->evaluator = std::make_unique<storm::expressions::ExpressionEvaluator<BaseValueType>>(program.getManager());

    if (this->options.isBuildAllRewardModelsSet()) {
        for (auto const& rewardModel : this->program.getRewardModels()) {
            rewardModels.push_back(rewardModel);
        }
    } else {
        // Extract the reward models from the program based on the names we were given.
        for (auto const& rewardModelName : this->options.getRewardModelNames()) {
            if (this->program.hasRewardModel(rewardModelName)) {
                rewardModels.push_back(this->program.getRewardModel(rewardModelName));
            } else {
                STORM_LOG_THROW(rewardModelName.empty(), storm::exceptions::InvalidArgumentException,
                                "Cannot build unknown reward model '" << rewardModelName << "'.");
                STORM_LOG_THROW(this->program.getNumberOfRewardModels() == 1, storm::exceptions::InvalidArgumentException,
                                "Reference to standard reward model is ambiguous.");
            }
        }

        // If no reward model was yet added, but there was one that was given in the options, we try to build the
        // standard reward model.
        if (rewardModels.empty() && !this->options.getRewardModelNames().empty()) {
            rewardModels.push_back(this->program.getRewardModel(0));
        }
    }

    // Determine whether any reward model has state action rewards.
    for (auto const& rewardModel : rewardModels) {
        hasStateActionRewards |= rewardModel.get().hasStateActionRewards();
    }

    // If there are terminal states we need to handle, we now need to translate all labels to expressions.
    if (this->options.hasTerminalStates()) {
        for (auto const& expressionOrLabelAndBool : this->options.getTerminalStates()) {
            if (expressionOrLabelAndBool.first.isExpression()) {
                this->terminalStates.push_back(std::make_pair(expressionOrLabelAndBool.first.getExpression(), expressionOrLabelAndBool.second));
            } else {
                if (program.hasLabel(expressionOrLabelAndBool.first.getLabel())) {
                    this->terminalStates.push_back(
                        std::make_pair(this->program.getLabelExpression(expressionOrLabelAndBool.first.getLabel()), expressionOrLabelAndBool.second));
                } else {
                    // If the label is not present in the program and is not a special one, we raise an error.
                    STORM_LOG_THROW(this->isSpecialLabel(expressionOrLabelAndBool.first.getLabel()), storm::exceptions::InvalidArgumentException,
                                    "Terminal states refer to illegal label '" << expressionOrLabelAndBool.first.getLabel() << "'.");
                }
            }
        }
    }

    if (program.getModelType() == storm::prism::Program::ModelType::SMG) {
        moduleIndexToPlayerIndexMap = program.buildModuleIndexToPlayerIndexMap();
        actionIndexToPlayerIndexMap = program.buildActionIndexToPlayerIndexMap();
    }
}

template<typename ValueType, typename StateType>
bool PrismNextStateGenerator<ValueType, StateType>::canHandle(storm::prism::Program const& program) {
    // We can handle all valid prism programs (except for PTAs)
    return program.getModelType() != storm::prism::Program::ModelType::PTA;
}

template<typename ValueType, typename StateType>
void PrismNextStateGenerator<ValueType, StateType>::checkValid() const {
    // If the program still contains undefined constants and we are not in a parametric setting, assemble an appropriate error message.
    if (!std::is_same<ValueType, storm::RationalFunction>::value && program.hasUndefinedConstants()) {
        std::vector<std::reference_wrapper<storm::prism::Constant const>> undefinedConstants = program.getUndefinedConstants();
        std::stringstream stream;
        bool printComma = false;
        for (auto const& constant : undefinedConstants) {
            if (printComma) {
                stream << ", ";
            } else {
                printComma = true;
            }
            stream << constant.get().getName() << " (" << constant.get().getType() << ")";
        }
        stream << ".";
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Program still contains these undefined constants: " + stream.str());
    } else if (std::is_same<ValueType, storm::RationalFunction>::value && !program.undefinedConstantsAreGraphPreserving()) {
        auto undef = program.getUndefinedConstantsAsString();
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                        "The program contains undefined constants that appear in some places other than update probabilities and reward value expressions, "
                        "which is not admitted. Undefined constants are: "
                            << undef);
    }
}

template<typename ValueType, typename StateType>
ModelType PrismNextStateGenerator<ValueType, StateType>::getModelType() const {
    switch (program.getModelType()) {
        case storm::prism::Program::ModelType::DTMC:
            return ModelType::DTMC;
        case storm::prism::Program::ModelType::CTMC:
            return ModelType::CTMC;
        case storm::prism::Program::ModelType::MDP:
            return ModelType::MDP;
        case storm::prism::Program::ModelType::MA:
            return ModelType::MA;
        case storm::prism::Program::ModelType::POMDP:
            return ModelType::POMDP;
        case storm::prism::Program::ModelType::SMG:
            return ModelType::SMG;
        default:
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Invalid model type.");
    }
}

template<typename ValueType, typename StateType>
bool PrismNextStateGenerator<ValueType, StateType>::isDeterministicModel() const {
    return program.isDeterministicModel();
}

template<typename ValueType, typename StateType>
bool PrismNextStateGenerator<ValueType, StateType>::isDiscreteTimeModel() const {
    return program.isDiscreteTimeModel();
}

template<typename ValueType, typename StateType>
bool PrismNextStateGenerator<ValueType, StateType>::isPartiallyObservable() const {
    return program.isPartiallyObservable();
}

template<typename ValueType, typename StateType>
std::vector<StateType> PrismNextStateGenerator<ValueType, StateType>::getInitialStates(StateToIdCallback const& stateToIdCallback) {
    std::vector<StateType> initialStateIndices;

    // If all states are initial, we can simplify the enumeration substantially.
    if (program.hasInitialConstruct() && program.getInitialStatesExpression().isTrue()) {
        // Create vectors holding all possible values
        std::vector<std::vector<uint64_t>> allValues;
        for (auto const& intVar : this->variableInformation.integerVariables) {
            STORM_LOG_ASSERT(intVar.lowerBound <= intVar.upperBound, "Expecting variable with non-empty set of possible values.");
            // The value of integer variables is shifted so that 0 is always the smallest possible value
            allValues.push_back(storm::utility::vector::buildVectorForRange<uint64_t>(static_cast<uint64_t>(0), intVar.upperBound + 1 - intVar.lowerBound));
        }
        uint64_t intEndIndex = allValues.size();
        // For boolean variables we consider the values 0 and 1.
        allValues.resize(allValues.size() + this->variableInformation.booleanVariables.size(),
                         std::vector<uint64_t>({static_cast<uint64_t>(0), static_cast<uint64_t>(1)}));

        std::vector<std::vector<uint64_t>::const_iterator> its;
        std::vector<std::vector<uint64_t>::const_iterator> ites;
        for (auto const& valVec : allValues) {
            its.push_back(valVec.cbegin());
            ites.push_back(valVec.cend());
        }

        // Now create an initial state for each combination of values
        CompressedState initialState(this->variableInformation.getTotalBitOffset(true));
        storm::utility::combinatorics::forEach(
            its, ites,
            [this, &initialState, &intEndIndex](uint64_t index, uint64_t value) {
                // Set the value for the variable corresponding to the given index
                if (index < intEndIndex) {
                    // Integer variable
                    auto const& intVar = this->variableInformation.integerVariables[index];
                    initialState.setFromInt(intVar.bitOffset, intVar.bitWidth, value);
                } else {
                    // Boolean variable
                    STORM_LOG_ASSERT(index - intEndIndex < this->variableInformation.booleanVariables.size(), "Unexpected index");
                    auto const& boolVar = this->variableInformation.booleanVariables[index - intEndIndex];
                    STORM_LOG_ASSERT(value <= 1u, "Unexpected value for boolean variable.");
                    initialState.set(boolVar.bitOffset, static_cast<bool>(value));
                }
            },
            [&stateToIdCallback, &initialStateIndices, &initialState]() {
                // Register initial state.
                StateType id = stateToIdCallback(initialState);
                initialStateIndices.push_back(id);
                return true;  // Keep on exploring
            });
        STORM_LOG_DEBUG("Enumerated " << initialStateIndices.size() << " initial states using brute force enumeration.");
    } else {
        // Prepare an SMT solver to enumerate all initial states.
        storm::utility::solver::SmtSolverFactory factory;
        std::unique_ptr<storm::solver::SmtSolver> solver = factory.create(program.getManager());

        std::vector<storm::expressions::Expression> rangeExpressions = program.getAllRangeExpressions();
        for (auto const& expression : rangeExpressions) {
            solver->add(expression);
        }
        solver->add(program.getInitialStatesExpression());

        // Proceed ss long as the solver can still enumerate initial states.
        while (solver->check() == storm::solver::SmtSolver::CheckResult::Sat) {
            // Create fresh state.
            CompressedState initialState(this->variableInformation.getTotalBitOffset(true));

            // Read variable assignment from the solution of the solver. Also, create an expression we can use to
            // prevent the variable assignment from being enumerated again.
            storm::expressions::Expression blockingExpression;
            std::shared_ptr<storm::solver::SmtSolver::ModelReference> model = solver->getModel();
            for (auto const& booleanVariable : this->variableInformation.booleanVariables) {
                bool variableValue = model->getBooleanValue(booleanVariable.variable);
                storm::expressions::Expression localBlockingExpression = variableValue ? !booleanVariable.variable : booleanVariable.variable;
                blockingExpression = blockingExpression.isInitialized() ? blockingExpression || localBlockingExpression : localBlockingExpression;
                initialState.set(booleanVariable.bitOffset, variableValue);
            }
            for (auto const& integerVariable : this->variableInformation.integerVariables) {
                int_fast64_t variableValue = model->getIntegerValue(integerVariable.variable);
                storm::expressions::Expression localBlockingExpression = integerVariable.variable != model->getManager().integer(variableValue);
                blockingExpression = blockingExpression.isInitialized() ? blockingExpression || localBlockingExpression : localBlockingExpression;
                initialState.setFromInt(integerVariable.bitOffset, integerVariable.bitWidth,
                                        static_cast<uint_fast64_t>(variableValue - integerVariable.lowerBound));
            }

            // Register initial state and return it.
            StateType id = stateToIdCallback(initialState);
            initialStateIndices.push_back(id);

            // Block the current initial state to search for the next one.
            if (!blockingExpression.isInitialized()) {
                break;
            }
            solver->add(blockingExpression);
        }

        STORM_LOG_DEBUG("Enumerated " << initialStateIndices.size() << " initial states using SMT solving.");
    }

    return initialStateIndices;
}

template<typename ValueType, typename StateType>
StateBehavior<ValueType, StateType> PrismNextStateGenerator<ValueType, StateType>::expand(StateToIdCallback const& stateToIdCallback) {
    // Prepare the result, in case we return early.
    StateBehavior<ValueType, StateType> result;

    // First, construct the state rewards, as we may return early if there are no choices later and we already
    // need the state rewards then.
    for (auto const& rewardModel : rewardModels) {
        ValueType stateRewardValue = storm::utility::zero<ValueType>();
        if (rewardModel.get().hasStateRewards()) {
            for (auto const& stateReward : rewardModel.get().getStateRewards()) {
                if (this->evaluator->asBool(stateReward.getStatePredicateExpression())) {
                    stateRewardValue += ValueType(this->evaluator->asRational(stateReward.getRewardValueExpression()));
                }
            }
        }
        result.addStateReward(stateRewardValue);
    }

    // If a terminal expression was set, we must not expand this state
    if (!this->terminalStates.empty()) {
        for (auto const& expressionBool : this->terminalStates) {
            if (this->evaluator->asBool(expressionBool.first) == expressionBool.second) {
                if (!isPartiallyObservable()) {
                    // If the model is not partially observable, return.
                    return result;
                } else {
                    // for partially observable models, we need to add self-loops for all enabled actions.
                    result.setExpanded();
                    std::vector<Choice<ValueType>> allChoices = getSelfLoopsForAllActions(*this->state, stateToIdCallback);
                    if (allChoices.size() != 0) {
                        for (auto& choice : allChoices) {
                            result.addChoice(std::move(choice));
                        }

                        this->postprocess(result);
                    }
                    return result;
                }
            }
        }
    }

    // Get all choices for the state.
    result.setExpanded();

    std::vector<Choice<ValueType>> allChoices;
    if (this->getOptions().isApplyMaximalProgressAssumptionSet()) {
        // First explore only edges without a rate
        allChoices = getAsynchronousChoices(*this->state, stateToIdCallback, CommandFilter::Probabilistic);
        addSynchronousChoices(allChoices, *this->state, stateToIdCallback, CommandFilter::Probabilistic);
        if (allChoices.empty()) {
            // Expand the Markovian edges if there are no probabilistic ones.
            allChoices = getAsynchronousChoices(*this->state, stateToIdCallback, CommandFilter::Markovian);
            addSynchronousChoices(allChoices, *this->state, stateToIdCallback, CommandFilter::Markovian);
        }
    } else {
        allChoices = getAsynchronousChoices(*this->state, stateToIdCallback);
        addSynchronousChoices(allChoices, *this->state, stateToIdCallback);
    }

    // If there is not a single choice, we return immediately, because the state has no behavior (other than
    // the state reward).
    if (allChoices.empty()) {
        return result;
    }

    // If the model is a deterministic model, we need to fuse the choices into one.
    if (this->isDeterministicModel() && allChoices.size() > 1) {
        Choice<ValueType> globalChoice;

        if (this->options.isAddOverlappingGuardLabelSet()) {
            this->overlappingGuardStates->push_back(stateToIdCallback(*this->state));
        }

        // For CTMCs, we need to keep track of the total exit rate to scale the action rewards later. For DTMCs
        // this is equal to the number of choices, which is why we initialize it like this here.
        ValueType const totalNumberOfChoices = storm::utility::convertNumber<ValueType, uint64_t>(allChoices.size());
        ValueType totalExitRate = this->isDiscreteTimeModel() ? totalNumberOfChoices : storm::utility::zero<ValueType>();

        // Iterate over all choices and combine the probabilities/rates into one choice.
        for (auto const& choice : allChoices) {
            for (auto const& stateProbabilityPair : choice) {
                if (this->isDiscreteTimeModel()) {
                    globalChoice.addProbability(stateProbabilityPair.first, stateProbabilityPair.second / totalNumberOfChoices);
                } else {
                    globalChoice.addProbability(stateProbabilityPair.first, stateProbabilityPair.second);
                }
            }

            if (hasStateActionRewards && !this->isDiscreteTimeModel()) {
                totalExitRate += choice.getTotalMass();
            }

            if (this->options.isBuildChoiceLabelsSet() && choice.hasLabels()) {
                globalChoice.addLabels(choice.getLabels());
            }

            if (this->options.isBuildChoiceOriginsSet() && choice.hasOriginData()) {
                globalChoice.addOriginData(choice.getOriginData());
            }
        }

        // Now construct the state-action reward for all selected reward models.
        for (auto const& rewardModel : rewardModels) {
            ValueType stateActionRewardValue = storm::utility::zero<ValueType>();
            if (rewardModel.get().hasStateActionRewards()) {
                for (auto const& stateActionReward : rewardModel.get().getStateActionRewards()) {
                    for (auto const& choice : allChoices) {
                        if (stateActionReward.getActionIndex() == choice.getActionIndex() &&
                            this->evaluator->asBool(stateActionReward.getStatePredicateExpression())) {
                            stateActionRewardValue +=
                                ValueType(this->evaluator->asRational(stateActionReward.getRewardValueExpression())) * choice.getTotalMass();
                        }
                    }
                }
            }
            if (hasStateActionRewards) {
                globalChoice.addReward(stateActionRewardValue / totalExitRate);
            }
        }

        // Move the newly fused choice in place.
        allChoices.clear();
        allChoices.push_back(std::move(globalChoice));
    }

    // For SMG we check whether the state has a unique player
    if (program.getModelType() == storm::prism::Program::ModelType::SMG && allChoices.size() > 1) {
        auto choiceIt = allChoices.begin();
        STORM_LOG_ASSERT(choiceIt->hasPlayerIndex(),
                         "State '" << this->stateToString(*this->state)
                                   << "' features a choice without player index.");  // This should have been catched while creating the choice already
        storm::storage::PlayerIndex statePlayerIndex = choiceIt->getPlayerIndex();
        STORM_LOG_ASSERT(statePlayerIndex != storm::storage::INVALID_PLAYER_INDEX,
                         "State '" << this->stateToString(*this->state)
                                   << "' features a choice with invalid player index.");  // This should have been catched while creating the choice already
        for (++choiceIt; choiceIt != allChoices.end(); ++choiceIt) {
            STORM_LOG_ASSERT(choiceIt->hasPlayerIndex(),
                             "State '" << this->stateToString(*this->state)
                                       << "' features a choice without player index.");  // This should have been catched while creating the choice already
            STORM_LOG_ASSERT(choiceIt->getPlayerIndex() != storm::storage::INVALID_PLAYER_INDEX,
                             "State '" << this->stateToString(*this->state)
                                       << "' features a choice with invalid player index.");  // This should have been catched while creating the choice already
            STORM_LOG_THROW(statePlayerIndex == choiceIt->getPlayerIndex(), storm::exceptions::WrongFormatException,
                            "The player for state '" << this->stateToString(*this->state) << "' is not unique. At least one choice is owned by player '"
                                                     << statePlayerIndex << "' while another is owned by player '" << choiceIt->getPlayerIndex() << "'.");
        }
    }

    // Move all remaining choices in place.
    for (auto& choice : allChoices) {
        result.addChoice(std::move(choice));
    }

    this->postprocess(result);

    return result;
}

template<typename ValueType, typename StateType>
bool PrismNextStateGenerator<ValueType, StateType>::evaluateBooleanExpressionInCurrentState(expressions::Expression const& expr) const {
    return this->evaluator->asBool(expr);
}

template<typename ValueType, typename StateType>
CompressedState PrismNextStateGenerator<ValueType, StateType>::applyUpdate(CompressedState const& state, storm::prism::Update const& update) {
    CompressedState newState(state);

    // NOTE: the following process assumes that the assignments of the update are ordered in such a way that the
    // assignments to boolean variables precede the assignments to all integer variables and that within the
    // types, the assignments to variables are ordered (in ascending order) by the expression variables.
    // This is guaranteed for PRISM models, by sorting the assignments as soon as an update is created.

    auto assignmentIt = update.getAssignments().begin();
    auto assignmentIte = update.getAssignments().end();

    // Iterate over all boolean assignments and carry them out.
    auto boolIt = this->variableInformation.booleanVariables.begin();
    for (; assignmentIt != assignmentIte && assignmentIt->getExpression().hasBooleanType(); ++assignmentIt) {
        while (assignmentIt->getVariable() != boolIt->variable) {
            ++boolIt;
        }
        newState.set(boolIt->bitOffset, this->evaluator->asBool(assignmentIt->getExpression()));
    }

    // Iterate over all integer assignments and carry them out.
    auto integerIt = this->variableInformation.integerVariables.begin();
    for (; assignmentIt != assignmentIte && assignmentIt->getExpression().hasIntegerType(); ++assignmentIt) {
        while (assignmentIt->getVariable() != integerIt->variable) {
            ++integerIt;
        }
        int_fast64_t assignedValue = this->evaluator->asInt(assignmentIt->getExpression());
        if (this->options.isAddOutOfBoundsStateSet()) {
            if (assignedValue < integerIt->lowerBound || assignedValue > integerIt->upperBound) {
                return this->outOfBoundsState;
            }
        } else if (integerIt->forceOutOfBoundsCheck || this->options.isExplorationChecksSet()) {
            STORM_LOG_THROW(assignedValue >= integerIt->lowerBound, storm::exceptions::WrongFormatException,
                            "The update " << update << " leads to an out-of-bounds value (" << assignedValue << ") for the variable '"
                                          << assignmentIt->getVariableName() << "'.");
            STORM_LOG_THROW(assignedValue <= integerIt->upperBound, storm::exceptions::WrongFormatException,
                            "The update " << update << " leads to an out-of-bounds value (" << assignedValue << ") for the variable '"
                                          << assignmentIt->getVariableName() << "'.");
        }
        newState.setFromInt(integerIt->bitOffset, integerIt->bitWidth, assignedValue - integerIt->lowerBound);
        STORM_LOG_ASSERT(static_cast<int_fast64_t>(newState.getAsInt(integerIt->bitOffset, integerIt->bitWidth)) + integerIt->lowerBound == assignedValue,
                         "Writing to the bit vector bucket failed (read " << newState.getAsInt(integerIt->bitOffset, integerIt->bitWidth) << " but wrote "
                                                                          << assignedValue << ").");
    }

    // Check that we processed all assignments.
    STORM_LOG_ASSERT(assignmentIt == assignmentIte, "Not all assignments were consumed.");

    return newState;
}

struct ActiveCommandData {
    ActiveCommandData(storm::prism::Module const* modulePtr, std::set<uint_fast64_t> const* commandIndicesPtr,
                      typename std::set<uint_fast64_t>::const_iterator currentCommandIndexIt)
        : modulePtr(modulePtr), commandIndicesPtr(commandIndicesPtr), currentCommandIndexIt(currentCommandIndexIt) {
        // Intentionally left empty
    }
    storm::prism::Module const* modulePtr;
    std::set<uint_fast64_t> const* commandIndicesPtr;
    typename std::set<uint_fast64_t>::const_iterator currentCommandIndexIt;
};

template<typename ValueType, typename StateType>
boost::optional<std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>>>
PrismNextStateGenerator<ValueType, StateType>::getActiveCommandsByActionIndex(uint_fast64_t const& actionIndex, CommandFilter const& commandFilter) {
    // First check whether there is at least one enabled command at each module
    // This avoids evaluating unnecessarily many guards.
    // If we find one module without an enabled command, we return boost::none.
    // At the same time, we store pointers to the relevant modules, the relevant command sets and the first enabled command within each set.

    // Iterate over all modules.
    std::vector<ActiveCommandData> activeCommands;
    for (uint_fast64_t i = 0; i < program.getNumberOfModules(); ++i) {
        storm::prism::Module const& module = program.getModule(i);

        // If the module has no command labeled with the given action, we can skip this module.
        if (!module.hasActionIndex(actionIndex)) {
            continue;
        }

        std::set<uint_fast64_t> const& commandIndices = module.getCommandIndicesByActionIndex(actionIndex);

        // If the module contains the action, but there is no command in the module that is labeled with
        // this action, we don't have any feasible command combinations.
        if (commandIndices.empty()) {
            return boost::none;
        }

        // Look up commands by their indices and check if the guard evaluates to true in the given state.
        bool hasOneEnabledCommand = false;
        for (auto commandIndexIt = commandIndices.begin(), commandIndexIte = commandIndices.end(); commandIndexIt != commandIndexIte; ++commandIndexIt) {
            storm::prism::Command const& command = module.getCommand(*commandIndexIt);
            if (!isCommandPotentiallySynchronizing(command)) {
                continue;
            }
            if (commandFilter != CommandFilter::All) {
                STORM_LOG_ASSERT(commandFilter == CommandFilter::Markovian || commandFilter == CommandFilter::Probabilistic, "Unexpected command filter.");
                if ((commandFilter == CommandFilter::Markovian) != command.isMarkovian()) {
                    continue;
                }
            }
            if (this->evaluator->asBool(command.getGuardExpression())) {
                // Found the first enabled command for this module.
                hasOneEnabledCommand = true;
                activeCommands.emplace_back(&module, &commandIndices, commandIndexIt);
                break;
            }
        }

        if (!hasOneEnabledCommand) {
            return boost::none;
        }
    }

    // If we reach this point, there has to be at least one active command for each relevant module.
    std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>> result;

    // Iterate over all command sets.
    for (auto const& activeCommand : activeCommands) {
        std::vector<std::reference_wrapper<storm::prism::Command const>> commands;

        auto commandIndexIt = activeCommand.currentCommandIndexIt;
        // The command at the current position is already known to be enabled
        commands.push_back(activeCommand.modulePtr->getCommand(*commandIndexIt));

        // Look up commands by their indices and add them if the guard evaluates to true in the given state.
        auto commandIndexIte = activeCommand.commandIndicesPtr->end();
        for (++commandIndexIt; commandIndexIt != commandIndexIte; ++commandIndexIt) {
            storm::prism::Command const& command = activeCommand.modulePtr->getCommand(*commandIndexIt);
            if (commandFilter != CommandFilter::All) {
                STORM_LOG_ASSERT(commandFilter == CommandFilter::Markovian || commandFilter == CommandFilter::Probabilistic, "Unexpected command filter.");
                if ((commandFilter == CommandFilter::Markovian) != command.isMarkovian()) {
                    continue;
                }
            }
            if (this->evaluator->asBool(command.getGuardExpression())) {
                commands.push_back(command);
            }
        }

        result.push_back(std::move(commands));
    }

    STORM_LOG_ASSERT(!result.empty(), "Expected non-empty list.");
    return result;
}

template<typename ValueType, typename BaseValueType>
ValueType evaluateLikelihoodExpression(storm::prism::Update const& update, storm::expressions::ExpressionEvaluator<BaseValueType> const& evaluator) {
    if constexpr (storm::IsIntervalType<ValueType>) {
        if (update.isLikelihoodInterval()) {
            BaseValueType lower = evaluator.asRational(update.getLikelihoodExpressionInterval().first);
            BaseValueType upper = evaluator.asRational(update.getLikelihoodExpressionInterval().second);
            return ValueType(lower, carl::BoundType::WEAK, upper, carl::BoundType::WEAK);
        } else {
            return evaluator.asRational(update.getLikelihoodExpression());
        }
    } else {
        // We don't expect to see an interval likelihood now. This case should have been catched earlier.
        STORM_LOG_ASSERT(!update.isLikelihoodInterval(), "Unexpected interval likelihood for non-interval model in update " << update << ".");
        return evaluator.asRational(update.getLikelihoodExpression());
    }
}

template<typename ValueType, typename StateType>
std::vector<Choice<ValueType>> PrismNextStateGenerator<ValueType, StateType>::getAsynchronousChoices(CompressedState const& state,
                                                                                                     StateToIdCallback stateToIdCallback,
                                                                                                     CommandFilter const& commandFilter) {
    std::vector<Choice<ValueType>> result;

    // Iterate over all modules.
    for (uint_fast64_t i = 0; i < program.getNumberOfModules(); ++i) {
        storm::prism::Module const& module = program.getModule(i);

        // Iterate over all commands.
        for (uint_fast64_t j = 0; j < module.getNumberOfCommands(); ++j) {
            storm::prism::Command const& command = module.getCommand(j);

            // Only consider commands that are not possibly synchronizing.
            if (isCommandPotentiallySynchronizing(command))
                continue;

            if (commandFilter != CommandFilter::All) {
                STORM_LOG_ASSERT(commandFilter == CommandFilter::Markovian || commandFilter == CommandFilter::Probabilistic, "Unexpected command filter.");
                if ((commandFilter == CommandFilter::Markovian) != command.isMarkovian()) {
                    continue;
                }
            }
            if (this->actionMask != nullptr) {
                if (!this->actionMask->query(*this, command.getActionIndex())) {
                    continue;
                }
            }

            // Skip the command, if it is not enabled.
            if (!this->evaluator->asBool(command.getGuardExpression())) {
                continue;
            }

            result.push_back(Choice<ValueType>(command.getActionIndex(), command.isMarkovian()));
            Choice<ValueType>& choice = result.back();

            // Remember the choice origin only if we were asked to.
            if (this->options.isBuildChoiceOriginsSet()) {
                CommandSet commandIndex{command.getGlobalIndex()};
                choice.addOriginData(boost::any(std::move(commandIndex)));
            }

            // Iterate over all updates of the current command.
            ValueType probabilitySum = storm::utility::zero<ValueType>();
            for (uint_fast64_t k = 0; k < command.getNumberOfUpdates(); ++k) {
                storm::prism::Update const& update = command.getUpdate(k);

                ValueType probability = evaluateLikelihoodExpression<ValueType>(update, *this->evaluator);
                if (probability != storm::utility::zero<ValueType>()) {
                    // Obtain target state index and add it to the list of known states. If it has not yet been
                    // seen, we also add it to the set of states that have yet to be explored.
                    StateType stateIndex = stateToIdCallback(applyUpdate(state, update));

                    // Update the choice by adding the probability/target state to it.
                    choice.addProbability(stateIndex, probability);
                    if (this->options.isExplorationChecksSet()) {
                        if constexpr (!std::is_same_v<ValueType, storm::RationalFunction>) {
                            STORM_LOG_THROW(probability > storm::utility::zero<ValueType>(), storm::exceptions::WrongFormatException,
                                            "Probability expression in update '" << update << " evaluates to negative value " << probability << ".");
                            STORM_LOG_THROW(!program.isDiscreteTimeModel() || probability <= storm::utility::one<ValueType>(),
                                            storm::exceptions::WrongFormatException,
                                            "Probability expression in update '" << update << " evaluates to value " << probability << " >1.");
                        }
                        probabilitySum += probability;
                    }
                }
            }

            // Create the state-action reward for the newly created choice.
            for (auto const& rewardModel : rewardModels) {
                ValueType stateActionRewardValue = storm::utility::zero<ValueType>();
                if (rewardModel.get().hasStateActionRewards()) {
                    for (auto const& stateActionReward : rewardModel.get().getStateActionRewards()) {
                        if (stateActionReward.getActionIndex() == choice.getActionIndex() &&
                            this->evaluator->asBool(stateActionReward.getStatePredicateExpression())) {
                            stateActionRewardValue += ValueType(this->evaluator->asRational(stateActionReward.getRewardValueExpression()));
                        }
                    }
                }
                choice.addReward(stateActionRewardValue);
            }

            if (this->options.isBuildChoiceLabelsSet() && command.isLabeled()) {
                choice.addLabel(program.getActionName(command.getActionIndex()));
            }

            if (program.getModelType() == storm::prism::Program::ModelType::SMG) {
                if (command.getActionIndex() == 0) {
                    // Unlabeled command
                    auto const playerOfModule = moduleIndexToPlayerIndexMap.at(i);
                    STORM_LOG_THROW(playerOfModule != storm::storage::INVALID_PLAYER_INDEX, storm::exceptions::WrongFormatException,
                                    "Module " << module.getName() << " is not owned by any player but has at least one enabled, unlabeled command.");
                    choice.setPlayerIndex(playerOfModule);
                } else {
                    // Labelled command (that happens to not synchronize with another command
                    auto const playerOfAction = actionIndexToPlayerIndexMap.at(command.getActionIndex());
                    STORM_LOG_THROW(playerOfAction != storm::storage::INVALID_PLAYER_INDEX, storm::exceptions::WrongFormatException,
                                    "Command with action label '" << program.getActionName(command.getActionIndex()) << "' is not owned by any player.");
                    choice.setPlayerIndex(playerOfAction);
                }
            }

            if (this->options.isExplorationChecksSet()) {
                // Check that the resulting distribution is in fact a distribution.
                STORM_LOG_THROW(!program.isDiscreteTimeModel() || !storm::utility::isConstant(probabilitySum) || this->comparator.isOne(probabilitySum),
                                storm::exceptions::WrongFormatException,
                                "Probabilities do not sum to one for command '" << command << "' (actually sum to " << probabilitySum << ").");
            }
        }
    }

    return result;
}

template<typename ValueType, typename StateType>
std::vector<Choice<ValueType>> PrismNextStateGenerator<ValueType, StateType>::getSelfLoopsForAllActions(CompressedState const& state,
                                                                                                        StateToIdCallback stateToIdCallback,
                                                                                                        CommandFilter const& commandFilter) {
    std::vector<Choice<ValueType>> result;

    // Asynchronous actions
    // Iterate over all modules.
    for (uint_fast64_t i = 0; i < program.getNumberOfModules(); ++i) {
        storm::prism::Module const& module = program.getModule(i);

        // Iterate over all commands.
        for (uint_fast64_t j = 0; j < module.getNumberOfCommands(); ++j) {
            storm::prism::Command const& command = module.getCommand(j);

            // Only consider commands that are not possibly synchronizing.
            if (isCommandPotentiallySynchronizing(command))
                continue;

            if (this->actionMask != nullptr) {
                if (!this->actionMask->query(*this, command.getActionIndex())) {
                    continue;
                }
            }

            // Skip the command, if it is not enabled.
            if (!this->evaluator->asBool(command.getGuardExpression())) {
                continue;
            }

            result.push_back(Choice<ValueType>(command.getActionIndex(), command.isMarkovian()));
            Choice<ValueType>& choice = result.back();

            // Remember the choice origin only if we were asked to.
            if (this->options.isBuildChoiceOriginsSet()) {
                CommandSet commandIndex{command.getGlobalIndex()};
                choice.addOriginData(boost::any(std::move(commandIndex)));
            }
            choice.addProbability(stateToIdCallback(*this->state), storm::utility::one<ValueType>());

            // Create the state-action reward for the newly created choice.
            for (auto const& rewardModel : rewardModels) {
                ValueType stateActionRewardValue = storm::utility::zero<ValueType>();
                if (rewardModel.get().hasStateActionRewards()) {
                    for (auto const& stateActionReward : rewardModel.get().getStateActionRewards()) {
                        if (stateActionReward.getActionIndex() == choice.getActionIndex() &&
                            this->evaluator->asBool(stateActionReward.getStatePredicateExpression())) {
                            stateActionRewardValue += ValueType(this->evaluator->asRational(stateActionReward.getRewardValueExpression()));
                        }
                    }
                }
                choice.addReward(stateActionRewardValue);
            }

            if (this->options.isBuildChoiceLabelsSet() && command.isLabeled()) {
                choice.addLabel(program.getActionName(command.getActionIndex()));
            }
        }
    }

    // Synchronizing actions
    for (uint_fast64_t actionIndex : program.getSynchronizingActionIndices()) {
        if (this->actionMask != nullptr) {
            if (!this->actionMask->query(*this, actionIndex)) {
                continue;
            }
        }
        boost::optional<std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>>> optionalActiveCommandLists =
            getActiveCommandsByActionIndex(actionIndex, commandFilter);

        // Only process this action label, if there is at least one feasible solution.
        if (optionalActiveCommandLists) {
            std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>> const& activeCommandList = optionalActiveCommandLists.get();
            std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>::const_iterator> iteratorList(activeCommandList.size());

            // Initialize the list of iterators.
            for (size_t i = 0; i < activeCommandList.size(); ++i) {
                iteratorList[i] = activeCommandList[i].cbegin();
            }

            // As long as there is one feasible combination of commands, keep on expanding it.
            bool done = false;
            while (!done) {
                // At this point, we applied all commands of the current command combination and newTargetStates
                // contains all target states and their respective probabilities. That means we are now ready to
                // add the choice to the list of transitions.
                result.push_back(Choice<ValueType>(actionIndex));

                // Now create the actual distribution.
                Choice<ValueType>& choice = result.back();

                // Remember the choice label and origins only if we were asked to.
                if (this->options.isBuildChoiceLabelsSet()) {
                    choice.addLabel(program.getActionName(actionIndex));
                }
                if (this->options.isBuildChoiceOriginsSet()) {
                    CommandSet commandIndices;
                    for (uint_fast64_t i = 0; i < iteratorList.size(); ++i) {
                        commandIndices.insert(iteratorList[i]->get().getGlobalIndex());
                    }
                    choice.addOriginData(boost::any(std::move(commandIndices)));
                }
                choice.addProbability(stateToIdCallback(*this->state), storm::utility::one<ValueType>());

                // Create the state-action reward for the newly created choice.
                for (auto const& rewardModel : rewardModels) {
                    ValueType stateActionRewardValue = storm::utility::zero<ValueType>();
                    if (rewardModel.get().hasStateActionRewards()) {
                        for (auto const& stateActionReward : rewardModel.get().getStateActionRewards()) {
                            if (stateActionReward.getActionIndex() == choice.getActionIndex() &&
                                this->evaluator->asBool(stateActionReward.getStatePredicateExpression())) {
                                stateActionRewardValue += ValueType(this->evaluator->asRational(stateActionReward.getRewardValueExpression()));
                            }
                        }
                    }
                    choice.addReward(stateActionRewardValue);
                }

                // Now, check whether there is one more command combination to consider.
                bool movedIterator = false;
                for (int_fast64_t j = iteratorList.size() - 1; !movedIterator && j >= 0; --j) {
                    ++iteratorList[j];
                    if (iteratorList[j] != activeCommandList[j].end()) {
                        movedIterator = true;
                    } else {
                        // Reset the iterator to the beginning of the list.
                        iteratorList[j] = activeCommandList[j].begin();
                    }
                }

                done = !movedIterator;
            }
        }
    }

    return result;
}

template<typename ValueType, typename StateType>
void PrismNextStateGenerator<ValueType, StateType>::generateSynchronizedDistribution(
    storm::storage::BitVector const& state, ValueType const& probability, uint64_t position,
    std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>::const_iterator> const& iteratorList,
    storm::generator::Distribution<StateType, ValueType>& distribution, StateToIdCallback stateToIdCallback) {
    if (storm::utility::isZero<ValueType>(probability)) {
        return;
    }

    if (position >= iteratorList.size()) {
        StateType id = stateToIdCallback(state);
        distribution.add(id, std::move(probability));
    } else {
        storm::prism::Command const& command = *iteratorList[position];
        for (uint_fast64_t j = 0; j < command.getNumberOfUpdates(); ++j) {
            storm::prism::Update const& update = command.getUpdate(j);
            ValueType updateProbability = evaluateLikelihoodExpression<ValueType>(update, *this->evaluator);
            if constexpr (!std::is_same_v<ValueType, storm::RationalFunction>) {
                if (this->options.isExplorationChecksSet()) {
                    STORM_LOG_THROW(updateProbability >= storm::utility::zero<ValueType>(), storm::exceptions::WrongFormatException,
                                    "Probability expression in update '" << update << " evaluates to negative value " << updateProbability << ".");
                    STORM_LOG_THROW(!program.isDiscreteTimeModel() || updateProbability <= storm::utility::one<ValueType>(),
                                    storm::exceptions::WrongFormatException,
                                    "Probability expression in update '" << update << " evaluates to value " << updateProbability << " >1.");
                }
            }
            updateProbability *= probability;
            generateSynchronizedDistribution(applyUpdate(state, update), updateProbability, position + 1, iteratorList, distribution, stateToIdCallback);
        }
    }
}

template<typename ValueType, typename StateType>
void PrismNextStateGenerator<ValueType, StateType>::addSynchronousChoices(std::vector<Choice<ValueType>>& choices, CompressedState const& state,
                                                                          StateToIdCallback stateToIdCallback, CommandFilter const& commandFilter) {
    for (uint_fast64_t actionIndex : program.getSynchronizingActionIndices()) {
        if (this->actionMask != nullptr) {
            if (!this->actionMask->query(*this, actionIndex)) {
                continue;
            }
        }
        boost::optional<std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>>> optionalActiveCommandLists =
            getActiveCommandsByActionIndex(actionIndex, commandFilter);

        // Only process this action label, if there is at least one feasible solution.
        if (optionalActiveCommandLists) {
            std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>> const& activeCommandList = optionalActiveCommandLists.get();
            std::vector<std::vector<std::reference_wrapper<storm::prism::Command const>>::const_iterator> iteratorList(activeCommandList.size());

            // Initialize the list of iterators.
            for (size_t i = 0; i < activeCommandList.size(); ++i) {
                iteratorList[i] = activeCommandList[i].cbegin();
            }

            storm::generator::Distribution<StateType, ValueType> distribution;

            // As long as there is one feasible combination of commands, keep on expanding it.
            bool done = false;
            while (!done) {
                distribution.clear();
                generateSynchronizedDistribution(state, storm::utility::one<ValueType>(), 0, iteratorList, distribution, stateToIdCallback);
                distribution.compress();

                // At this point, we applied all commands of the current command combination and newTargetStates
                // contains all target states and their respective probabilities. That means we are now ready to
                // add the choice to the list of transitions.
                choices.push_back(Choice<ValueType>(actionIndex));

                // Now create the actual distribution.
                Choice<ValueType>& choice = choices.back();

                if (program.getModelType() == storm::prism::Program::ModelType::SMG) {
                    storm::storage::PlayerIndex const& playerOfAction = actionIndexToPlayerIndexMap.at(actionIndex);
                    STORM_LOG_THROW(playerOfAction != storm::storage::INVALID_PLAYER_INDEX, storm::exceptions::WrongFormatException,
                                    "Action " << program.getActionName(actionIndex)
                                              << " is not owned by any player but has at least one enabled, unlabeled (synchronized) command.");
                    choice.setPlayerIndex(playerOfAction);
                }

                // Remember the choice label and origins only if we were asked to.
                if (this->options.isBuildChoiceLabelsSet()) {
                    choice.addLabel(program.getActionName(actionIndex));
                }
                if (this->options.isBuildChoiceOriginsSet()) {
                    CommandSet commandIndices;
                    for (uint_fast64_t i = 0; i < iteratorList.size(); ++i) {
                        commandIndices.insert(iteratorList[i]->get().getGlobalIndex());
                    }
                    choice.addOriginData(boost::any(std::move(commandIndices)));
                }

                // Add the probabilities/rates to the newly created choice.
                ValueType probabilitySum = storm::utility::zero<ValueType>();
                choice.reserve(std::distance(distribution.begin(), distribution.end()));
                for (auto const& stateProbability : distribution) {
                    choice.addProbability(stateProbability.getState(), stateProbability.getValue());
                    if (this->options.isExplorationChecksSet()) {
                        probabilitySum += stateProbability.getValue();
                    }
                }

                if (this->options.isExplorationChecksSet()) {
                    // Check that the resulting distribution is in fact a distribution.
                    STORM_LOG_THROW(!program.isDiscreteTimeModel() || !storm::utility::isConstant(probabilitySum) || this->comparator.isOne(probabilitySum),
                                    storm::exceptions::WrongFormatException,
                                    "Sum of update probabilities do not some to one for some command (actually sum to " << probabilitySum << ").");
                }

                // Create the state-action reward for the newly created choice.
                for (auto const& rewardModel : rewardModels) {
                    ValueType stateActionRewardValue = storm::utility::zero<ValueType>();
                    if (rewardModel.get().hasStateActionRewards()) {
                        for (auto const& stateActionReward : rewardModel.get().getStateActionRewards()) {
                            if (stateActionReward.getActionIndex() == choice.getActionIndex() &&
                                this->evaluator->asBool(stateActionReward.getStatePredicateExpression())) {
                                stateActionRewardValue += ValueType(this->evaluator->asRational(stateActionReward.getRewardValueExpression()));
                            }
                        }
                    }
                    choice.addReward(stateActionRewardValue);
                }

                // Now, check whether there is one more command combination to consider.
                bool movedIterator = false;
                for (int_fast64_t j = iteratorList.size() - 1; !movedIterator && j >= 0; --j) {
                    ++iteratorList[j];
                    if (iteratorList[j] != activeCommandList[j].end()) {
                        movedIterator = true;
                    } else {
                        // Reset the iterator to the beginning of the list.
                        iteratorList[j] = activeCommandList[j].begin();
                    }
                }

                done = !movedIterator;
            }
        }
    }
}

template<typename ValueType, typename StateType>
std::map<std::string, storm::storage::PlayerIndex> PrismNextStateGenerator<ValueType, StateType>::getPlayerNameToIndexMap() const {
    return program.getPlayerNameToIndexMapping();
}

template<typename ValueType, typename StateType>
storm::models::sparse::StateLabeling PrismNextStateGenerator<ValueType, StateType>::label(storm::storage::sparse::StateStorage<StateType> const& stateStorage,
                                                                                          std::vector<StateType> const& initialStateIndices,
                                                                                          std::vector<StateType> const& deadlockStateIndices,
                                                                                          std::vector<StateType> const& unexploredStateIndices) {
    // Gather a vector of labels and their expressions.
    std::vector<std::pair<std::string, storm::expressions::Expression>> labels;
    if (this->options.isBuildAllLabelsSet()) {
        for (auto const& label : program.getLabels()) {
            labels.push_back(std::make_pair(label.getName(), label.getStatePredicateExpression()));
        }
    } else {
        for (auto const& labelName : this->options.getLabelNames()) {
            if (program.hasLabel(labelName)) {
                labels.push_back(std::make_pair(labelName, program.getLabelExpression(labelName)));
            } else {
                STORM_LOG_THROW(this->isSpecialLabel(labelName), storm::exceptions::InvalidArgumentException,
                                "Cannot build labeling for unknown label '" << labelName << "'.");
            }
        }
    }

    return NextStateGenerator<ValueType, StateType>::label(stateStorage, initialStateIndices, deadlockStateIndices, unexploredStateIndices, labels);
}

template<typename ValueType, typename StateType>
storm::storage::BitVector PrismNextStateGenerator<ValueType, StateType>::evaluateObservationLabels(CompressedState const& state) const {
    // TODO consider to avoid reloading by computing these bitvectors in an earlier build stage
    storm::storage::BitVector result(program.getNumberOfObservationLabels() * 64);

    if (program.getNumberOfObservationLabels() == 0) {
        return result;
    }
    unpackStateIntoEvaluator(state, this->variableInformation, *this->evaluator);
    for (uint64_t i = 0; i < program.getNumberOfObservationLabels(); ++i) {
        result.setFromInt(64 * i, 64, this->evaluator->asInt(program.getObservationLabels()[i].getStatePredicateExpression()));
    }
    return result;
}

template<typename ValueType, typename StateType>
void PrismNextStateGenerator<ValueType, StateType>::extendStateInformation(storm::json<BaseValueType>& result) const {
    for (uint64_t i = 0; i < program.getNumberOfObservationLabels(); ++i) {
        result[program.getObservationLabels()[i].getName()] = this->evaluator->asInt(program.getObservationLabels()[i].getStatePredicateExpression());
    }
}

template<typename ValueType, typename StateType>
std::size_t PrismNextStateGenerator<ValueType, StateType>::getNumberOfRewardModels() const {
    return rewardModels.size();
}

template<typename ValueType, typename StateType>
storm::builder::RewardModelInformation PrismNextStateGenerator<ValueType, StateType>::getRewardModelInformation(uint64_t const& index) const {
    storm::prism::RewardModel const& rewardModel = rewardModels[index].get();
    return storm::builder::RewardModelInformation(rewardModel.getName(), rewardModel.hasStateRewards(), rewardModel.hasStateActionRewards(),
                                                  rewardModel.hasTransitionRewards());
}

template<typename ValueType, typename StateType>
std::shared_ptr<storm::storage::sparse::ChoiceOrigins> PrismNextStateGenerator<ValueType, StateType>::generateChoiceOrigins(
    std::vector<boost::any>& dataForChoiceOrigins) const {
    if (!this->getOptions().isBuildChoiceOriginsSet()) {
        return nullptr;
    }

    std::vector<uint_fast64_t> identifiers;
    identifiers.reserve(dataForChoiceOrigins.size());

    std::map<CommandSet, uint_fast64_t> commandSetToIdentifierMap;
    // The empty commandset (i.e., the choices without origin) always has to get identifier getIdentifierForChoicesWithNoOrigin() -- which is assumed to be 0
    STORM_LOG_ASSERT(storm::storage::sparse::ChoiceOrigins::getIdentifierForChoicesWithNoOrigin() == 0, "The no origin identifier is assumed to be zero");
    commandSetToIdentifierMap.insert(std::make_pair(CommandSet(), 0));
    uint_fast64_t currentIdentifier = 1;
    for (boost::any& originData : dataForChoiceOrigins) {
        STORM_LOG_ASSERT(originData.empty() || boost::any_cast<CommandSet>(&originData) != nullptr,
                         "Origin data has unexpected type: " << originData.type().name() << ".");

        CommandSet currentCommandSet = originData.empty() ? CommandSet() : boost::any_cast<CommandSet>(std::move(originData));
        auto insertionRes = commandSetToIdentifierMap.insert(std::make_pair(std::move(currentCommandSet), currentIdentifier));
        identifiers.push_back(insertionRes.first->second);
        if (insertionRes.second) {
            ++currentIdentifier;
        }
    }

    std::vector<CommandSet> identifierToCommandSetMapping(currentIdentifier);
    for (auto const& setIdPair : commandSetToIdentifierMap) {
        identifierToCommandSetMapping[setIdPair.second] = setIdPair.first;
    }

    return std::make_shared<storm::storage::sparse::PrismChoiceOrigins>(std::make_shared<storm::prism::Program>(program), std::move(identifiers),
                                                                        std::move(identifierToCommandSetMapping));
}

template<typename ValueType, typename StateType>
bool PrismNextStateGenerator<ValueType, StateType>::isCommandPotentiallySynchronizing(const prism::Command& command) const {
    return program.getPossiblySynchronizingCommands().get(command.getGlobalIndex());
}

template class PrismNextStateGenerator<double>;
template class PrismNextStateGenerator<storm::RationalNumber>;
template class PrismNextStateGenerator<storm::RationalFunction>;
template class PrismNextStateGenerator<storm::Interval>;
template class PrismNextStateGenerator<storm::RationalInterval>;
}  // namespace generator
}  // namespace storm
