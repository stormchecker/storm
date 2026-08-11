#include "storm/storage/prism/ToJaniConverter.h"

#include "storm/storage/expressions/ExpressionManager.h"

#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/TemplateEdge.h"
#include "storm/storage/jani/expressions/FunctionCallExpression.h"
#include "storm/storage/jani/types/AllJaniTypes.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/storage/prism/CompositionToJaniVisitor.h"
#include "storm/storage/prism/Program.h"

#include "storm/exceptions/NotImplementedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace prism {

storm::jani::Model ToJaniConverter::convert(storm::prism::Program const& program, bool allVariablesGlobal,
                                            std::set<storm::expressions::Variable> const& givenVariablesToMakeGlobal, std::string suffix) {
    labelRenaming.clear();
    rewardModelRenaming.clear();
    formulaToFunctionCallMap.clear();

    std::shared_ptr<storm::expressions::ExpressionManager> manager = program.getManager().getSharedPointer();

    // Start by creating an empty JANI model.
    storm::jani::ModelType modelType;
    switch (program.getModelType()) {
        case Program::ModelType::DTMC:
            modelType = storm::jani::ModelType::DTMC;
            break;
        case Program::ModelType::CTMC:
            modelType = storm::jani::ModelType::CTMC;
            break;
        case Program::ModelType::MDP:
            modelType = storm::jani::ModelType::MDP;
            break;
        case Program::ModelType::CTMDP:
            modelType = storm::jani::ModelType::CTMDP;
            break;
        case Program::ModelType::MA:
            modelType = storm::jani::ModelType::MA;
            break;
        case Program::ModelType::PTA:
            modelType = storm::jani::ModelType::PTA;
            break;
        default:
            modelType = storm::jani::ModelType::UNDEFINED;
    }
    storm::jani::Model janiModel("jani_from_prism", modelType, 1, manager);

    janiModel.getModelFeatures().add(storm::jani::ModelFeature::DerivedOperators);

    // Add all constants of the PRISM program to the JANI model.
    for (auto const& constant : program.getConstants()) {
        janiModel.addConstant(storm::jani::Constant(constant.getName(), constant.getExpressionVariable(),
                                                    constant.isDefined() ? constant.getExpression() : storm::expressions::Expression()));
    }

    // Maintain a mapping of each variable to a flag that is true if the variable will be made global.
    std::map<storm::expressions::Variable, bool> variablesToMakeGlobal;
    for (auto const& var : givenVariablesToMakeGlobal) {
        variablesToMakeGlobal.emplace(var, true);
    }

    // Get the set of variables that appeare in a renaimng of a renamed module
    if (program.getNumberOfFormulas() > 0) {
        std::set<storm::expressions::Variable> renamedVariables;
        for (auto const& module : program.getModules()) {
            if (module.isRenamedFromModule()) {
                for (auto const& renaimingPair : module.getRenaming()) {
                    if (manager->hasVariable(renaimingPair.first)) {
                        renamedVariables.insert(manager->getVariable(renaimingPair.first));
                    }
                    if (manager->hasVariable(renaimingPair.second)) {
                        renamedVariables.insert(manager->getVariable(renaimingPair.second));
                    }
                }
            }
        }

        // Add all formulas of the PRISM program to the JANI model.
        // Also collect a substitution of formula placeholder variables to function call expressions.
        for (auto const& formula : program.getFormulas()) {
            // First find 1. all variables that occurr in the formula definition (including the ones used in called formulae) and 2. the called formulae
            // Variables that occurr in a renaming need to become a parameter of the function.
            // Others need to be made global.
            std::set<storm::expressions::Variable> variablesInFormula, placeholdersInFormula;
            for (auto const& var : formula.getExpression().getVariables()) {
                // Check whether var is an actual variable/constant or another formula
                auto functionCallIt = formulaToFunctionCallMap.find(var);
                if (functionCallIt == formulaToFunctionCallMap.end()) {
                    if (renamedVariables.count(var) > 0) {
                        variablesInFormula.insert(var);
                    } else {
                        variablesToMakeGlobal.emplace(var, true);
                    }
                } else {
                    storm::expressions::FunctionCallExpression const& innerFunctionCall =
                        dynamic_cast<storm::expressions::FunctionCallExpression const&>(functionCallIt->second.getBaseExpression());
                    for (auto const& innerFunctionArg : innerFunctionCall.getArguments()) {
                        auto const& argVar = innerFunctionArg->asVariableExpression().getVariable();
                        if (renamedVariables.count(argVar) > 0) {
                            variablesInFormula.insert(argVar);
                        } else {
                            variablesToMakeGlobal.emplace(argVar, true);
                        }
                    }
                    placeholdersInFormula.insert(var);
                }
            }

            // Add a function argument and parameter for each occurring variable and prepare the substitution for the function body
            std::map<storm::expressions::Variable, storm::expressions::Expression> functionBodySubstitution;
            std::vector<storm::expressions::Variable> functionParameters;
            std::vector<std::shared_ptr<storm::expressions::BaseExpression const>> functionArguments;
            for (auto const& var : variablesInFormula) {
                functionArguments.push_back(var.getExpression().getBaseExpressionPointer());
                functionParameters.push_back(manager->declareVariable(formula.getName() + "__param__" + var.getName() + suffix, var.getType()));
                functionBodySubstitution[var] = functionParameters.back().getExpression();
            }
            for (auto const& formulaVar : placeholdersInFormula) {
                functionBodySubstitution[formulaVar] =
                    storm::jani::substituteJaniExpression(formulaToFunctionCallMap[formulaVar], functionBodySubstitution, false);
            }

            storm::jani::FunctionDefinition funDef(formula.getName(), formula.getType(), functionParameters,
                                                   storm::jani::substituteJaniExpression(formula.getExpression(), functionBodySubstitution, false));
            janiModel.addFunctionDefinition(funDef);
            auto functionCallExpression =
                std::make_shared<storm::expressions::FunctionCallExpression>(*manager, formula.getType(), formula.getName(), functionArguments);
            formulaToFunctionCallMap[formula.getExpressionVariable()] = functionCallExpression->toExpression();
        }
    }

    // Maintain a mapping from expression variables to JANI variables so we can fill in the correct objects when
    // creating assignments.
    std::map<storm::expressions::Variable, std::reference_wrapper<storm::jani::Variable const>> variableToVariableMap;

    // Add all global variables of the PRISM program to the JANI model.
    for (auto const& variable : program.getGlobalIntegerVariables()) {
        if (variable.hasLowerBoundExpression() || variable.hasUpperBoundExpression()) {
            storm::jani::Variable const& createdVariable = janiModel.addVariable(
                *storm::jani::Variable::makeBoundedIntegerVariable(variable.getName(), variable.getExpressionVariable(), variable.getInitialValueExpression(),
                                                                   false, variable.getLowerBoundExpression(), variable.getUpperBoundExpression()));
            variableToVariableMap.emplace(variable.getExpressionVariable(), createdVariable);
        } else {
            storm::jani::Variable const& createdVariable = janiModel.addVariable(
                *storm::jani::Variable::makeIntegerVariable(variable.getName(), variable.getExpressionVariable(), variable.getInitialValueExpression(), false));
            variableToVariableMap.emplace(variable.getExpressionVariable(), createdVariable);
        }
    }
    for (auto const& variable : program.getGlobalBooleanVariables()) {
        storm::jani::Variable const& createdVariable = janiModel.addVariable(
            *storm::jani::Variable::makeBooleanVariable(variable.getName(), variable.getExpressionVariable(), variable.getInitialValueExpression(), false));
        variableToVariableMap.emplace(variable.getExpressionVariable(), createdVariable);
    }

    // Add all actions of the PRISM program to the JANI model.
    for (auto const& action : program.getActions()) {
        // Ignore the empty action as every JANI program has predefined tau action.
        if (!action.empty()) {
            janiModel.addAction(storm::jani::Action(action));
        }
    }

    // Because of the rules of JANI, we have to make all variables of modules global that are read by other modules.
    // Create a mapping from variables to the indices of module indices that write/read the variable.
    std::map<storm::expressions::Variable, std::set<uint_fast64_t>> variablesToAccessingModuleIndices;
    for (uint_fast64_t index = 0; index < program.getNumberOfModules(); ++index) {
        storm::prism::Module const& module = program.getModule(index);

        // Gather all variables occurring in this module
        std::set<storm::expressions::Variable> variables = module.getAllExpressionVariables();
        for (auto const& command : module.getCommands()) {
            command.getGuardExpression().getBaseExpression().gatherVariables(variables);
            for (auto const& update : command.getUpdates()) {
                update.getLikelihoodExpression().gatherVariables(variables);
                for (auto const& assignment : update.getAssignments()) {
                    assignment.getExpression().getBaseExpression().gatherVariables(variables);
                    variables.insert(assignment.getVariable());
                }
            }
        }

        // insert the accessing module index for each accessed variable
        std::map<storm::expressions::Variable, storm::expressions::Expression> renamedFormulaToFunctionCallMap;
        if (module.isRenamedFromModule()) {
            renamedFormulaToFunctionCallMap = program.getSubstitutionForRenamedModule(module, formulaToFunctionCallMap);
        }

        for (auto const& variable : variables) {
            // Check whether the variable actually is a formula
            if (formulaToFunctionCallMap.count(variable) > 0) {
                std::set<storm::expressions::Variable> variablesInFunctionCall;
                if (module.isRenamedFromModule()) {
                    variablesInFunctionCall = renamedFormulaToFunctionCallMap[variable].getVariables();
                } else {
                    variablesInFunctionCall = formulaToFunctionCallMap[variable].getVariables();
                }
                for (auto const& funVar : variablesInFunctionCall) {
                    variablesToAccessingModuleIndices[funVar].insert(index);
                }
            } else {
                variablesToAccessingModuleIndices[variable].insert(index);
            }
        }
    }

    // Create a mapping from variables to a flag indicating whether it should be made global
    for (auto const& varMods : variablesToAccessingModuleIndices) {
        STORM_LOG_ASSERT(!varMods.second.empty(), "Variable has no accessing modules.");
        auto varIt = variablesToMakeGlobal.find(varMods.first);
        // If there is exactly one module reading and writing the variable, we can make the variable local to this module.
        if (varIt == variablesToMakeGlobal.end()) {
            variablesToMakeGlobal.emplace(varMods.first, allVariablesGlobal || (varMods.second.size() > 1));
        } else {
            varIt->second = varIt->second || allVariablesGlobal || (varMods.second.size() > 1);
        }
    }

    // Go through the labels and construct assignments to transient variables that are added to the locations.
    std::vector<storm::jani::Assignment> transientLocationAssignments;
    for (auto const& label : program.getLabels()) {
        bool renameLabel = manager->hasVariable(label.getName()) || program.hasRewardModel(label.getName());
        std::string finalLabelName = renameLabel ? "label_" + label.getName() + suffix : label.getName();
        if (renameLabel) {
            STORM_LOG_INFO("Label '" << label.getName() << "' was renamed to '" << finalLabelName
                                     << "' in PRISM-to-JANI conversion, as another variable with that name already exists.");
            labelRenaming[label.getName()] = finalLabelName;
        }
        auto newExpressionVariable = manager->declareBooleanVariable(finalLabelName);
        storm::jani::Variable const& newTransientVariable = janiModel.addVariable(
            *storm::jani::Variable::makeBooleanVariable(newExpressionVariable.getName(), newExpressionVariable, manager->boolean(false), true));
        transientLocationAssignments.emplace_back(storm::jani::LValue(newTransientVariable), label.getStatePredicateExpression());

        // Variables that are accessed in the label predicate expression should be made global.
        std::set<storm::expressions::Variable> variables = label.getStatePredicateExpression().getVariables();
        for (auto const& variable : variables) {
            if (formulaToFunctionCallMap.count(variable) > 0) {
                for (auto const& funVar : formulaToFunctionCallMap[variable].getVariables()) {
                    variablesToMakeGlobal[funVar] = true;
                }
            } else {
                variablesToMakeGlobal[variable] = true;
            }
        }
    }

    // Create an initial state restriction if there was an initial construct in the program.
    if (program.hasInitialConstruct()) {
        janiModel.setInitialStatesRestriction(program.getInitialStatesExpression());
        // Variables in the initial state expression should be made global
        std::set<storm::expressions::Variable> variables = program.getInitialStatesExpression().getVariables();
        for (auto const& variable : variables) {
            if (formulaToFunctionCallMap.count(variable) > 0) {
                for (auto const& funVar : formulaToFunctionCallMap[variable].getVariables()) {
                    variablesToMakeGlobal[funVar] = true;
                }
            } else {
                variablesToMakeGlobal[variable] = true;
            }
        }
    } else {
        janiModel.setInitialStatesRestriction(manager->boolean(true));
    }

    // Go through the reward models and construct assignments to the transient variables that are to be added to
    // edges and transient assignments that are added to the locations.
    std::map<uint_fast64_t, std::vector<storm::jani::Assignment>> transientEdgeAssignments;
    bool hasStateRewards = false;
    for (auto const& rewardModel : program.getRewardModels()) {
        std::string finalRewardModelName;
        if (rewardModel.getName().empty()) {
            finalRewardModelName = "default_reward_model";
        } else {
            if (manager->hasVariable(rewardModel.getName())) {
                // Rename
                finalRewardModelName = "rewardmodel_" + rewardModel.getName() + suffix;
                STORM_LOG_INFO("Rewardmodel '" << rewardModel.getName() << "' was renamed to '" << finalRewardModelName
                                               << "' in PRISM-to-JANI conversion, as another variable with that name already exists.");
                rewardModelRenaming[rewardModel.getName()] = finalRewardModelName;
            } else {
                finalRewardModelName = rewardModel.getName();
            }
        }

        auto newExpressionVariable = manager->declareRationalVariable(finalRewardModelName);
        storm::jani::Variable const& newTransientVariable = janiModel.addVariable(
            *storm::jani::Variable::makeRealVariable(newExpressionVariable.getName(), newExpressionVariable, manager->rational(0.0), true));

        if (rewardModel.hasStateRewards()) {
            hasStateRewards = true;
            storm::expressions::Expression transientLocationExpression;
            for (auto const& stateReward : rewardModel.getStateRewards()) {
                storm::expressions::Expression rewardTerm =
                    stateReward.getStatePredicateExpression().isTrue()
                        ? stateReward.getRewardValueExpression()
                        : storm::expressions::ite(stateReward.getStatePredicateExpression(), stateReward.getRewardValueExpression(), manager->rational(0));
                if (transientLocationExpression.isInitialized()) {
                    transientLocationExpression = transientLocationExpression + rewardTerm;
                } else {
                    transientLocationExpression = rewardTerm;
                }
            }
            transientLocationAssignments.emplace_back(storm::jani::LValue(newTransientVariable), transientLocationExpression);
            // Variables that are accessed in a reward term should be made global.
            std::set<storm::expressions::Variable> variables = transientLocationExpression.getVariables();
            for (auto const& variable : variables) {
                if (formulaToFunctionCallMap.count(variable) > 0) {
                    for (auto const& funVar : formulaToFunctionCallMap[variable].getVariables()) {
                        variablesToMakeGlobal[funVar] = true;
                    }
                } else {
                    variablesToMakeGlobal[variable] = true;
                }
            }
        }

        std::map<uint_fast64_t, storm::expressions::Expression> actionIndexToExpression;
        for (auto const& actionReward : rewardModel.getStateActionRewards()) {
            storm::expressions::Expression rewardTerm =
                actionReward.getStatePredicateExpression().isTrue()
                    ? actionReward.getRewardValueExpression()
                    : storm::expressions::ite(actionReward.getStatePredicateExpression(), actionReward.getRewardValueExpression(), manager->rational(0));
            auto it = actionIndexToExpression.find(janiModel.getActionIndex(actionReward.getActionName()));
            if (it != actionIndexToExpression.end()) {
                it->second = it->second + rewardTerm;
            } else {
                actionIndexToExpression[janiModel.getActionIndex(actionReward.getActionName())] = rewardTerm;
            }
        }

        for (auto const& entry : actionIndexToExpression) {
            auto it = transientEdgeAssignments.find(entry.first);
            if (it != transientEdgeAssignments.end()) {
                it->second.push_back(storm::jani::Assignment(storm::jani::LValue(newTransientVariable), entry.second));
            } else {
                std::vector<storm::jani::Assignment> assignments = {storm::jani::Assignment(storm::jani::LValue(newTransientVariable), entry.second)};
                transientEdgeAssignments.emplace(entry.first, assignments);
            }
            // Variables that are accessed in a reward term should be made global.
            std::set<storm::expressions::Variable> variables = entry.second.getVariables();
            for (auto const& variable : variables) {
                variablesToMakeGlobal[variable] = true;
            }
        }
        STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotImplementedException,
                        "Transition reward translation currently not implemented.");
    }
    STORM_LOG_THROW(transientEdgeAssignments.empty() || transientLocationAssignments.empty() || !program.specifiesSystemComposition(),
                    storm::exceptions::NotImplementedException, "Cannot translate reward models from PRISM to JANI that specify a custom system composition.");
    // if there are state rewards and the model is a discrete time model, we add the corresponding model feature
    if (janiModel.isDiscreteTimeModel() && hasStateRewards) {
        janiModel.getModelFeatures().add(storm::jani::ModelFeature::StateExitRewards);
    }

    // Now create the separate JANI automata from the modules of the PRISM program. While doing so, we use the
    // previously built mapping to make variables global that are read by more than one module.
    std::set<uint64_t> firstModules;
    bool firstModule = true;
    for (auto const& module : program.getModules()) {
        // Keep track of the action indices contained in this module.
        std::set<uint_fast64_t> actionIndicesOfModule;

        storm::jani::Automaton automaton(module.getName(), manager->declareIntegerVariable("_loc_prism2jani_" + module.getName() + "_" + suffix));
        for (auto const& variable : module.getIntegerVariables()) {
            auto findRes = variablesToMakeGlobal.find(variable.getExpressionVariable());
            if (findRes != variablesToMakeGlobal.end()) {
                bool makeVarGlobal = findRes->second;
                if (variable.hasLowerBoundExpression() || variable.hasUpperBoundExpression()) {
                    auto newIntegerVariable = storm::jani::Variable::makeBoundedIntegerVariable(
                        variable.getName(), variable.getExpressionVariable(), variable.getInitialValueExpression(), false, variable.getLowerBoundExpression(),
                        variable.getUpperBoundExpression());
                    storm::jani::Variable const& createdVariable =
                        makeVarGlobal ? janiModel.addVariable(*newIntegerVariable) : automaton.addVariable(*newIntegerVariable);
                    variableToVariableMap.emplace(variable.getExpressionVariable(), createdVariable);
                } else {
                    auto newIntegerVariable = storm::jani::Variable::makeIntegerVariable(variable.getName(), variable.getExpressionVariable(),
                                                                                         variable.getInitialValueExpression(), false);
                    storm::jani::Variable const& createdVariable =
                        makeVarGlobal ? janiModel.addVariable(*newIntegerVariable) : automaton.addVariable(*newIntegerVariable);
                    variableToVariableMap.emplace(variable.getExpressionVariable(), createdVariable);
                }
            } else {
                STORM_LOG_INFO("Variable " << variable.getName() << " is declared but never used.");
            }
        }
        for (auto const& variable : module.getBooleanVariables()) {
            auto newBooleanVariable = storm::jani::Variable::makeBooleanVariable(
                variable.getName(), variable.getExpressionVariable(),
                variable.hasInitialValue() ? boost::make_optional(variable.getInitialValueExpression()) : boost::none, false);
            auto findRes = variablesToMakeGlobal.find(variable.getExpressionVariable());
            if (findRes != variablesToMakeGlobal.end()) {
                bool makeVarGlobal = findRes->second;
                storm::jani::Variable const& createdVariable =
                    makeVarGlobal ? janiModel.addVariable(*newBooleanVariable) : automaton.addVariable(*newBooleanVariable);
                variableToVariableMap.emplace(variable.getExpressionVariable(), createdVariable);
            } else {
                STORM_LOG_INFO("Variable " << variable.getName() << " is declared but never used.");
            }
        }
        for (auto const& variable : module.getClockVariables()) {
            auto newClockVariable = storm::jani::Variable::makeClockVariable(
                variable.getName(), variable.getExpressionVariable(),
                variable.hasInitialValue() ? boost::make_optional(variable.getInitialValueExpression()) : boost::none, false);
            auto findRes = variablesToMakeGlobal.find(variable.getExpressionVariable());
            if (findRes != variablesToMakeGlobal.end()) {
                bool makeVarGlobal = findRes->second;
                storm::jani::Variable const& createdVariable =
                    makeVarGlobal ? janiModel.addVariable(*newClockVariable) : automaton.addVariable(*newClockVariable);
                variableToVariableMap.emplace(variable.getExpressionVariable(), createdVariable);
            } else {
                STORM_LOG_INFO("Variable " << variable.getName() << " is declared but never used.");
            }
        }

        automaton.setInitialStatesRestriction(manager->boolean(true));

        // Create a single location that will have all the edges.
        uint64_t onlyLocationIndex = automaton.addLocation(storm::jani::Location("l"));
        automaton.addInitialLocation(onlyLocationIndex);

        if (module.hasInvariant()) {
            storm::jani::Location& onlyLocation = automaton.getLocation(onlyLocationIndex);
            onlyLocation.setTimeProgressInvariant(module.getInvariant());
        }

        // If we are translating the first module that has the action, we need to add the transient assignments to the location.
        // However, in standard compliant JANI, there are no state rewards
        if (firstModule) {
            storm::jani::Location& onlyLocation = automaton.getLocation(onlyLocationIndex);
            for (auto const& assignment : transientLocationAssignments) {
                onlyLocation.addTransientAssignment(assignment);
            }
        }

        for (auto const& command : module.getCommands()) {
            std::shared_ptr<storm::jani::TemplateEdge> templateEdge = std::make_shared<storm::jani::TemplateEdge>(command.getGuardExpression());
            automaton.registerTemplateEdge(templateEdge);
            actionIndicesOfModule.insert(janiModel.getActionIndex(command.getActionName()));

            boost::optional<storm::expressions::Expression> rateExpression;
            if (program.getModelType() == Program::ModelType::CTMC || program.getModelType() == Program::ModelType::CTMDP ||
                (program.getModelType() == Program::ModelType::MA && command.isMarkovian())) {
                for (auto const& update : command.getUpdates()) {
                    if (rateExpression) {
                        rateExpression = rateExpression.get() + update.getLikelihoodExpression();
                    } else {
                        rateExpression = update.getLikelihoodExpression();
                    }
                }
            }

            std::vector<std::pair<uint64_t, storm::expressions::Expression>> destinationLocationsAndProbabilities;
            for (auto const& update : command.getUpdates()) {
                std::vector<storm::jani::Assignment> assignments;
                for (auto const& assignment : update.getAssignments()) {
                    assignments.push_back(
                        storm::jani::Assignment(storm::jani::LValue(variableToVariableMap.at(assignment.getVariable()).get()), assignment.getExpression()));
                }

                if (rateExpression) {
                    destinationLocationsAndProbabilities.emplace_back(onlyLocationIndex, update.getLikelihoodExpression() / rateExpression.get());
                } else {
                    destinationLocationsAndProbabilities.emplace_back(onlyLocationIndex, update.getLikelihoodExpression());
                }

                templateEdge->addDestination(storm::jani::TemplateEdgeDestination(assignments));
            }

            // Then add the transient assignments for the rewards. Note that we may do this only for the first
            // module that has this action, so we remove the assignments from the global list of assignments
            // to add after adding them to the created edge.
            auto transientEdgeAssignmentsToAdd = transientEdgeAssignments.find(janiModel.getActionIndex(command.getActionName()));
            if (transientEdgeAssignmentsToAdd != transientEdgeAssignments.end()) {
                for (auto const& assignment : transientEdgeAssignmentsToAdd->second) {
                    templateEdge->addTransientAssignment(assignment);
                }
            }

            // Create the edge object.
            storm::jani::Edge newEdge;
            if (command.getActionName().empty()) {
                newEdge = storm::jani::Edge(onlyLocationIndex, storm::jani::Model::SILENT_ACTION_INDEX, rateExpression, templateEdge,
                                            destinationLocationsAndProbabilities);
            } else {
                newEdge = storm::jani::Edge(onlyLocationIndex, janiModel.getActionIndex(command.getActionName()), rateExpression, templateEdge,
                                            destinationLocationsAndProbabilities);
            }

            // Finally add the constructed edge.
            automaton.addEdge(newEdge);
        }

        // Now remove for all actions of this module the corresponding transient assignments, because we must
        // not deal out this reward multiple times.
        // NOTE: This only works for the standard composition and not for any custom compositions. This case
        // must be checked for earlier.
        for (auto actionIndex : actionIndicesOfModule) {
            // Do not delete rewards dealt out on non-synchronizing edges.
            if (actionIndex == janiModel.getActionIndex("")) {
                continue;
            }

            auto it = transientEdgeAssignments.find(actionIndex);
            if (it != transientEdgeAssignments.end()) {
                transientEdgeAssignments.erase(it);
            }
        }

        // if there are formulas and if the current module was renamed, we need to apply the renaming to the resulting function calls before replacing the
        // formula placeholders. Note that the formula placeholders of non-renamed modules are replaced later.
        if (program.getNumberOfFormulas() > 0 && module.isRenamedFromModule()) {
            auto renamedFormulaToFunctionCallMap = program.getSubstitutionForRenamedModule(module, formulaToFunctionCallMap);
            automaton.substitute(renamedFormulaToFunctionCallMap, false);
        }

        janiModel.addAutomaton(automaton);
        firstModule = false;
    }

    // Set the standard system composition. This is possible, because we reject non-standard compositions anyway.
    if (program.specifiesSystemComposition()) {
        CompositionToJaniVisitor visitor;
        janiModel.setSystemComposition(visitor.toJani(program.getSystemCompositionConstruct().getSystemComposition(), janiModel));
    } else {
        janiModel.setSystemComposition(janiModel.getStandardSystemComposition());
    }

    // if there are formulas, replace the remaining placeholder variables by actual function calls in all expressions
    if (program.getNumberOfFormulas() > 0) {
        janiModel.getModelFeatures().add(storm::jani::ModelFeature::Functions);
        janiModel.substitute(formulaToFunctionCallMap, false);
    }

    janiModel.finalize();

    return janiModel;
}

bool ToJaniConverter::labelsWereRenamed() const {
    return !labelRenaming.empty();
}

bool ToJaniConverter::rewardModelsWereRenamed() const {
    return !rewardModelRenaming.empty();
}

std::map<std::string, std::string> const& ToJaniConverter::getLabelRenaming() const {
    return labelRenaming;
}

std::map<std::string, std::string> const& ToJaniConverter::getRewardModelRenaming() const {
    return rewardModelRenaming;
}

storm::jani::Property ToJaniConverter::applyRenaming(storm::jani::Property const& property) const {
    storm::jani::Property result;
    bool initialized = false;

    if (rewardModelsWereRenamed()) {
        result = property.substituteRewardModelNames(getRewardModelRenaming());
        initialized = true;
    }
    if (labelsWereRenamed()) {
        storm::jani::Property const& currProperty = initialized ? result : property;
        result = currProperty.substituteLabels(getLabelRenaming());
        initialized = true;
    }
    if (!formulaToFunctionCallMap.empty()) {
        storm::jani::Property const& currProperty = initialized ? result : property;
        result = currProperty.substitute(formulaToFunctionCallMap);
        initialized = true;
    }
    if (!initialized) {
        result = property.clone();
    }
    return result;
}

std::vector<storm::jani::Property> ToJaniConverter::applyRenaming(std::vector<storm::jani::Property> const& properties) const {
    std::vector<storm::jani::Property> result;
    for (auto const& p : properties) {
        result.push_back(applyRenaming(p));
    }
    return result;
}
}  // namespace prism
}  // namespace storm
