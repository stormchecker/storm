#include "storm/generator/JaniNextStateGenerator.h"

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/generator/Distribution.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/storage/expressions/SimpleValuation.h"
#include "storm/storage/jani/Automaton.h"
#include "storm/storage/jani/AutomatonComposition.h"
#include "storm/storage/jani/Edge.h"
#include "storm/storage/jani/EdgeDestination.h"
#include "storm/storage/jani/Location.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/ParallelComposition.h"
#include "storm/storage/jani/traverser/AssignmentLevelFinder.h"
#include "storm/storage/jani/traverser/RewardModelInformation.h"
#include "storm/storage/jani/visitor/CompositionInformationVisitor.h"
#include "storm/storage/sparse/JaniChoiceOrigins.h"
#include "storm/storage/umb/utility/ValuationDescriptionBuilder.h"
#include "storm/utility/combinatorics.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"
#include "storm/utility/vector.h"

namespace storm {
namespace generator {

template<typename ValueType, typename StateType>
JaniNextStateGenerator<ValueType, StateType>::JaniNextStateGenerator(storm::jani::Model const& model, NextStateGeneratorOptions const& options)
    : JaniNextStateGenerator(model.substituteConstantsFunctionsTranscendentals(), options, false) {
    // Intentionally left empty.
}

template<typename ValueType, typename StateType>
JaniNextStateGenerator<ValueType, StateType>::JaniNextStateGenerator(storm::jani::Model const& model, NextStateGeneratorOptions const& options, bool)
    : NextStateGenerator<ValueType, StateType>(model.getExpressionManager(), options),
      model(model),
      rewardExpressions(),
      hasStateActionRewards(false),
      evaluateRewardExpressionsAtEdges(false),
      evaluateRewardExpressionsAtDestinations(false) {
    auto features = this->model.getModelFeatures();
    features.remove(storm::jani::ModelFeature::DerivedOperators);
    features.remove(storm::jani::ModelFeature::StateExitRewards);
    features.remove(storm::jani::ModelFeature::MultiObjectiveProperties);
    features.remove(storm::jani::ModelFeature::TrigonometricFunctions);
    // Eliminate arrays if necessary.
    if (features.hasArrays()) {
        arrayEliminatorData = this->model.eliminateArrays(true);
        this->options.substituteExpressions([this](storm::expressions::Expression const& exp) { return arrayEliminatorData.transformExpression(exp); });
        features.remove(storm::jani::ModelFeature::Arrays);
    }
    STORM_LOG_THROW(features.empty(), storm::exceptions::NotSupportedException,
                    "The explicit next-state generator does not support the following model feature(s): " << features.toString() << ".");
    // Simplify the system compositions so that we can exclude the case where automata appear in the composition multiple times.
    this->model.simplifyComposition();

    // Get the reward expressions to be build. Also find out whether there is a non-trivial one.
    bool hasNonTrivialRewardExpressions = false;
    if (this->options.isBuildAllRewardModelsSet()) {
        rewardExpressions = this->model.getAllRewardModelExpressions();
        hasNonTrivialRewardExpressions = this->model.hasNonTrivialRewardExpression();
    } else {
        // Extract the reward models from the model based on the names we were given.
        for (auto const& rewardModelName : this->options.getRewardModelNames()) {
            rewardExpressions.emplace_back(rewardModelName, this->model.getRewardModelExpression(rewardModelName));
            hasNonTrivialRewardExpressions = hasNonTrivialRewardExpressions || this->model.isNonTrivialRewardModelExpression(rewardModelName);
        }
    }
    // If a transient variable has a non-zero default value, we also consider that non-trivial.
    // In those cases, lifting edge destination assignments to the edges would mean that reward is collected twice:
    // once at the edge (assigned value), once at the edge destinations (default value).
    if (!hasNonTrivialRewardExpressions) {
        for (auto const& rewExpr : rewardExpressions) {
            STORM_LOG_ASSERT(rewExpr.second.isVariable(), "Expected trivial reward expression to be a variable. Got " << rewExpr.second << " instead.");
            auto const& var = this->model.getGlobalVariables().getVariable(rewExpr.second.getBaseExpression().asVariableExpression().getVariable());
            if (var.isTransient() && var.hasInitExpression() && !storm::utility::isZero(var.getInitExpression().evaluateAsRational())) {
                hasNonTrivialRewardExpressions = true;
                break;
            }
        }
    }

    // We try to lift the edge destination assignments to the edges as this reduces the number of evaluator calls.
    // However, this will only be helpful if there are no assignment levels and only trivial reward expressions.
    if (hasNonTrivialRewardExpressions || this->model.usesAssignmentLevels()) {
        this->model.pushEdgeAssignmentsToDestinations();
    } else {
        this->model.liftTransientEdgeDestinationAssignments(storm::jani::AssignmentLevelFinder().getLowestAssignmentLevel(this->model));
        evaluateRewardExpressionsAtEdges = true;
    }

    // Create all synchronization-related information, e.g. the automata that are put in parallel.
    this->createSynchronizationInformation();

    // Now we are ready to initialize the variable information.
    this->checkValid();
    this->variableInformation =
        VariableInformation(this->model, this->parallelAutomata, options.getReservedBitsForUnboundedVariables(), options.isAddOutOfBoundsStateSet());
    this->variableInformation.registerArrayVariableReplacements(arrayEliminatorData);
    this->transientVariableInformation = TransientVariableInformation<ValueType>(this->model, this->parallelAutomata);
    this->transientVariableInformation.registerArrayVariableReplacements(arrayEliminatorData);
    this->initializeSpecialStates();

    // Create a proper evaluator.
    this->evaluator = std::make_unique<storm::expressions::ExpressionEvaluator<ValueType>>(this->model.getManager());
    this->transientVariableInformation.setDefaultValuesInEvaluator(*this->evaluator);

    // Build the information structs for the reward models.
    buildRewardModelInformation();

    // If there are terminal states we need to handle, we now need to translate all labels to expressions.
    if (this->options.hasTerminalStates()) {
        for (auto const& expressionOrLabelAndBool : this->options.getTerminalStates()) {
            if (expressionOrLabelAndBool.first.isExpression()) {
                this->terminalStates.emplace_back(expressionOrLabelAndBool.first.getExpression(), expressionOrLabelAndBool.second);
            } else {
                // If it's a label, i.e. refers to a transient boolean variable we do some sanity checks first
                if (!this->isSpecialLabel(expressionOrLabelAndBool.first.getLabel())) {
                    STORM_LOG_THROW(this->model.getGlobalVariables().hasVariable(expressionOrLabelAndBool.first.getLabel()),
                                    storm::exceptions::InvalidArgumentException,
                                    "Terminal states refer to illegal label '" << expressionOrLabelAndBool.first.getLabel() << "'.");

                    storm::jani::Variable const& variable = this->model.getGlobalVariables().getVariable(expressionOrLabelAndBool.first.getLabel());
                    STORM_LOG_THROW(variable.getType().isBasicType() && variable.getType().asBasicType().isBooleanType(),
                                    storm::exceptions::InvalidArgumentException,
                                    "Terminal states refer to non-boolean variable '" << expressionOrLabelAndBool.first.getLabel() << "'.");
                    STORM_LOG_THROW(variable.isTransient(), storm::exceptions::InvalidArgumentException,
                                    "Terminal states refer to non-transient variable '" << expressionOrLabelAndBool.first.getLabel() << "'.");

                    this->terminalStates.emplace_back(variable.getExpressionVariable().getExpression(), expressionOrLabelAndBool.second);
                }
            }
        }
    }
}

template<typename ValueType, typename StateType>
storm::jani::ModelFeatures JaniNextStateGenerator<ValueType, StateType>::getSupportedJaniFeatures() {
    storm::jani::ModelFeatures features;
    features.add(storm::jani::ModelFeature::DerivedOperators);
    features.add(storm::jani::ModelFeature::StateExitRewards);
    features.add(storm::jani::ModelFeature::MultiObjectiveProperties);
    features.add(storm::jani::ModelFeature::Arrays);
    features.add(storm::jani::ModelFeature::TrigonometricFunctions);
    // We do not add Functions as these should ideally be substituted before creating this generator.
    // This is because functions may also occur in properties and the user of this class should take care of that.
    return features;
}

template<typename ValueType, typename StateType>
bool JaniNextStateGenerator<ValueType, StateType>::canHandle(storm::jani::Model const& model) {
    auto features = model.getModelFeatures();
    features.remove(storm::jani::ModelFeature::Arrays);
    features.remove(storm::jani::ModelFeature::DerivedOperators);
    features.remove(storm::jani::ModelFeature::Functions);  // can be substituted
    features.remove(storm::jani::ModelFeature::StateExitRewards);
    features.remove(storm::jani::ModelFeature::MultiObjectiveProperties);
    features.remove(storm::jani::ModelFeature::TrigonometricFunctions);
    if (!features.empty()) {
        STORM_LOG_INFO("The model can not be build as it contains these unsupported features: " << features.toString());
        return false;
    }
    // There probably are more cases where the model is unsupported. However, checking these is more involved.
    // As this method is supposed to be a quick check, we just return true at this point.
    return true;
}

template<typename ValueType, typename StateType>
ModelType JaniNextStateGenerator<ValueType, StateType>::getModelType() const {
    switch (model.getModelType()) {
        case storm::jani::ModelType::DTMC:
            return ModelType::DTMC;
        case storm::jani::ModelType::CTMC:
            return ModelType::CTMC;
        case storm::jani::ModelType::MDP:
            return ModelType::MDP;
        case storm::jani::ModelType::MA:
            return ModelType::MA;
        default:
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Invalid model type.");
    }
}

template<typename ValueType, typename StateType>
bool JaniNextStateGenerator<ValueType, StateType>::isDeterministicModel() const {
    return model.isDeterministicModel();
}

template<typename ValueType, typename StateType>
bool JaniNextStateGenerator<ValueType, StateType>::isDiscreteTimeModel() const {
    return model.isDiscreteTimeModel();
}

template<typename ValueType, typename StateType>
bool JaniNextStateGenerator<ValueType, StateType>::isPartiallyObservable() const {
    return false;
}

template<typename ValueType, typename StateType>
uint64_t JaniNextStateGenerator<ValueType, StateType>::getLocation(CompressedState const& state, LocationVariableInformation const& locationVariable) const {
    if (locationVariable.bitWidth == 0) {
        return 0;
    } else {
        return state.getAsInt(locationVariable.bitOffset, locationVariable.bitWidth);
    }
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::setLocation(CompressedState& state, LocationVariableInformation const& locationVariable,
                                                               uint64_t locationIndex) const {
    if (locationVariable.bitWidth != 0) {
        state.setFromInt(locationVariable.bitOffset, locationVariable.bitWidth, locationIndex);
    }
}

template<typename ValueType, typename StateType>
std::vector<uint64_t> JaniNextStateGenerator<ValueType, StateType>::getLocations(CompressedState const& state) const {
    std::vector<uint64_t> result(this->variableInformation.locationVariables.size());

    auto resultIt = result.begin();
    for (auto it = this->variableInformation.locationVariables.begin(), ite = this->variableInformation.locationVariables.end(); it != ite; ++it, ++resultIt) {
        if (it->bitWidth == 0) {
            *resultIt = 0;
        } else {
            *resultIt = state.getAsInt(it->bitOffset, it->bitWidth);
        }
    }

    return result;
}

template<typename ValueType, typename StateType>
std::vector<StateType> JaniNextStateGenerator<ValueType, StateType>::getInitialStates(StateToIdCallback const& stateToIdCallback) {
    std::vector<StateType> initialStateIndices;

    if (this->model.hasNonTrivialInitialStates()) {
        // Prepare an SMT solver to enumerate all initial states.
        storm::utility::solver::SmtSolverFactory factory;
        std::unique_ptr<storm::solver::SmtSolver> solver = factory.create(model.getExpressionManager());

        std::vector<storm::expressions::Expression> rangeExpressions = model.getAllRangeExpressions(this->parallelAutomata);
        for (auto const& expression : rangeExpressions) {
            solver->add(expression);
        }
        solver->add(model.getInitialStatesExpression(this->parallelAutomata));

        // Proceed as long as the solver can still enumerate initial states.
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
                if (integerVariable.forceOutOfBoundsCheck || this->getOptions().isExplorationChecksSet()) {
                    STORM_LOG_THROW(variableValue >= integerVariable.lowerBound, storm::exceptions::WrongFormatException,
                                    "The initial value for variable " << integerVariable.variable.getName() << " is lower than the lower bound.");
                    STORM_LOG_THROW(variableValue <= integerVariable.upperBound, storm::exceptions::WrongFormatException,
                                    "The initial value for variable " << integerVariable.variable.getName() << " is higher than the upper bound");
                }
                storm::expressions::Expression localBlockingExpression = integerVariable.variable != model->getManager().integer(variableValue);
                blockingExpression = blockingExpression.isInitialized() ? blockingExpression || localBlockingExpression : localBlockingExpression;
                initialState.setFromInt(integerVariable.bitOffset, integerVariable.bitWidth,
                                        static_cast<uint_fast64_t>(variableValue - integerVariable.lowerBound));
            }

            // Gather iterators to the initial locations of all the automata.
            std::vector<std::set<uint64_t>::const_iterator> initialLocationsIts;
            std::vector<std::set<uint64_t>::const_iterator> initialLocationsItes;
            for (auto const& automatonRef : this->parallelAutomata) {
                auto const& automaton = automatonRef.get();
                initialLocationsIts.push_back(automaton.getInitialLocationIndices().cbegin());
                initialLocationsItes.push_back(automaton.getInitialLocationIndices().cend());
            }
            storm::utility::combinatorics::forEach(
                initialLocationsIts, initialLocationsItes,
                [this, &initialState](uint64_t index, uint64_t value) { setLocation(initialState, this->variableInformation.locationVariables[index], value); },
                [&stateToIdCallback, &initialStateIndices, &initialState]() {
                    // Register initial state.
                    StateType id = stateToIdCallback(initialState);
                    initialStateIndices.push_back(id);
                    return true;
                });

            // Block the current initial state to search for the next one.
            if (!blockingExpression.isInitialized()) {
                break;
            }
            solver->add(blockingExpression);
        }

        STORM_LOG_DEBUG("Enumerated " << initialStateIndices.size() << " initial states using SMT solving.");
    } else {
        // Create vectors holding all possible values
        std::vector<std::vector<uint64_t>> allValues;
        for (auto const& aRef : this->parallelAutomata) {
            auto const& aInitLocs = aRef.get().getInitialLocationIndices();
            allValues.emplace_back(aInitLocs.begin(), aInitLocs.end());
        }
        uint64_t locEndIndex = allValues.size();
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
            [this, &initialState, &locEndIndex, &intEndIndex](uint64_t index, uint64_t value) {
                // Set the value for the variable corresponding to the given index
                if (index < locEndIndex) {
                    // Location variable
                    setLocation(initialState, this->variableInformation.locationVariables[index], value);
                } else if (index < intEndIndex) {
                    // Integer variable
                    auto const& intVar = this->variableInformation.integerVariables[index - locEndIndex];
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
    }
    return initialStateIndices;
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::applyUpdate(CompressedState& state, storm::jani::EdgeDestination const& destination,
                                                               storm::generator::LocationVariableInformation const& locationVariable, int64_t assignmentLevel,
                                                               storm::expressions::ExpressionEvaluator<ValueType> const& expressionEvaluator) {
    // Update the location of the state.
    setLocation(state, locationVariable, destination.getLocationIndex());

    // Then perform the assignments.
    auto const& assignments = destination.getOrderedAssignments().getNonTransientAssignments(assignmentLevel);
    auto assignmentIt = assignments.begin();
    auto assignmentIte = assignments.end();

    // Iterate over all boolean assignments and carry them out.
    auto boolIt = this->variableInformation.booleanVariables.begin();
    for (; assignmentIt != assignmentIte && assignmentIt->lValueIsVariable() && assignmentIt->getExpressionVariable().hasBooleanType(); ++assignmentIt) {
        while (assignmentIt->getExpressionVariable() != boolIt->variable) {
            ++boolIt;
        }
        state.set(boolIt->bitOffset, expressionEvaluator.asBool(assignmentIt->getAssignedExpression()));
    }

    // Iterate over all integer assignments and carry them out.
    auto integerIt = this->variableInformation.integerVariables.begin();
    for (; assignmentIt != assignmentIte && assignmentIt->lValueIsVariable() && assignmentIt->getExpressionVariable().hasIntegerType(); ++assignmentIt) {
        while (assignmentIt->getExpressionVariable() != integerIt->variable) {
            ++integerIt;
        }
        int_fast64_t assignedValue = expressionEvaluator.asInt(assignmentIt->getAssignedExpression());
        if (this->options.isAddOutOfBoundsStateSet()) {
            if (assignedValue < integerIt->lowerBound || assignedValue > integerIt->upperBound) {
                state = this->outOfBoundsState;
            }
        } else if (integerIt->forceOutOfBoundsCheck || this->options.isExplorationChecksSet()) {
            STORM_LOG_THROW(assignedValue >= integerIt->lowerBound, storm::exceptions::WrongFormatException,
                            "The update " << assignmentIt->getExpressionVariable().getName() << " := " << assignmentIt->getAssignedExpression()
                                          << " leads to an out-of-bounds value (" << assignedValue << ") for the variable '"
                                          << assignmentIt->getExpressionVariable().getName() << "'.");
            STORM_LOG_THROW(assignedValue <= integerIt->upperBound, storm::exceptions::WrongFormatException,
                            "The update " << assignmentIt->getExpressionVariable().getName() << " := " << assignmentIt->getAssignedExpression()
                                          << " leads to an out-of-bounds value (" << assignedValue << ") for the variable '"
                                          << assignmentIt->getExpressionVariable().getName() << "'.");
        }
        state.setFromInt(integerIt->bitOffset, integerIt->bitWidth, assignedValue - integerIt->lowerBound);
        STORM_LOG_ASSERT(static_cast<int_fast64_t>(state.getAsInt(integerIt->bitOffset, integerIt->bitWidth)) + integerIt->lowerBound == assignedValue,
                         "Writing to the bit vector bucket failed (read " << state.getAsInt(integerIt->bitOffset, integerIt->bitWidth) << " but wrote "
                                                                          << assignedValue << ").");
    }
    // Iterate over all array access assignments and carry them out.
    for (; assignmentIt != assignmentIte && assignmentIt->lValueIsArrayAccess(); ++assignmentIt) {
        auto const& arrayIndicesAsExpr = assignmentIt->getLValue().getArrayIndexVector();
        std::vector<uint64_t> arrayIndices;
        arrayIndices.reserve(arrayIndicesAsExpr.size());
        for (auto const& i : arrayIndicesAsExpr) {
            arrayIndices.push_back(static_cast<uint64_t>(expressionEvaluator.asInt(i)));
        }
        if (assignmentIt->getAssignedExpression().hasIntegerType()) {
            IntegerVariableInformation const& intInfo =
                this->variableInformation.getIntegerArrayVariableReplacement(assignmentIt->getLValue().getVariable().getExpressionVariable(), arrayIndices);
            int_fast64_t assignedValue = expressionEvaluator.asInt(assignmentIt->getAssignedExpression());

            if (this->options.isAddOutOfBoundsStateSet()) {
                if (assignedValue < intInfo.lowerBound || assignedValue > intInfo.upperBound) {
                    state = this->outOfBoundsState;
                }
            } else if (this->options.isExplorationChecksSet()) {
                STORM_LOG_THROW(assignedValue >= intInfo.lowerBound, storm::exceptions::WrongFormatException,
                                "The update " << assignmentIt->getLValue() << " := " << assignmentIt->getAssignedExpression()
                                              << " leads to an out-of-bounds value (" << assignedValue << ") for the variable '"
                                              << assignmentIt->getExpressionVariable().getName() << "'.");
                STORM_LOG_THROW(assignedValue <= intInfo.upperBound, storm::exceptions::WrongFormatException,
                                "The update " << assignmentIt->getLValue() << " := " << assignmentIt->getAssignedExpression()
                                              << " leads to an out-of-bounds value (" << assignedValue << ") for the variable '"
                                              << assignmentIt->getExpressionVariable().getName() << "'.");
            }
            state.setFromInt(intInfo.bitOffset, intInfo.bitWidth, assignedValue - intInfo.lowerBound);
            STORM_LOG_ASSERT(static_cast<int_fast64_t>(state.getAsInt(intInfo.bitOffset, intInfo.bitWidth)) + intInfo.lowerBound == assignedValue,
                             "Writing to the bit vector bucket failed (read " << state.getAsInt(intInfo.bitOffset, intInfo.bitWidth) << " but wrote "
                                                                              << assignedValue << ").");
        } else if (assignmentIt->getAssignedExpression().hasBooleanType()) {
            BooleanVariableInformation const& boolInfo =
                this->variableInformation.getBooleanArrayVariableReplacement(assignmentIt->getLValue().getVariable().getExpressionVariable(), arrayIndices);
            state.set(boolInfo.bitOffset, expressionEvaluator.asBool(assignmentIt->getAssignedExpression()));
        } else {
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unhandled type of base variable.");
        }
    }

    // Check that we processed all assignments.
    STORM_LOG_ASSERT(assignmentIt == assignmentIte, "Not all assignments were consumed.");
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::applyTransientUpdate(TransientVariableValuation<ValueType>& transientValuation,
                                                                        storm::jani::detail::ConstAssignments const& transientAssignments,
                                                                        storm::expressions::ExpressionEvaluator<ValueType> const& expressionEvaluator) const {
    auto assignmentIt = transientAssignments.begin();
    auto assignmentIte = transientAssignments.end();

    // Iterate over all boolean assignments and carry them out.
    auto boolIt = this->transientVariableInformation.booleanVariableInformation.begin();
    for (; assignmentIt != assignmentIte && assignmentIt->lValueIsVariable() && assignmentIt->getExpressionVariable().hasBooleanType(); ++assignmentIt) {
        while (assignmentIt->getExpressionVariable() != boolIt->variable) {
            ++boolIt;
        }
        transientValuation.booleanValues.emplace_back(&(*boolIt), expressionEvaluator.asBool(assignmentIt->getAssignedExpression()));
    }
    // Iterate over all integer assignments and carry them out.
    auto integerIt = this->transientVariableInformation.integerVariableInformation.begin();
    for (; assignmentIt != assignmentIte && assignmentIt->lValueIsVariable() && assignmentIt->getExpressionVariable().hasIntegerType(); ++assignmentIt) {
        while (assignmentIt->getExpressionVariable() != integerIt->variable) {
            ++integerIt;
        }
        int64_t assignedValue = expressionEvaluator.asInt(assignmentIt->getAssignedExpression());
        if (this->options.isExplorationChecksSet()) {
            STORM_LOG_THROW(!integerIt->lowerBound || assignedValue >= integerIt->lowerBound.get(), storm::exceptions::WrongFormatException,
                            "The update " << assignmentIt->getExpressionVariable().getName() << " := " << assignmentIt->getAssignedExpression()
                                          << " leads to an out-of-bounds value (" << assignedValue << ") for the variable '"
                                          << assignmentIt->getExpressionVariable().getName() << "'.");
            STORM_LOG_THROW(!integerIt->upperBound || assignedValue <= integerIt->upperBound.get(), storm::exceptions::WrongFormatException,
                            "The update " << assignmentIt->getExpressionVariable().getName() << " := " << assignmentIt->getAssignedExpression()
                                          << " leads to an out-of-bounds value (" << assignedValue << ") for the variable '"
                                          << assignmentIt->getExpressionVariable().getName() << "'.");
        }
        transientValuation.integerValues.emplace_back(&(*integerIt), assignedValue);
    }
    // Iterate over all rational assignments and carry them out.
    auto rationalIt = this->transientVariableInformation.rationalVariableInformation.begin();
    for (; assignmentIt != assignmentIte && assignmentIt->lValueIsVariable() && assignmentIt->getExpressionVariable().hasRationalType(); ++assignmentIt) {
        while (assignmentIt->getExpressionVariable() != rationalIt->variable) {
            ++rationalIt;
        }
        transientValuation.rationalValues.emplace_back(&(*rationalIt), expressionEvaluator.asRational(assignmentIt->getAssignedExpression()));
    }

    // Iterate over all array access assignments and carry them out.
    for (; assignmentIt != assignmentIte && assignmentIt->lValueIsArrayAccess(); ++assignmentIt) {
        auto const& arrayIndicesAsExpr = assignmentIt->getLValue().getArrayIndexVector();
        std::vector<uint64_t> arrayIndices;
        arrayIndices.reserve(arrayIndicesAsExpr.size());
        for (auto const& i : arrayIndicesAsExpr) {
            arrayIndices.push_back(static_cast<uint64_t>(expressionEvaluator.asInt(i)));
        }
        storm::expressions::Type const& baseType = assignmentIt->getLValue().getVariable().getExpressionVariable().getType();
        if (baseType.isIntegerType()) {
            auto const& intInfo = this->transientVariableInformation.getIntegerArrayVariableReplacement(
                assignmentIt->getLValue().getVariable().getExpressionVariable(), arrayIndices);
            int64_t assignedValue = expressionEvaluator.asInt(assignmentIt->getAssignedExpression());
            if (this->options.isExplorationChecksSet()) {
                STORM_LOG_THROW(assignedValue >= intInfo.lowerBound, storm::exceptions::WrongFormatException,
                                "The update " << assignmentIt->getLValue() << " := " << assignmentIt->getAssignedExpression()
                                              << " leads to an out-of-bounds value (" << assignedValue << ") for the variable '"
                                              << assignmentIt->getExpressionVariable().getName() << "'.");
                STORM_LOG_THROW(assignedValue <= intInfo.upperBound, storm::exceptions::WrongFormatException,
                                "The update " << assignmentIt->getLValue() << " := " << assignmentIt->getAssignedExpression()
                                              << " leads to an out-of-bounds value (" << assignedValue << ") for the variable '"
                                              << assignmentIt->getExpressionVariable().getName() << "'.");
            }
            transientValuation.integerValues.emplace_back(&intInfo, assignedValue);
        } else if (baseType.isBooleanType()) {
            auto const& boolInfo = this->transientVariableInformation.getBooleanArrayVariableReplacement(
                assignmentIt->getLValue().getVariable().getExpressionVariable(), arrayIndices);
            transientValuation.booleanValues.emplace_back(&boolInfo, expressionEvaluator.asBool(assignmentIt->getAssignedExpression()));
        } else if (baseType.isRationalType()) {
            auto const& rationalInfo = this->transientVariableInformation.getRationalArrayVariableReplacement(
                assignmentIt->getLValue().getVariable().getExpressionVariable(), arrayIndices);
            transientValuation.rationalValues.emplace_back(&rationalInfo, expressionEvaluator.asRational(assignmentIt->getAssignedExpression()));
        } else {
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unhandled type of base variable.");
        }
    }

    // Check that we processed all assignments.
    STORM_LOG_ASSERT(assignmentIt == assignmentIte, "Not all assignments were consumed.");
}

template<typename ValueType, typename StateType>
TransientVariableValuation<ValueType> JaniNextStateGenerator<ValueType, StateType>::getTransientVariableValuationAtLocations(
    std::vector<uint64_t> const& locations, storm::expressions::ExpressionEvaluator<ValueType> const& evaluator) const {
    uint64_t automatonIndex = 0;
    TransientVariableValuation<ValueType> transientVariableValuation;
    for (auto const& automatonRef : this->parallelAutomata) {
        auto const& automaton = automatonRef.get();
        uint64_t currentLocationIndex = locations[automatonIndex];
        storm::jani::Location const& location = automaton.getLocation(currentLocationIndex);
        STORM_LOG_ASSERT(!location.getAssignments().hasMultipleLevels(true), "Indexed assignments at locations are not supported in the jani standard.");
        applyTransientUpdate(transientVariableValuation, location.getAssignments().getTransientAssignments(), evaluator);
        ++automatonIndex;
    }
    return transientVariableValuation;
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::unpackTransientVariableValuesIntoEvaluator(
    CompressedState const& state, storm::expressions::ExpressionEvaluator<ValueType>& evaluator) const {
    transientVariableInformation.setDefaultValuesInEvaluator(evaluator);
    auto transientVariableValuation = getTransientVariableValuationAtLocations(getLocations(state), evaluator);
    transientVariableValuation.setInEvaluator(evaluator, this->getOptions().isExplorationChecksSet());
}

template<typename ValueType, typename StateType>
storm::storage::sparse::Valuations JaniNextStateGenerator<ValueType, StateType>::initializeStateValuations() const {
    storm::umb::ValuationDescriptionBuilder builder(this->model.getManager().shared_from_this());
    if (this->variableInformation.hasOutOfBoundsBit()) {
        builder.addBooleanVariable(this->variableInformation.outOfBoundsBit->variable);
    }
    for (auto const& v : this->variableInformation.locationVariables) {
        builder.addIntegerVariable(v.variable, 0, v.highestValue);
    }
    for (auto const& v : this->variableInformation.booleanVariables) {
        builder.addBooleanVariable(v.variable);
    }
    for (auto const& v : this->variableInformation.integerVariables) {
        builder.addIntegerVariable(v.variable, v.lowerBound, v.upperBound);
    }
    // Also add information for transient variables
    for (auto const& varInfo : transientVariableInformation.booleanVariableInformation) {
        builder.addBooleanVariable(varInfo.variable);
    }
    for (auto const& varInfo : transientVariableInformation.integerVariableInformation) {
        builder.addIntegerVariable(varInfo.variable, varInfo.lowerBound.value_or(std::numeric_limits<int64_t>::min()),
                                   varInfo.upperBound.value_or(std::numeric_limits<int64_t>::max()));
    }
    for (auto const& varInfo : transientVariableInformation.rationalVariableInformation) {
        if (std::is_same_v<ValueType, storm::RationalNumber>) {
            uint64_t const bitSize = std::max<uint64_t>(64, this->getOptions().getReservedBitsForUnboundedVariables());
            builder.addRationalVariable(varInfo.variable, bitSize * 2);  // reserve bits for numerator and denominator
        } else {
            STORM_LOG_THROW((std::is_same_v<ValueType, double>), storm::exceptions::NotSupportedException,
                            "State valuations for transient variables of the given value type are not supported.");
            builder.addDoubleVariable(varInfo.variable);
        }
    }
    return storm::storage::sparse::Valuations(builder.buildClassDescription(), builder.getManager().shared_from_this());
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::addStateValuation(storm::storage::sparse::state_type const& currentStateIndex,
                                                                     storm::storage::sparse::Valuations& valuations) const {
    // Add values for non-transient variables
    unpackStateAppendToUmbValuations(*this->state, this->variableInformation, valuations.getUmbValuations());

    auto transientVariableValuation = getTransientVariableValuationAtLocations(getLocations(*this->state), *this->evaluator);
    transientVariableValuation.setInUmbValuations(currentStateIndex, transientVariableInformation, valuations.getUmbValuations());
}

template<typename ValueType, typename StateType>
StateBehavior<ValueType, StateType> JaniNextStateGenerator<ValueType, StateType>::expand(StateToIdCallback const& stateToIdCallback) {
    // The evaluator should have the default values of the transient variables right now.

    // Prepare the result, in case we return early.
    StateBehavior<ValueType, StateType> result;

    // Retrieve the locations from the state.
    std::vector<uint64_t> locations = getLocations(*this->state);

    // First, construct the state rewards, as we may return early if there are no choices later and we already
    // need the state rewards then.
    auto transientVariableValuation = getTransientVariableValuationAtLocations(locations, *this->evaluator);
    transientVariableValuation.setInEvaluator(*this->evaluator, this->getOptions().isExplorationChecksSet());
    result.addStateRewards(evaluateRewardExpressions());

    // If a terminal expression was set and we must not expand this state, return now.
    // Terminal state expressions do not consider transient variables.
    if (!this->terminalStates.empty()) {
        for (auto const& expressionBool : this->terminalStates) {
            if (this->evaluator->asBool(expressionBool.first) == expressionBool.second) {
                // Set back transient variables to default values so we are ready to process the next state
                this->transientVariableInformation.setDefaultValuesInEvaluator(*this->evaluator);
                return result;
            }
        }
    }

    // Set back transient variables to default values so we are ready to process the transition assignments
    this->transientVariableInformation.setDefaultValuesInEvaluator(*this->evaluator);

    // Get all choices for the state.
    result.setExpanded();
    std::vector<Choice<ValueType>> allChoices;
    if (this->getOptions().isApplyMaximalProgressAssumptionSet()) {
        // First explore only edges without a rate
        allChoices = getActionChoices(locations, *this->state, stateToIdCallback, EdgeFilter::WithoutRate);
        if (allChoices.empty()) {
            // Expand the Markovian edges if there are no probabilistic ones.
            allChoices = getActionChoices(locations, *this->state, stateToIdCallback, EdgeFilter::WithRate);
        }
    } else {
        allChoices = getActionChoices(locations, *this->state, stateToIdCallback);
    }
    std::size_t totalNumberOfChoices = allChoices.size();

    // If there is not a single choice, we return immediately, because the state has no behavior (other than
    // the state reward).
    if (totalNumberOfChoices == 0) {
        return result;
    }

    // If the model is a deterministic model, we need to fuse the choices into one.
    if (this->isDeterministicModel() && totalNumberOfChoices > 1) {
        Choice<ValueType> globalChoice;

        if (this->options.isAddOverlappingGuardLabelSet()) {
            this->overlappingGuardStates->push_back(stateToIdCallback(*this->state));
        }

        // For CTMCs, we need to keep track of the total exit rate to scale the action rewards later. For DTMCs
        // this is equal to the number of choices, which is why we initialize it like this here.
        ValueType totalExitRate = this->isDiscreteTimeModel() ? static_cast<ValueType>(totalNumberOfChoices) : storm::utility::zero<ValueType>();

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
        }

        std::vector<ValueType> stateActionRewards(rewardExpressions.size(), storm::utility::zero<ValueType>());
        for (auto const& choice : allChoices) {
            if (hasStateActionRewards) {
                for (uint_fast64_t rewardVariableIndex = 0; rewardVariableIndex < rewardExpressions.size(); ++rewardVariableIndex) {
                    stateActionRewards[rewardVariableIndex] += choice.getRewards()[rewardVariableIndex] * choice.getTotalMass() / totalExitRate;
                }
            }

            if (this->options.isBuildChoiceOriginsSet() && choice.hasOriginData()) {
                globalChoice.addOriginData(choice.getOriginData());
            }
        }
        globalChoice.addRewards(std::move(stateActionRewards));

        // Move the newly fused choice in place.
        allChoices.clear();
        allChoices.push_back(std::move(globalChoice));
    }

    // Move all remaining choices in place.
    for (auto& choice : allChoices) {
        result.addChoice(std::move(choice));
    }

    this->postprocess(result);

    return result;
}

template<typename ValueType, typename StateType>
Choice<ValueType> JaniNextStateGenerator<ValueType, StateType>::expandNonSynchronizingEdge(storm::jani::Edge const& edge, uint64_t outputActionIndex,
                                                                                           uint64_t automatonIndex, CompressedState const& state,
                                                                                           StateToIdCallback stateToIdCallback) {
    // Determine the exit rate if it's a Markovian edge.
    boost::optional<ValueType> exitRate = boost::none;
    if (edge.hasRate()) {
        exitRate = this->evaluator->asRational(edge.getRate());
    }

    Choice<ValueType> choice(edge.getActionIndex(), static_cast<bool>(exitRate));
    std::vector<ValueType> stateActionRewards;

    // Perform the transient edge assignments and create the state action rewards
    TransientVariableValuation<ValueType> transientVariableValuation;
    if (!evaluateRewardExpressionsAtEdges || edge.getAssignments().empty()) {
        stateActionRewards.resize(rewardModelInformation.size(), storm::utility::zero<ValueType>());
    } else {
        for (int64_t assignmentLevel = edge.getAssignments().getLowestLevel(true); assignmentLevel <= edge.getAssignments().getHighestLevel(true);
             ++assignmentLevel) {
            transientVariableValuation.clear();
            applyTransientUpdate(transientVariableValuation, edge.getAssignments().getTransientAssignments(assignmentLevel), *this->evaluator);
            transientVariableValuation.setInEvaluator(*this->evaluator, this->getOptions().isExplorationChecksSet());
        }
        stateActionRewards = evaluateRewardExpressions();
        transientVariableInformation.setDefaultValuesInEvaluator(*this->evaluator);
    }

    // Iterate over all updates of the current command.
    ValueType probabilitySum = storm::utility::zero<ValueType>();
    for (auto const& destination : edge.getDestinations()) {
        ValueType probability = this->evaluator->asRational(destination.getProbability());

        if (probability != storm::utility::zero<ValueType>()) {
            bool evaluatorChanged = false;
            // Obtain target state index and add it to the list of known states. If it has not yet been
            // seen, we also add it to the set of states that have yet to be explored.
            int64_t assignmentLevel = edge.getLowestAssignmentLevel();  // Might be the largest possible integer, if there is no assignment
            int64_t const& highestLevel = edge.getHighestAssignmentLevel();
            bool hasTransientAssignments = destination.hasTransientAssignment();
            CompressedState newState = state;
            applyUpdate(newState, destination, this->variableInformation.locationVariables[automatonIndex], assignmentLevel, *this->evaluator);
            if (hasTransientAssignments) {
                STORM_LOG_ASSERT(this->options.isScaleAndLiftTransitionRewardsSet(),
                                 "Transition rewards are not supported and scaling to action rewards is disabled.");
                transientVariableValuation.clear();
                applyTransientUpdate(transientVariableValuation, destination.getOrderedAssignments().getTransientAssignments(assignmentLevel),
                                     *this->evaluator);
                transientVariableValuation.setInEvaluator(*this->evaluator, this->getOptions().isExplorationChecksSet());
                evaluatorChanged = true;
            }
            if (assignmentLevel < highestLevel) {
                while (assignmentLevel < highestLevel) {
                    ++assignmentLevel;
                    unpackStateIntoEvaluator(newState, this->variableInformation, *this->evaluator);
                    evaluatorChanged = true;
                    applyUpdate(newState, destination, this->variableInformation.locationVariables[automatonIndex], assignmentLevel, *this->evaluator);
                    if (hasTransientAssignments) {
                        transientVariableValuation.clear();
                        applyTransientUpdate(transientVariableValuation, destination.getOrderedAssignments().getTransientAssignments(assignmentLevel),
                                             *this->evaluator);
                        transientVariableValuation.setInEvaluator(*this->evaluator, this->getOptions().isExplorationChecksSet());
                        evaluatorChanged = true;
                    }
                }
            }
            if (evaluateRewardExpressionsAtDestinations) {
                unpackStateIntoEvaluator(newState, this->variableInformation, *this->evaluator);
                evaluatorChanged = true;
                addEvaluatedRewardExpressions(stateActionRewards, probability);
            }

            if (evaluatorChanged) {
                // Restore the old variable valuation
                unpackStateIntoEvaluator(state, this->variableInformation, *this->evaluator);
                if (hasTransientAssignments) {
                    this->transientVariableInformation.setDefaultValuesInEvaluator(*this->evaluator);
                }
            }

            StateType stateIndex = stateToIdCallback(newState);

            // Update the choice by adding the probability/target state to it.
            probability = exitRate ? exitRate.get() * probability : probability;
            choice.addProbability(stateIndex, probability);

            if (this->options.isExplorationChecksSet()) {
                probabilitySum += probability;
            }
        }
    }

    // Add the state action rewards
    choice.addRewards(std::move(stateActionRewards));

    if (this->options.isExplorationChecksSet()) {
        // Check that the resulting distribution is in fact a distribution.
        STORM_LOG_THROW(!this->isDiscreteTimeModel() || (!storm::utility::isConstant(probabilitySum) || this->comparator.isOne(probabilitySum)),
                        storm::exceptions::WrongFormatException, "Probabilities do not sum to one for edge (actually sum to " << probabilitySum << ").");
    }

    return choice;
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::generateSynchronizedDistribution(storm::storage::BitVector const& state,
                                                                                    AutomataEdgeSets const& edgeCombination,
                                                                                    std::vector<EdgeSetWithIndices::const_iterator> const& iteratorList,
                                                                                    storm::generator::Distribution<StateType, ValueType>& distribution,
                                                                                    std::vector<ValueType>& stateActionRewards, EdgeIndexSet& edgeIndices,
                                                                                    StateToIdCallback stateToIdCallback) {
    // Collect some information of the edges.
    int64_t lowestDestinationAssignmentLevel = std::numeric_limits<int64_t>::max();
    int64_t highestDestinationAssignmentLevel = std::numeric_limits<int64_t>::min();
    int64_t lowestEdgeAssignmentLevel = std::numeric_limits<int64_t>::max();
    int64_t highestEdgeAssignmentLevel = std::numeric_limits<int64_t>::min();
    uint64_t numDestinations = 1;
    for (uint_fast64_t i = 0; i < iteratorList.size(); ++i) {
        if (this->getOptions().isBuildChoiceOriginsSet()) {
            auto automatonIndex = model.getAutomatonIndex(parallelAutomata[edgeCombination[i].first].get().getName());
            edgeIndices.insert(model.encodeAutomatonAndEdgeIndices(automatonIndex, iteratorList[i]->first));
        }
        storm::jani::Edge const& edge = *iteratorList[i]->second;
        lowestDestinationAssignmentLevel = std::min(lowestDestinationAssignmentLevel, edge.getLowestAssignmentLevel());
        highestDestinationAssignmentLevel = std::max(highestDestinationAssignmentLevel, edge.getHighestAssignmentLevel());
        if (!edge.getAssignments().empty(true)) {
            lowestEdgeAssignmentLevel = std::min(lowestEdgeAssignmentLevel, edge.getAssignments().getLowestLevel(true));
            highestEdgeAssignmentLevel = std::max(highestEdgeAssignmentLevel, edge.getAssignments().getHighestLevel(true));
        }
        numDestinations *= edge.getNumberOfDestinations();
    }

    // Perform the edge assignments (if there are any)
    TransientVariableValuation<ValueType> transientVariableValuation;
    if (evaluateRewardExpressionsAtEdges && lowestEdgeAssignmentLevel <= highestEdgeAssignmentLevel) {
        for (int64_t assignmentLevel = lowestEdgeAssignmentLevel; assignmentLevel <= highestEdgeAssignmentLevel; ++assignmentLevel) {
            transientVariableValuation.clear();
            for (uint_fast64_t i = 0; i < iteratorList.size(); ++i) {
                storm::jani::Edge const& edge = *iteratorList[i]->second;
                applyTransientUpdate(transientVariableValuation, edge.getAssignments().getTransientAssignments(assignmentLevel), *this->evaluator);
            }
            transientVariableValuation.setInEvaluator(*this->evaluator, this->getOptions().isExplorationChecksSet());
        }
        addEvaluatedRewardExpressions(stateActionRewards, storm::utility::one<ValueType>());
        transientVariableInformation.setDefaultValuesInEvaluator(*this->evaluator);
    }

    std::vector<storm::jani::EdgeDestination const*> destinations;
    std::vector<LocationVariableInformation const*> locationVars;
    destinations.reserve(iteratorList.size());
    locationVars.reserve(iteratorList.size());

    for (uint64_t destinationId = 0; destinationId < numDestinations; ++destinationId) {
        // First assignment level
        destinations.clear();
        locationVars.clear();
        transientVariableValuation.clear();
        CompressedState successorState = state;
        ValueType successorProbability = storm::utility::one<ValueType>();

        uint64_t destinationIndex = destinationId;
        for (uint64_t i = 0; i < iteratorList.size(); ++i) {
            storm::jani::Edge const& edge = *iteratorList[i]->second;
            STORM_LOG_ASSERT(edge.getNumberOfDestinations() > 0, "Found an edge with zero destinations. This is not expected.");
            uint64_t localDestinationIndex = destinationIndex % edge.getNumberOfDestinations();
            destinations.push_back(&edge.getDestination(localDestinationIndex));
            locationVars.push_back(&this->variableInformation.locationVariables[edgeCombination[i].first]);
            destinationIndex /= edge.getNumberOfDestinations();
            ValueType probability = this->evaluator->asRational(destinations.back()->getProbability());
            if (edge.hasRate()) {
                successorProbability *= probability * this->evaluator->asRational(edge.getRate());
            } else {
                successorProbability *= probability;
            }
            if (storm::utility::isZero(successorProbability)) {
                break;
            }

            applyUpdate(successorState, *destinations.back(), *locationVars.back(), lowestDestinationAssignmentLevel, *this->evaluator);
            applyTransientUpdate(transientVariableValuation,
                                 destinations.back()->getOrderedAssignments().getTransientAssignments(lowestDestinationAssignmentLevel), *this->evaluator);
        }

        if (!storm::utility::isZero(successorProbability)) {
            bool evaluatorChanged = false;
            // remaining assignment levels (if there are any)
            for (int64_t assignmentLevel = lowestDestinationAssignmentLevel + 1; assignmentLevel <= highestDestinationAssignmentLevel; ++assignmentLevel) {
                unpackStateIntoEvaluator(successorState, this->variableInformation, *this->evaluator);
                transientVariableValuation.setInEvaluator(*this->evaluator, this->getOptions().isExplorationChecksSet());
                transientVariableValuation.clear();
                evaluatorChanged = true;
                auto locationVarIt = locationVars.begin();
                for (auto const& destPtr : destinations) {
                    applyUpdate(successorState, *destPtr, **locationVarIt, assignmentLevel, *this->evaluator);
                    applyTransientUpdate(transientVariableValuation, destinations.back()->getOrderedAssignments().getTransientAssignments(assignmentLevel),
                                         *this->evaluator);
                    ++locationVarIt;
                }
            }
            if (!transientVariableValuation.empty()) {
                evaluatorChanged = true;
                transientVariableValuation.setInEvaluator(*this->evaluator, this->getOptions().isExplorationChecksSet());
            }
            if (evaluateRewardExpressionsAtDestinations) {
                unpackStateIntoEvaluator(successorState, this->variableInformation, *this->evaluator);
                evaluatorChanged = true;
                addEvaluatedRewardExpressions(stateActionRewards, successorProbability);
            }
            if (evaluatorChanged) {
                // Restore the old state information
                unpackStateIntoEvaluator(state, this->variableInformation, *this->evaluator);
                this->transientVariableInformation.setDefaultValuesInEvaluator(*this->evaluator);
            }

            StateType id = stateToIdCallback(successorState);
            distribution.add(id, successorProbability);
        }
    }
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::expandSynchronizingEdgeCombination(AutomataEdgeSets const& edgeCombination, uint64_t outputActionIndex,
                                                                                      CompressedState const& state, StateToIdCallback stateToIdCallback,
                                                                                      std::vector<Choice<ValueType>>& newChoices) {
    if (this->options.isExplorationChecksSet()) {
        // Check whether a global variable is written multiple times in any combination.
        checkGlobalVariableWritesValid(edgeCombination);
    }

    std::vector<EdgeSetWithIndices::const_iterator> iteratorList(edgeCombination.size());

    // Initialize the list of iterators.
    for (size_t i = 0; i < edgeCombination.size(); ++i) {
        iteratorList[i] = edgeCombination[i].second.cbegin();
    }

    storm::generator::Distribution<StateType, ValueType> distribution;

    // As long as there is one feasible combination of commands, keep on expanding it.
    bool done = false;
    while (!done) {
        distribution.clear();

        EdgeIndexSet edgeIndices;
        std::vector<ValueType> stateActionRewards(rewardExpressions.size(), storm::utility::zero<ValueType>());
        // old version without assignment levels generateSynchronizedDistribution(state, storm::utility::one<ValueType>(), 0, edgeCombination, iteratorList,
        // distribution, stateActionRewards, edgeIndices, stateToIdCallback);
        generateSynchronizedDistribution(state, edgeCombination, iteratorList, distribution, stateActionRewards, edgeIndices, stateToIdCallback);
        distribution.compress();

        // At this point, we applied all commands of the current command combination and newTargetStates
        // contains all target states and their respective probabilities. That means we are now ready to
        // add the choice to the list of transitions.
        newChoices.emplace_back(outputActionIndex);

        // Now create the actual distribution.
        Choice<ValueType>& choice = newChoices.back();

        // Add the edge indices if requested.
        if (this->getOptions().isBuildChoiceOriginsSet()) {
            choice.addOriginData(boost::any(std::move(edgeIndices)));
        }

        // Add the rewards to the choice.
        choice.addRewards(std::move(stateActionRewards));

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
            STORM_LOG_THROW(!this->isDiscreteTimeModel() || !storm::utility::isConstant(probabilitySum) || this->comparator.isOne(probabilitySum),
                            storm::exceptions::WrongFormatException,
                            "Sum of update probabilities do not sum to one for some edge (actually sum to " << probabilitySum << ").");
        }

        if (this->options.isBuildChoiceLabelsSet()) {
            if (outputActionIndex != storm::jani::Model::SILENT_ACTION_INDEX) {
                choice.addLabel(model.getAction(outputActionIndex).getName());
            }
        }

        // Now, check whether there is one more command combination to consider.
        bool movedIterator = false;
        for (uint64_t j = 0; !movedIterator && j < iteratorList.size(); ++j) {
            ++iteratorList[j];
            if (iteratorList[j] != edgeCombination[j].second.end()) {
                movedIterator = true;
            } else {
                // Reset the iterator to the beginning of the list.
                iteratorList[j] = edgeCombination[j].second.begin();
            }
        }

        done = !movedIterator;
    }
}

template<typename ValueType, typename StateType>
std::vector<Choice<ValueType>> JaniNextStateGenerator<ValueType, StateType>::getActionChoices(std::vector<uint64_t> const& locations,
                                                                                              CompressedState const& state, StateToIdCallback stateToIdCallback,
                                                                                              EdgeFilter const& edgeFilter) {
    std::vector<Choice<ValueType>> result;

    // To avoid reallocations, we declare some memory here here.
    // This vector will store for each automaton the set of edges with the current output and the current source location
    std::vector<EdgeSetWithIndices const*> edgeSetsMemory;
    // This vector will store the 'first' combination of edges that is productive.
    std::vector<typename EdgeSetWithIndices::const_iterator> edgeIteratorMemory;

    for (OutputAndEdges const& outputAndEdges : edges) {
        auto const& edges = outputAndEdges.second;
        if (edges.size() == 1) {
            // If the synch consists of just one element, it's non-synchronizing.
            auto const& nonsychingEdges = edges.front();
            uint64_t automatonIndex = nonsychingEdges.first;

            auto edgesIt = nonsychingEdges.second.find(locations[automatonIndex]);
            if (edgesIt != nonsychingEdges.second.end()) {
                for (auto const& indexAndEdge : edgesIt->second) {
                    if (edgeFilter != EdgeFilter::All) {
                        STORM_LOG_ASSERT(edgeFilter == EdgeFilter::WithRate || edgeFilter == EdgeFilter::WithoutRate, "Unexpected edge filter.");
                        if ((edgeFilter == EdgeFilter::WithRate) != indexAndEdge.second->hasRate()) {
                            continue;
                        }
                    }
                    if (!this->evaluator->asBool(indexAndEdge.second->getGuard())) {
                        continue;
                    }

                    uint64_t actionIndex = outputAndEdges.first ? outputAndEdges.first.get() : indexAndEdge.second->getActionIndex();
                    result.push_back(expandNonSynchronizingEdge(*indexAndEdge.second, actionIndex, automatonIndex, state, stateToIdCallback));

                    if (this->getOptions().isBuildChoiceOriginsSet()) {
                        auto modelAutomatonIndex = model.getAutomatonIndex(parallelAutomata[automatonIndex].get().getName());
                        EdgeIndexSet edgeIndex{model.encodeAutomatonAndEdgeIndices(modelAutomatonIndex, indexAndEdge.first)};
                        result.back().addOriginData(boost::any(std::move(edgeIndex)));
                    }

                    if (this->getOptions().isBuildChoiceLabelsSet()) {
                        if (actionIndex != storm::jani::Model::SILENT_ACTION_INDEX) {
                            result.back().addLabel(model.getAction(actionIndex).getName());
                        }
                    }
                }
            }
        } else {
            // If the element has more than one set of edges, we need to perform a synchronization.
            // We require that some output action for the synchronisation must have been set before.
            // This might be the silent action, if the Jani model does not specify an output action.
            STORM_LOG_ASSERT(outputAndEdges.first, "Need output action index for synchronization.");

            uint64_t outputActionIndex = outputAndEdges.first.get();

            // Find out whether this combination is productive
            bool productiveCombination = true;
            // First check, whether each automaton has at least one edge with the current output and the current source location
            // We will also store the edges of each automaton with the current outputAction
            edgeSetsMemory.clear();
            for (auto const& automatonAndEdges : outputAndEdges.second) {
                uint64_t automatonIndex = automatonAndEdges.first;
                LocationsAndEdges const& locationsAndEdges = automatonAndEdges.second;
                auto edgesIt = locationsAndEdges.find(locations[automatonIndex]);
                if (edgesIt == locationsAndEdges.end()) {
                    productiveCombination = false;
                    break;
                }
                edgeSetsMemory.push_back(&edgesIt->second);
            }

            if (productiveCombination) {
                // second, check whether each automaton has at least one enabled action
                edgeIteratorMemory.clear();  // Store the first enabled edge in each automaton.
                for (auto const& edgesIt : edgeSetsMemory) {
                    bool atLeastOneEdge = false;
                    EdgeSetWithIndices const& edgeSetWithIndices = *edgesIt;
                    for (auto indexAndEdgeIt = edgeSetWithIndices.begin(), indexAndEdgeIte = edgeSetWithIndices.end(); indexAndEdgeIt != indexAndEdgeIte;
                         ++indexAndEdgeIt) {
                        // check whether we do not consider this edge
                        if (edgeFilter != EdgeFilter::All) {
                            STORM_LOG_ASSERT(edgeFilter == EdgeFilter::WithRate || edgeFilter == EdgeFilter::WithoutRate, "Unexpected edge filter.");
                            if ((edgeFilter == EdgeFilter::WithRate) != indexAndEdgeIt->second->hasRate()) {
                                continue;
                            }
                        }

                        if (!this->evaluator->asBool(indexAndEdgeIt->second->getGuard())) {
                            continue;
                        }

                        // If we reach this point, the edge is considered enabled.
                        atLeastOneEdge = true;
                        edgeIteratorMemory.push_back(indexAndEdgeIt);
                        break;
                    }

                    // If there is no enabled edge of this automaton, the whole combination is not productive.
                    if (!atLeastOneEdge) {
                        productiveCombination = false;
                        break;
                    }
                }
            }

            // produce the combination
            if (productiveCombination) {
                AutomataEdgeSets automataEdgeSets;
                automataEdgeSets.reserve(outputAndEdges.second.size());
                STORM_LOG_ASSERT(edgeSetsMemory.size() == outputAndEdges.second.size(), "Unexpected number of edge sets stored.");
                STORM_LOG_ASSERT(edgeIteratorMemory.size() == outputAndEdges.second.size(), "Unexpected number of edge iterators stored.");
                auto edgeSetIt = edgeSetsMemory.begin();
                auto edgeIteratorIt = edgeIteratorMemory.begin();
                for (auto const& automatonAndEdges : outputAndEdges.second) {
                    EdgeSetWithIndices enabledEdgesOfAutomaton;
                    uint64_t automatonIndex = automatonAndEdges.first;
                    EdgeSetWithIndices const& edgeSetWithIndices = **edgeSetIt;
                    auto indexAndEdgeIt = *edgeIteratorIt;
                    // The first edge where the edgeIterator points to is always enabled.
                    enabledEdgesOfAutomaton.emplace_back(*indexAndEdgeIt);
                    auto indexAndEdgeIte = edgeSetWithIndices.end();
                    for (++indexAndEdgeIt; indexAndEdgeIt != indexAndEdgeIte; ++indexAndEdgeIt) {
                        // check whether we do not consider this edge
                        if (edgeFilter != EdgeFilter::All) {
                            STORM_LOG_ASSERT(edgeFilter == EdgeFilter::WithRate || edgeFilter == EdgeFilter::WithoutRate, "Unexpected edge filter.");
                            if ((edgeFilter == EdgeFilter::WithRate) != indexAndEdgeIt->second->hasRate()) {
                                continue;
                            }
                        }

                        if (!this->evaluator->asBool(indexAndEdgeIt->second->getGuard())) {
                            continue;
                        }
                        // If we reach this point, the edge is considered enabled.
                        enabledEdgesOfAutomaton.emplace_back(*indexAndEdgeIt);
                    }
                    automataEdgeSets.emplace_back(std::move(automatonIndex), std::move(enabledEdgesOfAutomaton));
                    ++edgeSetIt;
                    ++edgeIteratorIt;
                }
                // insert choices in the result vector.
                expandSynchronizingEdgeCombination(automataEdgeSets, outputActionIndex, state, stateToIdCallback, result);
            }
        }
    }

    return result;
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::checkGlobalVariableWritesValid(AutomataEdgeSets const& enabledEdges) const {
    // Todo: this also throws if the writes are on different assignment level
    // Todo: this also throws if the writes are on different elements of the same array
    std::map<storm::expressions::Variable, uint64_t> writtenGlobalVariables;
    for (auto edgeSetIt = enabledEdges.begin(), edgeSetIte = enabledEdges.end(); edgeSetIt != edgeSetIte; ++edgeSetIt) {
        for (auto const& indexAndEdge : edgeSetIt->second) {
            for (auto const& globalVariable : indexAndEdge.second->getWrittenGlobalVariables()) {
                auto it = writtenGlobalVariables.find(globalVariable);

                auto index = std::distance(enabledEdges.begin(), edgeSetIt);
                if (it != writtenGlobalVariables.end()) {
                    STORM_LOG_THROW(it->second == static_cast<uint64_t>(index), storm::exceptions::WrongFormatException,
                                    "Multiple writes to global variable '" << globalVariable.getName() << "' in synchronizing edges.");
                } else {
                    writtenGlobalVariables.emplace(globalVariable, index);
                }
            }
        }
    }
}

template<typename ValueType, typename StateType>
std::size_t JaniNextStateGenerator<ValueType, StateType>::getNumberOfRewardModels() const {
    return rewardExpressions.size();
}

template<typename ValueType, typename StateType>
storm::builder::RewardModelInformation JaniNextStateGenerator<ValueType, StateType>::getRewardModelInformation(uint64_t const& index) const {
    return rewardModelInformation[index];
}

template<typename ValueType, typename StateType>
storm::models::sparse::StateLabeling JaniNextStateGenerator<ValueType, StateType>::label(storm::storage::sparse::StateStorage<StateType> const& stateStorage,
                                                                                         std::vector<StateType> const& initialStateIndices,
                                                                                         std::vector<StateType> const& deadlockStateIndices,
                                                                                         std::vector<StateType> const& unexploredStateIndices) {
    // As in JANI we can use transient boolean variable assignments in locations to identify states, we need to
    // create a list of boolean transient variables and the expressions that define them.
    std::vector<std::pair<std::string, storm::expressions::Expression>> transientVariableExpressions;
    for (auto const& variable : model.getGlobalVariables().getTransientVariables()) {
        if (variable.getType().isBasicType() && variable.getType().asBasicType().isBooleanType()) {
            if (this->options.isBuildAllLabelsSet() || this->options.getLabelNames().find(variable.getName()) != this->options.getLabelNames().end()) {
                transientVariableExpressions.emplace_back(variable.getName(), variable.getExpressionVariable().getExpression());
            }
        }
    }
    return NextStateGenerator<ValueType, StateType>::label(stateStorage, initialStateIndices, deadlockStateIndices, unexploredStateIndices,
                                                           transientVariableExpressions);
}

template<typename ValueType, typename StateType>
std::vector<ValueType> JaniNextStateGenerator<ValueType, StateType>::evaluateRewardExpressions() const {
    std::vector<ValueType> result;
    result.reserve(rewardExpressions.size());
    for (auto const& rewardExpression : rewardExpressions) {
        result.push_back(this->evaluator->asRational(rewardExpression.second));
    }
    return result;
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::addEvaluatedRewardExpressions(std::vector<ValueType>& rewards, ValueType const& factor) const {
    assert(rewards.size() == rewardExpressions.size());
    auto rewIt = rewards.begin();
    for (auto const& rewardExpression : rewardExpressions) {
        (*rewIt) += factor * this->evaluator->asRational(rewardExpression.second);
        ++rewIt;
    }
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::buildRewardModelInformation() {
    for (auto const& rewardModel : rewardExpressions) {
        storm::jani::RewardModelInformation info(this->model, rewardModel.second);
        rewardModelInformation.emplace_back(rewardModel.first, info.hasStateRewards(), false, false);
        STORM_LOG_THROW(this->options.isScaleAndLiftTransitionRewardsSet() || !info.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                        "Transition rewards are not supported and a reduction to action-based rewards was not possible.");
        if (info.hasTransitionRewards()) {
            evaluateRewardExpressionsAtDestinations = true;
        }
        if (info.hasActionRewards() || (this->options.isScaleAndLiftTransitionRewardsSet() && info.hasTransitionRewards())) {
            hasStateActionRewards = true;
            rewardModelInformation.back().setHasStateActionRewards();
        }
    }
    if (!hasStateActionRewards) {
        evaluateRewardExpressionsAtDestinations = false;
        evaluateRewardExpressionsAtEdges = false;
    }
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::createSynchronizationInformation() {
    // Create synchronizing edges information.
    storm::jani::Composition const& topLevelComposition = this->model.getSystemComposition();
    if (topLevelComposition.isAutomatonComposition()) {
        auto const& automaton = this->model.getAutomaton(topLevelComposition.asAutomatonComposition().getAutomatonName());
        this->parallelAutomata.push_back(automaton);

        LocationsAndEdges locationsAndEdges;
        uint64_t edgeIndex = 0;
        for (auto const& edge : automaton.getEdges()) {
            locationsAndEdges[edge.getSourceLocationIndex()].emplace_back(std::make_pair(edgeIndex, &edge));
            ++edgeIndex;
        }

        AutomataAndEdges automataAndEdges;
        automataAndEdges.emplace_back(std::make_pair(0, std::move(locationsAndEdges)));

        this->edges.emplace_back(std::make_pair(boost::none, std::move(automataAndEdges)));
    } else {
        STORM_LOG_THROW(topLevelComposition.isParallelComposition(), storm::exceptions::WrongFormatException, "Expected parallel composition.");
        storm::jani::ParallelComposition const& parallelComposition = topLevelComposition.asParallelComposition();

        uint64_t automatonIndex = 0;
        for (auto const& composition : parallelComposition.getSubcompositions()) {
            STORM_LOG_THROW(composition->isAutomatonComposition(), storm::exceptions::WrongFormatException, "Expected flat parallel composition.");
            STORM_LOG_THROW(composition->asAutomatonComposition().getInputEnabledActions().empty(), storm::exceptions::NotSupportedException,
                            "Input-enabled actions are not supported right now.");

            this->parallelAutomata.push_back(this->model.getAutomaton(composition->asAutomatonComposition().getAutomatonName()));

            // Add edges with silent action.
            LocationsAndEdges locationsAndEdges;
            uint64_t edgeIndex = 0;
            for (auto const& edge : parallelAutomata.back().get().getEdges()) {
                if (edge.getActionIndex() == storm::jani::Model::SILENT_ACTION_INDEX) {
                    locationsAndEdges[edge.getSourceLocationIndex()].emplace_back(std::make_pair(edgeIndex, &edge));
                }
                ++edgeIndex;
            }

            if (!locationsAndEdges.empty()) {
                AutomataAndEdges automataAndEdges;
                automataAndEdges.emplace_back(std::make_pair(automatonIndex, std::move(locationsAndEdges)));
                this->edges.emplace_back(std::make_pair(boost::none, std::move(automataAndEdges)));
            }
            ++automatonIndex;
        }

        for (auto const& vector : parallelComposition.getSynchronizationVectors()) {
            uint64_t outputActionIndex = this->model.getActionIndex(vector.getOutput());

            AutomataAndEdges automataAndEdges;
            bool atLeastOneEdge = true;
            uint64_t automatonIndex = 0;
            for (auto const& element : vector.getInput()) {
                if (!storm::jani::SynchronizationVector::isNoActionInput(element)) {
                    LocationsAndEdges locationsAndEdges;
                    uint64_t actionIndex = this->model.getActionIndex(element);
                    uint64_t edgeIndex = 0;
                    for (auto const& edge : parallelAutomata[automatonIndex].get().getEdges()) {
                        if (edge.getActionIndex() == actionIndex) {
                            locationsAndEdges[edge.getSourceLocationIndex()].emplace_back(std::make_pair(edgeIndex, &edge));
                        }
                        ++edgeIndex;
                    }
                    if (locationsAndEdges.empty()) {
                        atLeastOneEdge = false;
                        break;
                    }
                    automataAndEdges.emplace_back(std::make_pair(automatonIndex, std::move(locationsAndEdges)));
                }
                ++automatonIndex;
            }

            if (atLeastOneEdge) {
                this->edges.emplace_back(std::make_pair(outputActionIndex, std::move(automataAndEdges)));
            }
        }
    }

    STORM_LOG_TRACE("Number of synchronizations: " << this->edges.size() << ".");
}

template<typename ValueType, typename StateType>
std::shared_ptr<storm::storage::sparse::ChoiceOrigins> JaniNextStateGenerator<ValueType, StateType>::generateChoiceOrigins(
    std::vector<boost::any>& dataForChoiceOrigins) const {
    if (!this->getOptions().isBuildChoiceOriginsSet()) {
        return nullptr;
    }

    std::vector<uint_fast64_t> identifiers;
    identifiers.reserve(dataForChoiceOrigins.size());

    std::map<EdgeIndexSet, uint_fast64_t> edgeIndexSetToIdentifierMap;
    // The empty edge set (i.e., the choices without origin) always has to get identifier getIdentifierForChoicesWithNoOrigin() -- which is assumed to be 0
    STORM_LOG_ASSERT(storm::storage::sparse::ChoiceOrigins::getIdentifierForChoicesWithNoOrigin() == 0, "The no origin identifier is assumed to be zero");
    edgeIndexSetToIdentifierMap.insert(std::make_pair(EdgeIndexSet(), 0));
    uint_fast64_t currentIdentifier = 1;
    for (boost::any& originData : dataForChoiceOrigins) {
        STORM_LOG_ASSERT(originData.empty() || boost::any_cast<EdgeIndexSet>(&originData) != nullptr,
                         "Origin data has unexpected type: " << originData.type().name() << ".");

        EdgeIndexSet currentEdgeIndexSet = originData.empty() ? EdgeIndexSet() : boost::any_cast<EdgeIndexSet>(std::move(originData));
        auto insertionRes = edgeIndexSetToIdentifierMap.emplace(std::move(currentEdgeIndexSet), currentIdentifier);
        identifiers.push_back(insertionRes.first->second);
        if (insertionRes.second) {
            ++currentIdentifier;
        }
    }

    std::vector<EdgeIndexSet> identifierToEdgeIndexSetMapping(currentIdentifier);
    for (auto const& setIdPair : edgeIndexSetToIdentifierMap) {
        identifierToEdgeIndexSetMapping[setIdPair.second] = setIdPair.first;
    }

    return std::make_shared<storm::storage::sparse::JaniChoiceOrigins>(std::make_shared<storm::jani::Model>(model), std::move(identifiers),
                                                                       std::move(identifierToEdgeIndexSetMapping));
}

template<typename ValueType, typename StateType>
storm::storage::BitVector JaniNextStateGenerator<ValueType, StateType>::evaluateObservationLabels(CompressedState const& /*state*/) const {
    STORM_LOG_WARN("There are no observation labels in JANI currenty");
    return storm::storage::BitVector(0);
}

template<typename ValueType, typename StateType>
void JaniNextStateGenerator<ValueType, StateType>::checkValid() const {
    // If the program still contains undefined constants and we are not in a parametric setting, assemble an appropriate error message.
    if (!std::is_same<ValueType, storm::RationalFunction>::value && model.hasUndefinedConstants()) {
        std::vector<std::reference_wrapper<storm::jani::Constant const>> undefinedConstants = model.getUndefinedConstants();
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
    } else if (std::is_same<ValueType, storm::RationalFunction>::value && !model.undefinedConstantsAreGraphPreserving()) {
        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                        "The input model contains undefined constants that influence the graph structure of the underlying model, which is not allowed.");
    }
}

template class JaniNextStateGenerator<double>;
template class JaniNextStateGenerator<storm::RationalNumber>;
template class JaniNextStateGenerator<storm::RationalFunction>;
}  // namespace generator
}  // namespace storm
