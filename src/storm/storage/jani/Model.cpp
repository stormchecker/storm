#include "storm/storage/jani/Model.h"

#include <algorithm>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/InvalidTypeException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/expressions/LinearityCheckVisitor.h"
#include "storm/storage/jani/Automaton.h"
#include "storm/storage/jani/AutomatonComposition.h"
#include "storm/storage/jani/Compositions.h"
#include "storm/storage/jani/Edge.h"
#include "storm/storage/jani/EdgeDestination.h"
#include "storm/storage/jani/Location.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/ParallelComposition.h"
#include "storm/storage/jani/TemplateEdge.h"
#include "storm/storage/jani/VariablesToConstantsTransformer.h"
#include "storm/storage/jani/eliminator/ArrayEliminator.h"
#include "storm/storage/jani/eliminator/FunctionEliminator.h"
#include "storm/storage/jani/traverser/InformationCollector.h"
#include "storm/storage/jani/visitor/CompositionInformationVisitor.h"
#include "storm/storage/jani/visitor/JSONExporter.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/utility/combinatorics.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace jani {

const std::string Model::SILENT_ACTION_NAME = "";
const uint64_t Model::SILENT_ACTION_INDEX = 0;

Model::Model(Model&& other) = default;
Model& Model::operator=(Model&& other) = default;

Model::Model() {
    // Intentionally left empty.
}

Model::Model(std::string const& name, ModelType const& modelType, uint64_t version,
             boost::optional<std::shared_ptr<storm::expressions::ExpressionManager>> const& expressionManager)
    : name(name), modelType(modelType), version(version), composition(nullptr) {
    // Use the provided manager or create a new one.
    if (expressionManager) {
        this->expressionManager = expressionManager.get();
    } else {
        this->expressionManager = std::make_shared<storm::expressions::ExpressionManager>();
    }

    // Create an initial restriction.
    initialStatesRestriction = this->expressionManager->boolean(true);

    // Add a prefined action that represents the silent action.
    [[maybe_unused]] uint64_t actionIndex = addAction(storm::jani::Action(SILENT_ACTION_NAME));
    STORM_LOG_ASSERT(actionIndex == SILENT_ACTION_INDEX, "Illegal silent action index.");
}

Model::Model(Model const& other) {
    *this = other;
}

Model& Model::operator=(Model const& other) {
    if (this != &other) {
        this->name = other.name;
        this->modelType = other.modelType;
        this->modelFeatures = other.modelFeatures;
        this->version = other.version;
        this->expressionManager = other.expressionManager;
        this->actions = other.actions;
        this->actionToIndex = other.actionToIndex;
        this->nonsilentActionIndices = other.nonsilentActionIndices;
        this->constants = other.constants;
        this->constantToIndex = other.constantToIndex;
        this->globalVariables = other.globalVariables;
        this->nonTrivialRewardModels = other.nonTrivialRewardModels;
        this->automata = other.automata;
        this->automatonToIndex = other.automatonToIndex;
        this->composition = other.composition;
        this->initialStatesRestriction = other.initialStatesRestriction;
        this->globalFunctions = other.globalFunctions;

        // Now that we have copied all the data, we need to fix all assignments as they contain references to the old model.
        std::map<Variable const*, std::reference_wrapper<Variable const>> remapping;
        for (auto const& variable : other.getGlobalVariables()) {
            remapping.emplace(&variable, this->getGlobalVariables().getVariable(variable.getName()));
        }
        auto otherAutomatonIt = other.automata.begin();
        auto thisAutomatonIt = this->automata.begin();

        for (; otherAutomatonIt != other.automata.end(); ++otherAutomatonIt, ++thisAutomatonIt) {
            for (auto const& variable : otherAutomatonIt->getVariables()) {
                remapping.emplace(&variable, thisAutomatonIt->getVariables().getVariable(variable.getName()));
            }

            thisAutomatonIt->changeAssignmentVariables(remapping);
        }
    }

    return *this;
}

storm::expressions::ExpressionManager& Model::getManager() const {
    return *expressionManager;
}

uint64_t Model::getJaniVersion() const {
    return version;
}

ModelType const& Model::getModelType() const {
    return modelType;
}

void Model::setModelType(ModelType const& newModelType) {
    modelType = newModelType;
}

ModelFeatures const& Model::getModelFeatures() const {
    return modelFeatures;
}

ModelFeatures& Model::getModelFeatures() {
    return modelFeatures;
}

std::string const& Model::getName() const {
    return name;
}

void Model::setName(std::string const& newName) {
    name = newName;
}

struct ConditionalMetaEdge {
    ConditionalMetaEdge() : actionIndex(0) {
        // Intentionally left empty.
    }

    uint64_t actionIndex;
    std::vector<uint64_t> components;
    std::vector<uint64_t> condition;
    boost::optional<storm::expressions::Expression> rate;
    std::vector<storm::expressions::Expression> probabilities;
    std::vector<std::vector<uint64_t>> effects;
    std::shared_ptr<TemplateEdge> templateEdge;
};

storm::expressions::Expression createSynchronizedGuard(std::vector<std::reference_wrapper<Edge const>> const& chosenEdges) {
    STORM_LOG_ASSERT(!chosenEdges.empty(), "Expected non-empty set of edges.");
    auto it = chosenEdges.begin();
    storm::expressions::Expression result = it->get().getGuard();
    ++it;
    for (; it != chosenEdges.end(); ++it) {
        result = result && it->get().getGuard();
    }
    return result;
}

ConditionalMetaEdge createSynchronizedMetaEdge(Automaton& automaton, std::vector<std::reference_wrapper<Edge const>> const& edgesToSynchronize) {
    ConditionalMetaEdge result;

    result.templateEdge = std::make_shared<TemplateEdge>(createSynchronizedGuard(edgesToSynchronize));
    automaton.registerTemplateEdge(result.templateEdge);

    for (auto const& edge : edgesToSynchronize) {
        result.condition.push_back(edge.get().getSourceLocationIndex());
    }

    // Initialize all update iterators.
    std::vector<std::vector<EdgeDestination>::const_iterator> destinationIterators;
    for (uint_fast64_t i = 0; i < edgesToSynchronize.size(); ++i) {
        destinationIterators.push_back(edgesToSynchronize[i].get().getDestinations().cbegin());
    }

    bool doneDestinations = false;
    do {
        // We create the new likelihood expression by multiplying the particapting destination probability expressions.
        result.probabilities.emplace_back(destinationIterators[0]->getProbability());
        for (uint_fast64_t i = 1; i < destinationIterators.size(); ++i) {
            result.probabilities.back() = result.probabilities.back() * destinationIterators[i]->getProbability();
        }

        // Now concatenate all assignments of all participating destinations.
        TemplateEdgeDestination templateDestination;
        for (uint_fast64_t i = 0; i < destinationIterators.size(); ++i) {
            for (auto const& assignment : destinationIterators[i]->getOrderedAssignments().getAllAssignments()) {
                templateDestination.addAssignment(assignment);
            }
        }

        // Then we are ready to add the new destination.
        result.templateEdge->addDestination(templateDestination);

        // Finally, add the location effects.
        result.effects.emplace_back();
        for (uint_fast64_t i = 0; i < destinationIterators.size(); ++i) {
            result.effects.back().push_back(destinationIterators[i]->getLocationIndex());
        }

        // Now check whether there is some update combination we have not yet explored.
        bool movedIterator = false;
        for (int_fast64_t j = destinationIterators.size() - 1; j >= 0; --j) {
            ++destinationIterators[j];
            if (destinationIterators[j] != edgesToSynchronize[j].get().getDestinations().cend()) {
                movedIterator = true;
                break;
            } else {
                // Reset the iterator to the beginning of the list.
                destinationIterators[j] = edgesToSynchronize[j].get().getDestinations().cbegin();
            }
        }

        doneDestinations = !movedIterator;
    } while (!doneDestinations);

    return result;
}

std::vector<ConditionalMetaEdge> createSynchronizingMetaEdges(Model const& oldModel, Model& newModel, Automaton& newAutomaton,
                                                              std::vector<std::set<uint64_t>>& synchronizingActionIndices, SynchronizationVector const& vector,
                                                              std::vector<std::reference_wrapper<Automaton const>> const& composedAutomata,
                                                              storm::solver::SmtSolver& solver) {
    std::vector<ConditionalMetaEdge> result;

    // Gather all participating automata and the corresponding input symbols.
    std::vector<uint64_t> components;
    std::vector<std::pair<std::reference_wrapper<Automaton const>, uint64_t>> participatingAutomataAndActions;
    for (uint64_t i = 0; i < composedAutomata.size(); ++i) {
        std::string const& actionName = vector.getInput(i);
        if (!SynchronizationVector::isNoActionInput(actionName)) {
            components.push_back(i);
            uint64_t actionIndex = oldModel.getActionIndex(actionName);
            // store that automaton occurs in the sync vector.
            participatingAutomataAndActions.push_back(std::make_pair(composedAutomata[i], actionIndex));
            // Store for later that this action is one of the possible actions that synchronise
            synchronizingActionIndices[i].insert(actionIndex);
        }
    }

    // What is the action label that should be attached to the composed actions
    uint64_t resultingActionIndex = Model::SILENT_ACTION_INDEX;
    if (vector.getOutput() != Model::SILENT_ACTION_NAME) {
        if (newModel.hasAction(vector.getOutput())) {
            resultingActionIndex = newModel.getActionIndex(vector.getOutput());
        } else {
            resultingActionIndex = newModel.addAction(vector.getOutput());
        }
    }

    bool noCombinations = false;

    // Prepare the list that stores for each automaton the list of edges with the participating action.
    std::vector<std::vector<std::reference_wrapper<storm::jani::Edge const>>> possibleEdges;

    for (auto const& automatonActionPair : participatingAutomataAndActions) {
        possibleEdges.emplace_back();
        for (auto const& edge : automatonActionPair.first.get().getEdges()) {
            if (edge.getActionIndex() == automatonActionPair.second) {
                possibleEdges.back().push_back(edge);
            }
        }

        // If there were no edges with the participating action index, then there is no synchronization possible.
        if (possibleEdges.back().empty()) {
            noCombinations = true;
            break;
        }
    }

    // If there are no valid combinations for the action, we need to skip the generation of synchronizing edges.
    if (!noCombinations) {
        // Save state of solver so that we can always restore the point where we have exactly the constant values
        // and variables bounds on the assertion stack.
        solver.push();

        // Start by creating a fresh auxiliary variable for each edge and link it with the guard.
        std::vector<std::vector<storm::expressions::Variable>> edgeVariables(possibleEdges.size());
        std::vector<storm::expressions::Variable> allEdgeVariables;
        for (uint_fast64_t outerIndex = 0; outerIndex < possibleEdges.size(); ++outerIndex) {
            // Create auxiliary variables and link them with the guards.
            for (uint_fast64_t innerIndex = 0; innerIndex < possibleEdges[outerIndex].size(); ++innerIndex) {
                edgeVariables[outerIndex].push_back(newModel.getManager().declareFreshBooleanVariable());
                allEdgeVariables.push_back(edgeVariables[outerIndex].back());
                storm::expressions::Expression guard = eliminateFunctionCallsInExpression(possibleEdges[outerIndex][innerIndex].get().getGuard(), oldModel);
                solver.add(implies(edgeVariables[outerIndex].back(), guard));
            }

            storm::expressions::Expression atLeastOneEdgeFromAutomaton = newModel.getManager().boolean(false);
            for (auto const& edgeVariable : edgeVariables[outerIndex]) {
                atLeastOneEdgeFromAutomaton = atLeastOneEdgeFromAutomaton || edgeVariable;
            }
            solver.add(atLeastOneEdgeFromAutomaton);

            storm::expressions::Expression atMostOneEdgeFromAutomaton = newModel.getManager().boolean(true);
            for (uint64_t first = 0; first < possibleEdges[outerIndex].size(); ++first) {
                for (uint64_t second = first + 1; second < possibleEdges[outerIndex].size(); ++second) {
                    atMostOneEdgeFromAutomaton = atMostOneEdgeFromAutomaton && !(edgeVariables[outerIndex][first] && edgeVariables[outerIndex][second]);
                }
            }
            solver.add(atMostOneEdgeFromAutomaton);
        }

        // Now enumerate all possible combinations.
        solver.allSat(allEdgeVariables, [&](storm::solver::SmtSolver::ModelReference& modelReference) -> bool {
            // Now we need to reconstruct the chosen edges from the valuation of the edge variables.
            std::vector<std::reference_wrapper<Edge const>> chosenEdges;

            for (uint_fast64_t outerIndex = 0; outerIndex < edgeVariables.size(); ++outerIndex) {
                for (uint_fast64_t innerIndex = 0; innerIndex < edgeVariables[outerIndex].size(); ++innerIndex) {
                    if (modelReference.getBooleanValue(edgeVariables[outerIndex][innerIndex])) {
                        chosenEdges.emplace_back(possibleEdges[outerIndex][innerIndex]);
                        break;
                    }
                }
            }

            // Get a basic conditional meta edge that represents the synchronization of the provided edges.
            // Note that there is still information missing, which we need to add (like the action index etc.).
            ConditionalMetaEdge conditionalMetaEdge = createSynchronizedMetaEdge(newAutomaton, chosenEdges);

            // Set the participating components.
            conditionalMetaEdge.components = components;

            // Set the action index.
            conditionalMetaEdge.actionIndex = resultingActionIndex;

            result.push_back(conditionalMetaEdge);

            return true;
        });

        solver.pop();
    }

    return result;
}

void createCombinedLocation(std::vector<std::reference_wrapper<Automaton const>> const& composedAutomata, Automaton& newAutomaton,
                            std::vector<uint64_t> const& locations, bool initial = false) {
    std::stringstream locationNameBuilder;
    for (uint64_t i = 0; i < locations.size(); ++i) {
        locationNameBuilder << composedAutomata[i].get().getLocation(locations[i]).getName() << "_";
    }

    uint64_t locationIndex = newAutomaton.addLocation(Location(locationNameBuilder.str()));
    Location& location = newAutomaton.getLocation(locationIndex);
    for (uint64_t i = 0; i < locations.size(); ++i) {
        for (auto const& assignment : composedAutomata[i].get().getLocation(locations[i]).getAssignments()) {
            location.addTransientAssignment(assignment);
        }
    }

    if (initial) {
        newAutomaton.addInitialLocation(locationIndex);
    }
}

void addEdgesToReachableLocations(std::vector<std::reference_wrapper<Automaton const>> const& composedAutomata, Automaton& newAutomaton,
                                  std::vector<ConditionalMetaEdge> const& conditionalMetaEdges) {
    // Maintain a stack of locations that still need to be to explored.
    std::vector<std::vector<uint64_t>> locationsToExplore;

    // Enumerate all initial location combinations.
    std::vector<std::set<uint64_t>::const_iterator> initialLocationsIts;
    std::vector<std::set<uint64_t>::const_iterator> initialLocationsItes;
    for (auto const& automaton : composedAutomata) {
        initialLocationsIts.push_back(automaton.get().getInitialLocationIndices().cbegin());
        initialLocationsItes.push_back(automaton.get().getInitialLocationIndices().cend());
    }
    std::vector<uint64_t> initialLocation(composedAutomata.size());
    storm::utility::combinatorics::forEach(
        initialLocationsIts, initialLocationsItes, [&initialLocation](uint64_t index, uint64_t value) { initialLocation[index] = value; },
        [&locationsToExplore, &initialLocation]() {
            locationsToExplore.push_back(initialLocation);
            return true;
        });

    // We also maintain a mapping from location combinations to new locations.
    std::unordered_map<std::vector<uint64_t>, uint64_t, storm::utility::vector::VectorHash<uint64_t>> newLocationMapping;

    // Register all initial locations as new locations.
    for (auto const& location : locationsToExplore) {
        uint64_t id = newLocationMapping.size();
        newLocationMapping[location] = id;
        createCombinedLocation(composedAutomata, newAutomaton, location, true);
    }

    // As long as there are locations to explore, do so.
    while (!locationsToExplore.empty()) {
        std::vector<uint64_t> currentLocations = std::move(locationsToExplore.back());
        locationsToExplore.pop_back();

        for (auto const& metaEdge : conditionalMetaEdges) {
            bool isApplicable = true;
            for (uint64_t i = 0; i < metaEdge.components.size(); ++i) {
                if (currentLocations[metaEdge.components[i]] != metaEdge.condition[i]) {
                    isApplicable = false;
                    break;
                }
            }

            if (isApplicable) {
                std::vector<uint64_t> newLocations;

                for (auto const& effect : metaEdge.effects) {
                    std::vector<uint64_t> targetLocationCombination = currentLocations;
                    for (uint64_t i = 0; i < metaEdge.components.size(); ++i) {
                        targetLocationCombination[metaEdge.components[i]] = effect[i];
                    }

                    // Check whether the target combination is new.
                    auto it = newLocationMapping.find(targetLocationCombination);
                    if (it != newLocationMapping.end()) {
                        newLocations.emplace_back(it->second);
                    } else {
                        uint64_t id = newLocationMapping.size();
                        newLocationMapping[targetLocationCombination] = id;
                        locationsToExplore.emplace_back(std::move(targetLocationCombination));
                        newLocations.emplace_back(id);
                        createCombinedLocation(composedAutomata, newAutomaton, newLocations);
                    }
                }

                newAutomaton.addEdge(Edge(newLocationMapping.at(currentLocations), metaEdge.actionIndex, metaEdge.rate, metaEdge.templateEdge, newLocations,
                                          metaEdge.probabilities));
            }
        }
    }
}

Model Model::flattenComposition(std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory) const {
    // If there is only one automaton and then system composition is the standard one, we don't need to modify
    // the model.
    if (this->getNumberOfAutomata() == 1 && this->hasStandardComposition()) {
        return *this;
    }

    // Check for current restrictions of flatting process.
    STORM_LOG_THROW(this->hasStandardCompliantComposition(), storm::exceptions::WrongFormatException,
                    "Flatting composition is only supported for standard-compliant compositions.");
    STORM_LOG_THROW(this->getModelType() == ModelType::DTMC || this->getModelType() == ModelType::MDP, storm::exceptions::InvalidTypeException,
                    "Unable to flatten modules for model of type '" << this->getModelType() << "'.");
    STORM_LOG_WARN_COND(!this->getModelFeatures().hasArrays(),
                        "Flattening JANI model with arrays is not supported. We'll try but there might be unexpected errors.");
    if (this->getModelFeatures().hasFunctions()) {
        for (auto const& aut : automata) {
            STORM_LOG_THROW(aut.getFunctionDefinitions().empty(), storm::exceptions::NotImplementedException,
                            "Flattening JANI model with local function declarations not implemented. Try to eliminate functions first or make them global.");
        }
    }

    // Otherwise, we need to actually flatten composition.
    Model flattenedModel(this->getName() + "_flattened", this->getModelType(), this->getJaniVersion(), this->getManager().shared_from_this());

    flattenedModel.getModelFeatures() = getModelFeatures();

    // Get an SMT solver for computing possible guard combinations.
    std::unique_ptr<storm::solver::SmtSolver> solver = smtSolverFactory->create(*expressionManager);

    Composition const& systemComposition = getSystemComposition();
    if (systemComposition.isAutomatonComposition()) {
        AutomatonComposition const& automatonComposition = systemComposition.asAutomatonComposition();
        STORM_LOG_THROW(automatonComposition.getInputEnabledActions().empty(), storm::exceptions::WrongFormatException,
                        "Flatting does not support input-enabling actions.");
        return createModelFromAutomaton(getAutomaton(automatonComposition.getAutomatonName()));
    }

    // Ensure that we have a parallel composition from now on.
    STORM_LOG_THROW(systemComposition.isParallelComposition(), storm::exceptions::WrongFormatException, "Unknown system composition cannot be flattened.");
    ParallelComposition const& parallelComposition = systemComposition.asParallelComposition();

    // Create the new automaton that will hold the flattened system.
    Automaton newAutomaton(this->getName() + "_flattened", expressionManager->declareIntegerVariable("_loc_flattened_" + this->getName()));

    std::map<Variable const*, std::reference_wrapper<Variable const>> variableRemapping;
    for (auto const& variable : getGlobalVariables()) {
        std::unique_ptr<Variable> renamedVariable = variable.clone();
        variableRemapping.emplace(&variable, flattenedModel.addVariable(*renamedVariable));
    }

    for (auto const& constant : getConstants()) {
        flattenedModel.addConstant(constant);
    }

    for (auto const& nonTrivRew : getNonTrivialRewardExpressions()) {
        flattenedModel.addNonTrivialRewardExpression(nonTrivRew.first, nonTrivRew.second);
    }

    for (auto const& funDef : getGlobalFunctionDefinitions()) {
        flattenedModel.addFunctionDefinition(funDef.second);
    }

    std::vector<std::reference_wrapper<Automaton const>> composedAutomata;
    for (auto const& element : parallelComposition.getSubcompositions()) {
        STORM_LOG_THROW(element->isAutomatonComposition(), storm::exceptions::WrongFormatException,
                        "Cannot flatten recursive (not standard-compliant) composition.");
        AutomatonComposition const& automatonComposition = element->asAutomatonComposition();
        STORM_LOG_THROW(automatonComposition.getInputEnabledActions().empty(), storm::exceptions::WrongFormatException,
                        "Flatting does not support input-enabling actions.");
        Automaton const& oldAutomaton = this->getAutomaton(automatonComposition.getAutomatonName());
        composedAutomata.push_back(oldAutomaton);

        // Prefix all variables of this automaton with the automaton's name and add the to the resulting automaton.
        for (auto const& variable : oldAutomaton.getVariables()) {
            std::unique_ptr<Variable> renamedVariable = variable.clone();
            renamedVariable->setName(oldAutomaton.getName() + "_" + renamedVariable->getName());
            variableRemapping.emplace(&variable, newAutomaton.addVariable(*renamedVariable));
        }
    }

    // Prepare the solver.
    // Assert the values of the constants.
    for (auto const& constant : this->getConstants()) {
        if (constant.isDefined()) {
            if (constant.isBooleanConstant()) {
                solver->add(storm::expressions::iff(constant.getExpressionVariable(), constant.getExpression()));
            } else {
                solver->add(constant.getExpressionVariable() == constant.getExpression());
            }
        }
    }
    // Assert the bounds of the global variables.
    for (auto const& variable : newAutomaton.getVariables().getBoundedIntegerVariables()) {
        solver->add(variable.getRangeExpression());
    }

    // Perform all necessary synchronizations and keep track which action indices participate in synchronization.
    std::vector<std::set<uint64_t>> synchronizingActionIndices(composedAutomata.size());
    std::vector<ConditionalMetaEdge> conditionalMetaEdges;
    for (auto const& vector : parallelComposition.getSynchronizationVectors()) {
        // If less then 2 automata participate, there is no need to perform a synchronization.
        if (vector.getNumberOfActionInputs() <= 1) {
            continue;
        }

        // Create all conditional template edges corresponding to this synchronization vector.
        std::vector<ConditionalMetaEdge> newConditionalMetaEdges =
            createSynchronizingMetaEdges(*this, flattenedModel, newAutomaton, synchronizingActionIndices, vector, composedAutomata, *solver);
        conditionalMetaEdges.insert(conditionalMetaEdges.end(), newConditionalMetaEdges.begin(), newConditionalMetaEdges.end());
    }

    // Now add all edges with action indices that were not mentioned in synchronization vectors.
    for (uint64_t i = 0; i < composedAutomata.size(); ++i) {
        Automaton const& automaton = composedAutomata[i].get();
        for (auto const& edge : automaton.getEdges()) {
            if (synchronizingActionIndices[i].find(edge.getActionIndex()) == synchronizingActionIndices[i].end()) {
                uint64_t actionIndex = edge.getActionIndex();
                if (actionIndex != SILENT_ACTION_INDEX) {
                    std::string actionName = this->getActionIndexToNameMap().at(edge.getActionIndex());
                    if (flattenedModel.hasAction(actionName)) {
                        actionIndex = flattenedModel.getActionIndex(actionName);
                    } else {
                        actionIndex = flattenedModel.addAction(actionName);
                    }
                }

                conditionalMetaEdges.emplace_back();
                ConditionalMetaEdge& conditionalMetaEdge = conditionalMetaEdges.back();

                conditionalMetaEdge.templateEdge = std::make_shared<TemplateEdge>(edge.getGuard());
                newAutomaton.registerTemplateEdge(conditionalMetaEdge.templateEdge);
                conditionalMetaEdge.actionIndex = edge.getActionIndex();
                conditionalMetaEdge.components.emplace_back(static_cast<uint64_t>(i));
                conditionalMetaEdge.condition.emplace_back(edge.getSourceLocationIndex());
                conditionalMetaEdge.rate = edge.getOptionalRate();
                for (auto const& destination : edge.getDestinations()) {
                    conditionalMetaEdge.templateEdge->addDestination(destination.getOrderedAssignments());
                    conditionalMetaEdge.effects.emplace_back();

                    conditionalMetaEdge.effects.back().emplace_back(destination.getLocationIndex());
                    conditionalMetaEdge.probabilities.emplace_back(destination.getProbability());
                }
            }
        }
    }

    // Now that all meta edges have been built, we can explore the location space and add all edges based
    // on the templates.
    addEdgesToReachableLocations(composedAutomata, newAutomaton, conditionalMetaEdges);

    // Fix all variables mentioned in assignments by applying the constructed remapping.
    newAutomaton.changeAssignmentVariables(variableRemapping);

    // Finalize the flattened model.
    storm::expressions::Expression initialStatesRestriction = getManager().boolean(true);
    for (auto const& automaton : composedAutomata) {
        if (automaton.get().hasInitialStatesRestriction()) {
            initialStatesRestriction = initialStatesRestriction && automaton.get().getInitialStatesRestriction();
        }
        for (auto const& funDef : automaton.get().getFunctionDefinitions()) {
            newAutomaton.addFunctionDefinition(funDef.second);
        }
    }

    newAutomaton.setInitialStatesRestriction(this->getInitialStatesExpression(composedAutomata));
    if (this->hasInitialStatesRestriction()) {
        flattenedModel.setInitialStatesRestriction(this->getInitialStatesRestriction());
    }
    flattenedModel.addAutomaton(newAutomaton);
    flattenedModel.setStandardSystemComposition();
    flattenedModel.finalize();

    return flattenedModel;
}

uint64_t Model::addAction(Action const& action) {
    auto it = actionToIndex.find(action.getName());
    STORM_LOG_THROW(it == actionToIndex.end(), storm::exceptions::WrongFormatException, "Action with name '" << action.getName() << "' already exists.");
    actionToIndex.emplace(action.getName(), actions.size());
    actions.push_back(action);
    if (action.getName() != SILENT_ACTION_NAME) {
        nonsilentActionIndices.insert(actions.size() - 1);
    }
    return actions.size() - 1;
}

Action const& Model::getAction(uint64_t index) const {
    return actions[index];
}

bool Model::hasAction(std::string const& name) const {
    return actionToIndex.find(name) != actionToIndex.end();
}

uint64_t Model::getActionIndex(std::string const& name) const {
    auto it = actionToIndex.find(name);
    STORM_LOG_THROW(it != actionToIndex.end(), storm::exceptions::InvalidOperationException, "Unable to retrieve index of unknown action '" << name << "'.");
    return it->second;
}

std::unordered_map<std::string, uint64_t> const& Model::getActionToIndexMap() const {
    return actionToIndex;
}

std::vector<Action> const& Model::getActions() const {
    return actions;
}

storm::storage::FlatSet<uint64_t> const& Model::getNonsilentActionIndices() const {
    return nonsilentActionIndices;
}

void Model::addConstant(Constant const& constant) {
    auto it = constantToIndex.find(constant.getName());
    STORM_LOG_THROW(it == constantToIndex.end(), storm::exceptions::WrongFormatException,
                    "Cannot add constant with name '" << constant.getName() << "', because a constant with that name already exists.");
    constantToIndex.emplace(constant.getName(), constants.size());
    constants.push_back(constant);
    // Note that we should not return a reference to the inserted constant as it might get invalidated when more constants are added.
}

bool Model::hasConstant(std::string const& name) const {
    return constantToIndex.find(name) != constantToIndex.end();
}

void Model::removeConstant(std::string const& name) {
    auto pos = constantToIndex.find(name);
    if (pos != constantToIndex.end()) {
        uint64_t index = pos->second;
        constants.erase(constants.begin() + index);
        constantToIndex.erase(pos);
        for (auto& entry : constantToIndex) {
            if (entry.second > index) {
                entry.second--;
            }
        }
    } else {
        STORM_LOG_ERROR("Could not remove constant: " << name << ".");
    }
}

Constant const& Model::getConstant(std::string const& name) const {
    auto it = constantToIndex.find(name);
    STORM_LOG_THROW(it != constantToIndex.end(), storm::exceptions::WrongFormatException, "Unable to retrieve unknown constant '" << name << "'.");
    return constants[it->second];
}

std::vector<Constant> const& Model::getConstants() const {
    return constants;
}

std::vector<Constant>& Model::getConstants() {
    return constants;
}

std::size_t Model::getNumberOfEdges() const {
    size_t res = 0;
    for (auto const& aut : getAutomata()) {
        res += aut.getNumberOfEdges();
    }
    return res;
}

std::size_t Model::getTotalNumberOfNonTransientVariables() const {
    std::size_t res = globalVariables.getNumberOfNontransientVariables();
    for (auto const& aut : getAutomata()) {
        res += aut.getVariables().getNumberOfNontransientVariables();
    }
    return res;
}

InformationObject Model::getModelInformation() const {
    return collectModelInformation(*this);
}

Variable const& Model::addVariable(Variable const& variable) {
    return globalVariables.addVariable(variable);
}

VariableSet& Model::getGlobalVariables() {
    return globalVariables;
}

VariableSet const& Model::getGlobalVariables() const {
    return globalVariables;
}

std::set<storm::expressions::Variable> Model::getAllExpressionVariables(bool includeLocationExpressionVariables) const {
    std::set<storm::expressions::Variable> result;

    for (auto const& constant : constants) {
        result.insert(constant.getExpressionVariable());
    }
    for (auto const& variable : this->getGlobalVariables()) {
        result.insert(variable.getExpressionVariable());
    }
    for (auto const& automaton : automata) {
        auto const& automatonVariables = automaton.getAllExpressionVariables();
        result.insert(automatonVariables.begin(), automatonVariables.end());
        if (includeLocationExpressionVariables) {
            result.insert(automaton.getLocationExpressionVariable());
        }
    }

    return result;
}

std::set<storm::expressions::Variable> Model::getAllLocationExpressionVariables() const {
    std::set<storm::expressions::Variable> result;
    for (auto const& automaton : automata) {
        result.insert(automaton.getLocationExpressionVariable());
    }
    return result;
}

bool Model::hasGlobalVariable(std::string const& name) const {
    return globalVariables.hasVariable(name);
}

Variable const& Model::getGlobalVariable(std::string const& name) const {
    return globalVariables.getVariable(name);
}

bool Model::hasNonGlobalTransientVariable() const {
    for (auto const& automaton : automata) {
        if (automaton.hasTransientVariable()) {
            return true;
        }
    }
    return false;
}

FunctionDefinition const& Model::addFunctionDefinition(FunctionDefinition const& functionDefinition) {
    auto insertionRes = globalFunctions.emplace(functionDefinition.getName(), functionDefinition);
    STORM_LOG_THROW(insertionRes.second, storm::exceptions::InvalidOperationException,
                    " a function with the name " << functionDefinition.getName() << " already exists in this model.");
    return insertionRes.first->second;
}

std::unordered_map<std::string, FunctionDefinition> const& Model::getGlobalFunctionDefinitions() const {
    return globalFunctions;
}

std::unordered_map<std::string, FunctionDefinition>& Model::getGlobalFunctionDefinitions() {
    return globalFunctions;
}

storm::expressions::ExpressionManager& Model::getExpressionManager() const {
    return *expressionManager;
}

bool Model::hasNonTrivialRewardExpression() const {
    return !nonTrivialRewardModels.empty();
}

bool Model::isNonTrivialRewardModelExpression(std::string const& identifier) const {
    return nonTrivialRewardModels.count(identifier) > 0;
}

bool Model::addNonTrivialRewardExpression(std::string const& identifier, storm::expressions::Expression const& rewardExpression) {
    if (isNonTrivialRewardModelExpression(identifier)) {
        return false;
    } else {
        STORM_LOG_THROW(!globalVariables.hasVariable(identifier) || !globalVariables.getVariable(identifier).isTransient(),
                        storm::exceptions::InvalidArgumentException,
                        "Non trivial reward expression with identifier '" << identifier << "' clashes with global transient variable of the same name.");
        nonTrivialRewardModels.emplace(identifier, rewardExpression);
        return true;
    }
}

storm::expressions::Expression Model::getRewardModelExpression(std::string const& identifier) const {
    auto findRes = nonTrivialRewardModels.find(identifier);
    if (findRes != nonTrivialRewardModels.end()) {
        return findRes->second;
    } else {
        // Check whether the reward model refers to a global variable
        if (globalVariables.hasVariable(identifier)) {
            return globalVariables.getVariable(identifier).getExpressionVariable().getExpression();
        } else {
            STORM_LOG_THROW(identifier.empty(), storm::exceptions::InvalidArgumentException, "Cannot find unknown reward model '" << identifier << "'.");
            STORM_LOG_THROW(nonTrivialRewardModels.size() + globalVariables.getNumberOfNumericalTransientVariables() == 1,
                            storm::exceptions::InvalidArgumentException, "Reference to standard reward model is ambiguous.");
            if (nonTrivialRewardModels.size() == 1) {
                return nonTrivialRewardModels.begin()->second;
            } else {
                for (auto const& variable : globalVariables.getTransientVariables()) {
                    auto const& type = variable.getType();
                    if ((type.isBasicType() && type.asBasicType().isNumericalType()) || (type.isBoundedType() && type.asBoundedType().isNumericalType())) {
                        return variable.getExpressionVariable().getExpression();
                    }
                }
            }
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Cannot find unknown reward model '" << identifier << "'.");
    return storm::expressions::Expression();
}

std::vector<std::pair<std::string, storm::expressions::Expression>> Model::getAllRewardModelExpressions() const {
    std::vector<std::pair<std::string, storm::expressions::Expression>> result;
    for (auto const& nonTrivExpr : nonTrivialRewardModels) {
        result.emplace_back(nonTrivExpr.first, nonTrivExpr.second);
    }
    for (auto const& variable : globalVariables.getTransientVariables()) {
        auto const& type = variable.getType();
        if ((type.isBasicType() && type.asBasicType().isNumericalType()) || (type.isBoundedType() && type.asBoundedType().isNumericalType())) {
            result.emplace_back(variable.getName(), variable.getExpressionVariable().getExpression());
        }
    }
    return result;
}

std::unordered_map<std::string, storm::expressions::Expression> const& Model::getNonTrivialRewardExpressions() const {
    return nonTrivialRewardModels;
}

std::unordered_map<std::string, storm::expressions::Expression>& Model::getNonTrivialRewardExpressions() {
    return nonTrivialRewardModels;
}

uint64_t Model::addAutomaton(Automaton const& automaton) {
    auto it = automatonToIndex.find(automaton.getName());
    STORM_LOG_THROW(it == automatonToIndex.end(), storm::exceptions::WrongFormatException,
                    "Automaton with name '" << automaton.getName() << "' already exists.");
    automatonToIndex.emplace(automaton.getName(), automata.size());
    automata.push_back(automaton);
    return automata.size() - 1;
}

std::vector<Automaton>& Model::getAutomata() {
    return automata;
}

std::vector<Automaton> const& Model::getAutomata() const {
    return automata;
}

bool Model::hasAutomaton(std::string const& name) const {
    return automatonToIndex.find(name) != automatonToIndex.end();
}

void Model::replaceAutomaton(uint64_t index, Automaton const& automaton) {
    automata[index] = automaton;
}

Automaton& Model::getAutomaton(std::string const& name) {
    auto it = automatonToIndex.find(name);
    STORM_LOG_THROW(it != automatonToIndex.end(), storm::exceptions::InvalidOperationException, "Unable to retrieve unknown automaton '" << name << "'.");
    return automata[it->second];
}

Automaton& Model::getAutomaton(uint64_t index) {
    return automata[index];
}

Automaton const& Model::getAutomaton(uint64_t index) const {
    return automata[index];
}

Automaton const& Model::getAutomaton(std::string const& name) const {
    auto it = automatonToIndex.find(name);
    STORM_LOG_THROW(it != automatonToIndex.end(), storm::exceptions::InvalidOperationException, "Unable to retrieve unknown automaton '" << name << "'.");
    return automata[it->second];
}

uint64_t Model::getAutomatonIndex(std::string const& name) const {
    auto it = automatonToIndex.find(name);
    STORM_LOG_THROW(it != automatonToIndex.end(), storm::exceptions::InvalidOperationException, "Unable to retrieve unknown automaton '" << name << "'.");
    return it->second;
}

std::size_t Model::getNumberOfAutomata() const {
    return automata.size();
}

std::shared_ptr<Composition> Model::getStandardSystemComposition() const {
    // Determine the action indices used by each of the automata and create the standard subcompositions.
    std::set<uint64_t> allActionIndices;
    std::vector<std::set<uint64_t>> automatonActionIndices;
    std::vector<std::shared_ptr<Composition>> subcompositions;
    for (auto const& automaton : automata) {
        automatonActionIndices.push_back(automaton.getActionIndices());
        automatonActionIndices.back().erase(SILENT_ACTION_INDEX);
        allActionIndices.insert(automatonActionIndices.back().begin(), automatonActionIndices.back().end());
        subcompositions.push_back(std::make_shared<AutomatonComposition>(automaton.getName()));
    }

    // Create the standard synchronization vectors: every automaton with that action participates in the
    // synchronization.
    std::vector<storm::jani::SynchronizationVector> synchVectors;
    for (auto actionIndex : allActionIndices) {
        std::string const& actionName = this->getAction(actionIndex).getName();
        std::vector<std::string> synchVectorInputs;
        for (auto const& actionIndices : automatonActionIndices) {
            if (actionIndices.find(actionIndex) != actionIndices.end()) {
                synchVectorInputs.push_back(actionName);
            } else {
                synchVectorInputs.push_back(storm::jani::SynchronizationVector::NO_ACTION_INPUT);
            }
        }
        synchVectors.push_back(storm::jani::SynchronizationVector(synchVectorInputs, actionName));
    }

    return std::make_shared<ParallelComposition>(subcompositions, synchVectors);
}

Composition const& Model::getSystemComposition() const {
    return *composition;
}

class CompositionSimplificationVisitor : public CompositionVisitor {
   public:
    CompositionSimplificationVisitor(std::unordered_map<std::string, std::vector<std::string>> const& automatonToCopiesMap)
        : automatonToCopiesMap(automatonToCopiesMap) {}

    std::shared_ptr<Composition> simplify(Composition const& oldComposition) {
        return boost::any_cast<std::shared_ptr<Composition>>(oldComposition.accept(*this, boost::any()));
    }

    virtual boost::any visit(AutomatonComposition const& composition, boost::any const&) override {
        std::string name = composition.getAutomatonName();
        if (automatonToCopiesMap.count(name) != 0) {
            auto& copies = automatonToCopiesMap[name];
            STORM_LOG_ASSERT(!copies.empty(), "Not enough copies of automaton " << name << ".");
            name = copies.back();
            copies.pop_back();
        }
        return std::shared_ptr<Composition>(new AutomatonComposition(name, composition.getInputEnabledActions()));
    }

    virtual boost::any visit(ParallelComposition const& composition, boost::any const& data) override {
        std::vector<std::shared_ptr<Composition>> subcomposition;
        for (auto const& p : composition.getSubcompositions()) {
            subcomposition.push_back(boost::any_cast<std::shared_ptr<Composition>>(p->accept(*this, data)));
        }
        return std::shared_ptr<Composition>(new ParallelComposition(subcomposition, composition.getSynchronizationVectors()));
    }

   private:
    std::unordered_map<std::string, std::vector<std::string>> automatonToCopiesMap;
};

void Model::simplifyComposition() {
    CompositionInformationVisitor visitor(*this, this->getSystemComposition());
    CompositionInformation info = visitor.getInformation();
    if (info.containsNestedParallelComposition()) {
        STORM_LOG_WARN("Unable to simplify non-standard compliant system composition.");
    }

    // Check whether we need to copy certain automata
    std::unordered_map<std::string, std::vector<std::string>> automatonToCopiesMap;
    for (auto const& automatonMultiplicity : info.getAutomatonToMultiplicityMap()) {
        if (automatonMultiplicity.second > 1) {
            std::vector<std::string> copies = {automatonMultiplicity.first};
            // We need to copy this automaton n-1 times.
            for (uint64_t copyIndex = 1; copyIndex < automatonMultiplicity.second; ++copyIndex) {
                std::string copyPrefix = "Copy__" + std::to_string(copyIndex) + "_Of";
                std::string copyAutName = copyPrefix + automatonMultiplicity.first;
                this->addAutomaton(this->getAutomaton(automatonMultiplicity.first).clone(getManager(), copyAutName, copyPrefix));
                copies.push_back(copyAutName);
            }
            // For esthetic reasons we reverse the list of copies so that the ones with the lowest index will be pop_back'ed first
            std::reverse(copies.begin(), copies.end());
            // We insert the copies in reversed order as they will be popped in reversed order, as well.
            automatonToCopiesMap[automatonMultiplicity.first] = std::move(copies);
        }
    }

    if (!automatonToCopiesMap.empty()) {
        // Traverse the system composition and exchange automata by their copy
        auto newComposition = CompositionSimplificationVisitor(automatonToCopiesMap).simplify(getSystemComposition());
        this->setSystemComposition(newComposition);
    }
}

void Model::setSystemComposition(std::shared_ptr<Composition> const& composition) {
    this->composition = composition;
}

void Model::setStandardSystemComposition() {
    setSystemComposition(getStandardSystemComposition());
}

std::set<std::string> Model::getActionNames(bool includeSilent) const {
    std::set<std::string> result;
    for (auto const& entry : actionToIndex) {
        if (includeSilent || entry.second != SILENT_ACTION_INDEX) {
            result.insert(entry.first);
        }
    }
    return result;
}

std::map<uint64_t, std::string> Model::getActionIndexToNameMap() const {
    std::map<uint64_t, std::string> mapping;
    uint64_t i = 0;
    for (auto const& act : actions) {
        mapping[i] = act.getName();
        ++i;
    }
    return mapping;
}

Model Model::defineUndefinedConstants(std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantDefinitions) const {
    Model result(*this);

    std::set<storm::expressions::Variable> definedUndefinedConstants;
    for (auto& constant : result.constants) {
        // If the constant is already defined, we need to replace the appearances of undefined constants in its
        // defining expression
        if (constant.isDefined()) {
            // Make sure we are not trying to define an already defined constant.
            STORM_LOG_THROW(constantDefinitions.find(constant.getExpressionVariable()) == constantDefinitions.end(),
                            storm::exceptions::InvalidOperationException, "Illegally defining already defined constant '" << constant.getName() << "'.");
        } else {
            auto const& variableExpressionPair = constantDefinitions.find(constant.getExpressionVariable());

            if (variableExpressionPair != constantDefinitions.end()) {
                // If we need to define it, we add it to the defined constants and assign it the appropriate expression.
                definedUndefinedConstants.insert(constant.getExpressionVariable());

                // Make sure the type of the constant is correct.
                STORM_LOG_THROW(variableExpressionPair->second.getType() == constant.getType(), storm::exceptions::InvalidOperationException,
                                "Illegal type of expression defining constant '" << constant.getName() << "'.");

                // Now define the constant.
                if (constant.hasConstraint()) {
                    // Constraints need to be evaluated before defining a constant.
                    using SubMap = std::map<storm::expressions::Variable, storm::expressions::Expression>;
                    storm::expressions::JaniExpressionSubstitutionVisitor<SubMap> transcendentalsVisitor(SubMap(), true);
                    constant.setConstraintExpression(transcendentalsVisitor.substitute(constant.getConstraintExpression()));
                }
                constant.define(variableExpressionPair->second);
            }
        }
    }

    return result;
}

bool Model::hasUndefinedConstants() const {
    for (auto const& constant : constants) {
        if (!constant.isDefined()) {
            return true;
        }
    }
    return false;
}

std::vector<std::reference_wrapper<Constant const>> Model::getUndefinedConstants() const {
    std::vector<std::reference_wrapper<Constant const>> result;

    for (auto const& constant : constants) {
        if (!constant.isDefined()) {
            result.push_back(constant);
        }
    }

    return result;
}

Model& Model::replaceUnassignedVariablesWithConstants() {
    VariablesToConstantsTransformer().transform(*this);
    return *this;
}

Model& Model::substituteConstantsInPlace(bool const substituteTranscendentalNumbers) {
    // Gather all defining expressions of constants.
    std::map<storm::expressions::Variable, storm::expressions::Expression> constantSubstitution;
    for (auto& constant : this->getConstants()) {
        if (constant.hasConstraint()) {
            constant.setConstraintExpression(
                substituteJaniExpression(constant.getConstraintExpression(), constantSubstitution, substituteTranscendentalNumbers));
        }
        if (constant.isDefined()) {
            constant.define(substituteJaniExpression(constant.getExpression(), constantSubstitution, substituteTranscendentalNumbers));
            constantSubstitution[constant.getExpressionVariable()] = constant.getExpression();
        }
    }

    for (auto& functionDefinition : this->getGlobalFunctionDefinitions()) {
        functionDefinition.second.substitute(constantSubstitution, substituteTranscendentalNumbers);
    }

    // Substitute constants in all global variables.
    this->getGlobalVariables().substitute(constantSubstitution, substituteTranscendentalNumbers);

    // Substitute constants in initial states expression.
    this->setInitialStatesRestriction(substituteJaniExpression(this->getInitialStatesRestriction(), constantSubstitution, substituteTranscendentalNumbers));

    for (auto& rewMod : this->getNonTrivialRewardExpressions()) {
        rewMod.second = substituteJaniExpression(rewMod.second, constantSubstitution, substituteTranscendentalNumbers);
    }

    // Substitute constants in variables of automata and their edges.
    for (auto& automaton : this->getAutomata()) {
        automaton.substitute(constantSubstitution, substituteTranscendentalNumbers);
    }
    return *this;
}

Model Model::substituteConstants() const {
    Model result(*this);
    result.replaceUnassignedVariablesWithConstants();
    result.substituteConstantsInPlace(false);
    return result;
}

Model Model::substituteConstantsFunctionsTranscendentals() const {
    Model result(*this);
    result.replaceUnassignedVariablesWithConstants();
    result.substituteConstantsInPlace(true);
    result.substituteFunctions();
    return result;
}

Model Model::preprocess(std::map<storm::expressions::Variable, storm::expressions::Expression> const& constantDefinitions) const {
    // We intentionally do not eliminate function expressions in jani models at this point because that would also remove the function
    // declarations from the model. However, those might still be needed to, e.g., process properties that refer to functions.
    return this->defineUndefinedConstants(constantDefinitions).substituteConstants();
}

Model Model::preprocess(std::string const& constantDefinitionString) const {
    return this->preprocess(storm::storage::parseConstantDefinitionString(this->getManager(), constantDefinitionString));
}

std::map<storm::expressions::Variable, storm::expressions::Expression> Model::getConstantsSubstitution() const {
    std::map<storm::expressions::Variable, storm::expressions::Expression> result;

    for (auto const& constant : constants) {
        if (constant.isDefined()) {
            result.emplace(constant.getExpressionVariable(), constant.getExpression());
        }
    }

    return result;
}

void Model::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers) {
    // substitute in all defining expressions of constants
    for (auto& constant : this->getConstants()) {
        if (constant.hasConstraint()) {
            constant.setConstraintExpression(substituteJaniExpression(constant.getConstraintExpression(), substitution, substituteTranscendentalNumbers));
        }
        if (constant.isDefined()) {
            constant.define(substituteJaniExpression(constant.getExpression(), substitution, substituteTranscendentalNumbers));
        }
    }

    for (auto& functionDefinition : this->getGlobalFunctionDefinitions()) {
        functionDefinition.second.substitute(substitution, substituteTranscendentalNumbers);
    }

    // Substitute in all global variables.
    for (auto& variable : this->getGlobalVariables().getBoundedIntegerVariables()) {
        variable.substitute(substitution, substituteTranscendentalNumbers);
    }
    for (auto& variable : this->getGlobalVariables().getArrayVariables()) {
        variable.substitute(substitution, substituteTranscendentalNumbers);
    }
    for (auto& variable : this->getGlobalVariables().getClockVariables()) {
        variable.substitute(substitution, substituteTranscendentalNumbers);
    }

    // Substitute in initial states expression.
    this->setInitialStatesRestriction(substituteJaniExpression(this->getInitialStatesRestriction(), substitution, substituteTranscendentalNumbers));

    for (auto& rewMod : getNonTrivialRewardExpressions()) {
        rewMod.second = substituteJaniExpression(rewMod.second, substitution, substituteTranscendentalNumbers);
    }

    // Substitute in variables of automata and their edges.
    for (auto& automaton : this->getAutomata()) {
        automaton.substitute(substitution, substituteTranscendentalNumbers);
    }
}

void Model::substituteFunctions() {
    std::vector<Property> emptyPropertyVector;
    substituteFunctions(emptyPropertyVector);
}

void Model::substituteFunctions(std::vector<Property>& properties) {
    eliminateFunctions(*this, properties);
}

bool Model::containsArrayVariables() const {
    if (getGlobalVariables().containsArrayVariables()) {
        return true;
    }
    for (auto const& a : getAutomata()) {
        if (a.getVariables().containsArrayVariables()) {
            return true;
        }
    }
    return false;
}

ArrayEliminatorData Model::eliminateArrays(bool keepNonTrivialArrayAccess) {
    ArrayEliminator arrayEliminator;
    return arrayEliminator.eliminate(*this, keepNonTrivialArrayAccess);
}

void Model::eliminateArrays(std::vector<Property>& properties) {
    auto data = eliminateArrays(false);
    for (auto& p : properties) {
        data.transformProperty(p);
    }
}

ModelFeatures Model::restrictToFeatures(ModelFeatures const& modelFeatures) {
    std::vector<Property> emptyPropertyVector;
    return restrictToFeatures(modelFeatures, emptyPropertyVector);
}

ModelFeatures Model::restrictToFeatures(ModelFeatures const& features, std::vector<Property>& properties) {
    ModelFeatures uncheckedFeatures = getModelFeatures();
    // Check if functions need to be eliminated.
    if (uncheckedFeatures.hasFunctions() && !features.hasFunctions()) {
        substituteFunctions(properties);
    }
    uncheckedFeatures.remove(ModelFeature::Functions);

    // Check if arrays need to be eliminated. This should be done after! eliminating the functions
    if (uncheckedFeatures.hasArrays() && !features.hasArrays()) {
        eliminateArrays(properties);
    }
    uncheckedFeatures.remove(ModelFeature::Arrays);

    // There is no elimination for state exit rewards
    if (features.hasStateExitRewards()) {
        uncheckedFeatures.remove(ModelFeature::StateExitRewards);
    }

    // There is no elimination of derived operators
    if (features.hasDerivedOperators()) {
        uncheckedFeatures.remove(ModelFeature::DerivedOperators);
    }

    // There is no elimination of MultiObjective properties
    if (features.hasDerivedOperators()) {
        uncheckedFeatures.remove(ModelFeature::MultiObjectiveProperties);
    }

    // There is no elimination of trigonometric operators
    if (features.hasTrigonometricFunctions()) {
        uncheckedFeatures.remove(ModelFeature::TrigonometricFunctions);
    }

    return uncheckedFeatures;
}

void Model::setInitialStatesRestriction(storm::expressions::Expression const& initialStatesRestriction) {
    this->initialStatesRestriction = initialStatesRestriction;
}

bool Model::hasInitialStatesRestriction() const {
    return this->initialStatesRestriction.isInitialized();
}

storm::expressions::Expression const& Model::getInitialStatesRestriction() const {
    return initialStatesRestriction;
}

bool Model::hasNonTrivialInitialStates() const {
    if (this->hasInitialStatesRestriction() && !this->getInitialStatesRestriction().isTrue()) {
        return true;
    } else {
        for (auto const& variable : this->getGlobalVariables()) {
            if (variable.hasInitExpression() && !variable.isTransient()) {
                return true;
            }
        }

        for (auto const& automaton : this->automata) {
            if (automaton.hasNonTrivialInitialStates()) {
                return true;
            }
        }
    }

    return false;
}

storm::expressions::Expression Model::getInitialStatesExpression() const {
    std::vector<std::reference_wrapper<storm::jani::Automaton const>> allAutomata;
    for (auto const& automaton : this->getAutomata()) {
        allAutomata.push_back(automaton);
    }
    return getInitialStatesExpression(allAutomata);
}

bool Model::hasTrivialInitialStatesExpression() const {
    if (this->hasInitialStatesRestriction() && !this->getInitialStatesRestriction().isTrue()) {
        return false;
    }

    bool result = true;
    for (auto const& automaton : this->getAutomata()) {
        result &= automaton.hasTrivialInitialStatesExpression();
        if (!result) {
            break;
        }
    }
    return result;
}

storm::expressions::Expression Model::getInitialStatesExpression(std::vector<std::reference_wrapper<storm::jani::Automaton const>> const& automata) const {
    // Start with the restriction of variables.
    storm::expressions::Expression result = initialStatesRestriction;

    // Then add initial values for those non-transient variables that have one.
    for (auto const& variable : globalVariables) {
        if (variable.isTransient()) {
            continue;
        }

        if (variable.hasInitExpression()) {
            storm::expressions::Expression newInitExpression;
            if (variable.getType().isBasicType() && variable.getType().asBasicType().isBooleanType()) {
                newInitExpression = storm::expressions::iff(variable.getExpressionVariable(), variable.getInitExpression());
            } else {
                newInitExpression = variable.getExpressionVariable() == variable.getInitExpression();
            }
            result = result && newInitExpression;
        }
    }

    // If we are to include the expressions for the automata, do so now.
    for (auto const& automatonReference : automata) {
        storm::jani::Automaton const& automaton = automatonReference.get();
        if (!automaton.getVariables().empty()) {
            storm::expressions::Expression automatonInitialStatesExpression = automaton.getInitialStatesExpression();
            if (automatonInitialStatesExpression.isInitialized() && !automatonInitialStatesExpression.isTrue()) {
                result = result && automatonInitialStatesExpression;
            }
        }
    }
    return result;
}

bool Model::isDeterministicModel() const {
    return this->getModelType() == ModelType::DTMC || this->getModelType() == ModelType::CTMC;
}

bool Model::isDiscreteTimeModel() const {
    return this->getModelType() == ModelType::DTMC || this->getModelType() == ModelType::MDP;
}

std::vector<storm::expressions::Expression> Model::getAllRangeExpressions(
    std::vector<std::reference_wrapper<storm::jani::Automaton const>> const& automata) const {
    std::vector<storm::expressions::Expression> result;
    for (auto const& variable : this->getGlobalVariables().getBoundedIntegerVariables()) {
        result.push_back(variable.getRangeExpression());
    }
    STORM_LOG_ASSERT(this->getGlobalVariables().getArrayVariables().empty(), "This operation is unsupported if array variables are present.");

    if (automata.empty()) {
        for (auto const& automaton : this->getAutomata()) {
            std::vector<storm::expressions::Expression> automatonRangeExpressions = automaton.getAllRangeExpressions();
            result.insert(result.end(), automatonRangeExpressions.begin(), automatonRangeExpressions.end());
        }
    } else {
        for (auto const& automaton : automata) {
            std::vector<storm::expressions::Expression> automatonRangeExpressions = automaton.get().getAllRangeExpressions();
            result.insert(result.end(), automatonRangeExpressions.begin(), automatonRangeExpressions.end());
        }
    }
    return result;
}

void Model::finalize() {
    for (auto& automaton : getAutomata()) {
        automaton.finalize(*this);
    }
}

void Model::checkValid() const {
    // TODO switch to exception based return value.
    STORM_LOG_ASSERT(getModelType() != storm::jani::ModelType::UNDEFINED, "Model type not set");
    STORM_LOG_ASSERT(!automata.empty(), "No automata set");
    STORM_LOG_ASSERT(composition != nullptr, "Composition is not set");
}

storm::expressions::Expression Model::getLabelExpression(Variable const& transientVariable) const {
    std::vector<std::reference_wrapper<Automaton const>> allAutomata;
    for (auto const& automaton : automata) {
        allAutomata.emplace_back(automaton);
    }
    return getLabelExpression(transientVariable, allAutomata);
}

storm::expressions::Expression Model::getLabelExpression(Variable const& transientVariable,
                                                         std::vector<std::reference_wrapper<Automaton const>> const& automata) const {
    STORM_LOG_THROW(transientVariable.isTransient(), storm::exceptions::InvalidArgumentException, "Expected transient variable.");
    auto const& type = transientVariable.getType();
    STORM_LOG_THROW(type.isBasicType() && type.asBasicType().isBooleanType(), storm::exceptions::InvalidArgumentException, "Expected boolean variable.");

    storm::expressions::Expression result;
    bool negate = transientVariable.getInitExpression().isTrue();

    for (auto const& automaton : automata) {
        storm::expressions::Variable const& locationVariable = automaton.get().getLocationExpressionVariable();

        for (auto const& location : automaton.get().getLocations()) {
            for (auto const& assignment : location.getAssignments().getTransientAssignments()) {
                if (assignment.getExpressionVariable() == transientVariable.getExpressionVariable()) {
                    storm::expressions::Expression newExpression;
                    if (automaton.get().getNumberOfLocations() <= 1) {
                        newExpression = (negate ? !assignment.getAssignedExpression() : assignment.getAssignedExpression());
                    } else {
                        newExpression = (locationVariable == this->getManager().integer(automaton.get().getLocationIndex(location.getName()))) &&
                                        (negate ? !assignment.getAssignedExpression() : assignment.getAssignedExpression());
                    }
                    if (result.isInitialized()) {
                        result = result || newExpression;
                    } else {
                        result = newExpression;
                    }
                }
            }
        }
    }

    if (result.isInitialized()) {
        if (negate) {
            result = !result;
        }
    } else {
        result = this->getManager().boolean(negate);
    }

    return result;
}

bool Model::hasStandardComposition() const {
    CompositionInformationVisitor visitor(*this, this->getSystemComposition());
    CompositionInformation info = visitor.getInformation();
    if (info.containsNonStandardParallelComposition()) {
        return false;
    }
    for (auto const& multiplicity : info.getAutomatonToMultiplicityMap()) {
        if (multiplicity.second > 1) {
            return false;
        }
    }
    return true;
}

bool Model::hasStandardCompliantComposition() const {
    CompositionInformationVisitor visitor(*this, this->getSystemComposition());
    CompositionInformation info = visitor.getInformation();
    if (info.containsNestedParallelComposition()) {
        return false;
    }
    return true;
}

bool Model::undefinedConstantsAreGraphPreserving() const {
    if (!this->hasUndefinedConstants()) {
        return true;
    }

    // Gather the variables of all undefined constants.
    std::set<storm::expressions::Variable> undefinedConstantVariables;
    for (auto const& constant : this->getConstants()) {
        if (!constant.isDefined()) {
            undefinedConstantVariables.insert(constant.getExpressionVariable());
        }
    }

    // Start by checking the defining expressions of all defined constants. If it contains a currently undefined
    // constant, we need to mark the target constant as undefined as well.
    for (auto const& constant : this->getConstants()) {
        if (constant.isDefined()) {
            if (constant.getExpression().containsVariable(undefinedConstantVariables)) {
                undefinedConstantVariables.insert(constant.getExpressionVariable());
            }
        }
    }

    // Check global variable definitions.
    if (this->getGlobalVariables().containsVariablesInBoundExpressionsOrInitialValues(undefinedConstantVariables)) {
        return false;
    }

    // Check the automata.
    for (auto const& automaton : this->getAutomata()) {
        if (!automaton.containsVariablesOnlyInProbabilitiesOrTransientAssignments(undefinedConstantVariables)) {
            return false;
        }
    }

    // Check initial states restriction.
    if (initialStatesRestriction.containsVariable(undefinedConstantVariables)) {
        return false;
    }
    return true;
}

void Model::makeStandardJaniCompliant() {
    for (auto& automaton : automata) {
        // For discrete-time models, we push the assignments to real-valued transient variables (rewards) to the
        // edges.
        if (this->isDiscreteTimeModel()) {
            automaton.pushTransientRealLocationAssignmentsToEdges();
        }
        automaton.pushEdgeAssignmentsToDestinations();
    }
}

void Model::pushEdgeAssignmentsToDestinations() {
    for (auto& automaton : automata) {
        automaton.pushEdgeAssignmentsToDestinations();
    }
}

void Model::liftTransientEdgeDestinationAssignments(int64_t maxLevel) {
    for (auto& automaton : this->getAutomata()) {
        automaton.liftTransientEdgeDestinationAssignments(maxLevel);
    }
}

bool Model::hasTransientEdgeDestinationAssignments() const {
    for (auto const& automaton : this->getAutomata()) {
        if (automaton.hasTransientEdgeDestinationAssignments()) {
            return true;
        }
    }
    return false;
}

bool Model::usesAssignmentLevels(bool onlyTransient) const {
    for (auto const& automaton : this->getAutomata()) {
        if (automaton.usesAssignmentLevels(onlyTransient)) {
            return true;
        }
    }
    return false;
}

bool Model::isLinear() const {
    bool result = true;

    storm::expressions::LinearityCheckVisitor linearityChecker;
    result &= linearityChecker.check(this->getInitialStatesExpression(), true);

    for (auto const& automaton : this->getAutomata()) {
        result &= automaton.isLinear();
    }

    return result;
}

bool Model::reusesActionsInComposition() const {
    if (composition->isParallelComposition()) {
        return composition->asParallelComposition().areActionsReused();
    }
    return false;
}

uint64_t Model::encodeAutomatonAndEdgeIndices(uint64_t automatonIndex, uint64_t edgeIndex) {
    return automatonIndex << 32 | edgeIndex;
}

std::pair<uint64_t, uint64_t> Model::decodeAutomatonAndEdgeIndices(uint64_t index) {
    return std::make_pair(index >> 32, index & ((1ull << 32) - 1));
}

Model Model::restrictEdges(storm::storage::FlatSet<uint_fast64_t> const& automataAndEdgeIndices) const {
    Model result(*this);

    // Restrict all automata.
    for (uint64_t automatonIndex = 0; automatonIndex < result.automata.size(); ++automatonIndex) {
        // Compute the set of edges that is to be kept for this automaton.
        storm::storage::FlatSet<uint_fast64_t> automatonEdgeIndices;
        for (auto const& e : automataAndEdgeIndices) {
            auto automatonAndEdgeIndex = decodeAutomatonAndEdgeIndices(e);
            if (automatonAndEdgeIndex.first == automatonIndex) {
                automatonEdgeIndices.insert(automatonAndEdgeIndex.second);
            }
        }

        result.automata[automatonIndex].restrictToEdges(automatonEdgeIndices);
    }

    return result;
}

Model Model::createModelFromAutomaton(Automaton const& automaton) const {
    // Copy the full model
    Model newModel(*this);

    // Replace the automata by the one single selected automaton.
    newModel.automata = std::vector<Automaton>({automaton});

    // Set the standard composition for the new model to the default one.
    newModel.setSystemComposition(newModel.getStandardSystemComposition());

    return newModel;
}

// Helper for writeDotToStream:

std::string filterName(std::string const& text) {
    std::string result = text;
    std::replace_if(result.begin(), result.end(), [](const char& c) { return std::ispunct(c); }, '_');
    return result;
}

void Model::writeDotToStream(std::ostream& outStream) const {
    outStream << "digraph " << filterName(name) << " {\n";

    std::vector<std::string> actionNames;
    for (auto const& act : actions) {
        actionNames.push_back(act.getName());
    }

    for (auto const& automaton : automata) {
        automaton.writeDotToStream(outStream, actionNames);
        outStream << '\n';
    }

    outStream << "}";
}

std::ostream& operator<<(std::ostream& out, Model const& model) {
    JsonExporter::toStream(model, std::vector<storm::jani::Property>(), out);
    return out;
}
}  // namespace jani
}  // namespace storm
