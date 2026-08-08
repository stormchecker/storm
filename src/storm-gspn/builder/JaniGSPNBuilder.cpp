#include "JaniGSPNBuilder.h"

#include <memory>

#include "storm/logic/Formulas.h"

#include "storm/exceptions/InvalidModelException.h"
namespace storm {
namespace builder {

storm::jani::Model* JaniGSPNBuilder::build(std::string const& automatonName) {
    storm::jani::ModelType modelType = storm::jani::ModelType::MA;
    if (gspn.getNumberOfTimedTransitions() == 0) {
        modelType = storm::jani::ModelType::MDP;
    } else if (gspn.getNumberOfImmediateTransitions() == 0) {
        modelType = storm::jani::ModelType::CTMC;
    }
    storm::jani::Model* model = new storm::jani::Model(gspn.getName(), modelType, janiVersion, expressionManager);
    storm::jani::Automaton mainAutomaton(automatonName, expressionManager->declareIntegerVariable("loc"));
    addVariables(model);
    uint64_t locId = addLocation(mainAutomaton);
    addEdges(mainAutomaton, locId);
    model->addAutomaton(mainAutomaton);
    model->setStandardSystemComposition();
    model->getModelFeatures().add(storm::jani::ModelFeature::DerivedOperators);
    model->finalize();
    return model;
}

void JaniGSPNBuilder::addVariables(storm::jani::Model* model) {
    for (auto const& place : gspn.getPlaces()) {
        std::shared_ptr<storm::jani::Variable> janiVar = nullptr;
        if (!place.hasRestrictedCapacity()) {
            // Effectively no capacity limit known
            janiVar = storm::jani::Variable::makeIntegerVariable(place.getName(), expressionManager->getVariable(place.getName()),
                                                                 expressionManager->integer(place.getNumberOfInitialTokens()), false);
        } else {
            STORM_LOG_ASSERT(place.hasRestrictedCapacity(), "Place does not have restricted capacity.");
            janiVar = storm::jani::Variable::makeBoundedIntegerVariable(place.getName(), expressionManager->getVariable(place.getName()),
                                                                        expressionManager->integer(place.getNumberOfInitialTokens()), false,
                                                                        expressionManager->integer(0), expressionManager->integer(place.getCapacity()));
        }
        STORM_LOG_ASSERT(janiVar != nullptr, "Jani variable is null.");
        STORM_LOG_ASSERT(vars.count(place.getID()) == 0, "Variable already exists for this place.");
        vars[place.getID()] = &model->addVariable(*janiVar);
    }
}

uint64_t JaniGSPNBuilder::addLocation(storm::jani::Automaton& automaton) {
    uint64_t janiLoc = automaton.addLocation(storm::jani::Location("loc"));
    automaton.addInitialLocation("loc");
    return janiLoc;
}

void JaniGSPNBuilder::addEdges(storm::jani::Automaton& automaton, uint64_t locId) {
    uint64_t lastPriority = -1;
    storm::expressions::Expression lastPriorityGuard = expressionManager->boolean(false);
    storm::expressions::Expression priorityGuard = expressionManager->boolean(true);

    for (auto const& partition : gspn.getPartitions()) {
        storm::expressions::Expression guard = expressionManager->boolean(false);

        STORM_LOG_ASSERT(lastPriority >= partition.priority, "Priority decreased unexpectedly.");
        if (lastPriority > partition.priority) {
            priorityGuard = priorityGuard && !lastPriorityGuard;
            lastPriority = partition.priority;
        } else {
            STORM_LOG_ASSERT(lastPriority == partition.priority, "Priority mismatch after decrement.");
        }

        // Compute enabled weight expression.
        storm::expressions::Expression totalWeight = expressionManager->rational(0.0);
        for (auto const& transId : partition.transitions) {
            auto const& trans = gspn.getImmediateTransitions()[transId];
            if (trans.noWeightAttached()) {
                continue;
            }
            storm::expressions::Expression destguard = expressionManager->boolean(true);
            for (auto const& inPlaceEntry : trans.getInputPlaces()) {
                destguard = destguard && (vars[inPlaceEntry.first]->getExpressionVariable() >= inPlaceEntry.second);
            }
            for (auto const& inhibPlaceEntry : trans.getInhibitionPlaces()) {
                destguard = destguard && (vars[inhibPlaceEntry.first]->getExpressionVariable() < inhibPlaceEntry.second);
            }
            totalWeight = totalWeight + storm::expressions::ite(destguard, expressionManager->rational(trans.getWeight()), expressionManager->rational(0.0));
        }
        totalWeight = totalWeight.simplify();

        std::vector<storm::jani::OrderedAssignments> oas;
        std::vector<storm::expressions::Expression> probabilities;
        std::vector<uint64_t> destinationLocations;
        for (auto const& transId : partition.transitions) {
            auto const& trans = gspn.getImmediateTransitions()[transId];
            if (trans.noWeightAttached()) {
                std::cout << "ERROR -- no weights attached at transition\n";
                continue;
            }
            storm::expressions::Expression destguard = expressionManager->boolean(true);
            std::vector<storm::jani::Assignment> assignments;
            for (auto const& inPlaceEntry : trans.getInputPlaces()) {
                destguard = destguard && (vars[inPlaceEntry.first]->getExpressionVariable() >= inPlaceEntry.second);
                if (trans.getOutputPlaces().count(inPlaceEntry.first) == 0) {
                    assignments.emplace_back(storm::jani::LValue(*vars[inPlaceEntry.first]),
                                             (vars[inPlaceEntry.first])->getExpressionVariable() - inPlaceEntry.second);
                }
            }
            for (auto const& inhibPlaceEntry : trans.getInhibitionPlaces()) {
                destguard = destguard && (vars[inhibPlaceEntry.first]->getExpressionVariable() < inhibPlaceEntry.second);
            }
            for (auto const& outputPlaceEntry : trans.getOutputPlaces()) {
                if (trans.getInputPlaces().count(outputPlaceEntry.first) == 0) {
                    assignments.emplace_back(storm::jani::LValue(*vars[outputPlaceEntry.first]),
                                             (vars[outputPlaceEntry.first])->getExpressionVariable() + outputPlaceEntry.second);
                } else {
                    assignments.emplace_back(
                        storm::jani::LValue(*vars[outputPlaceEntry.first]),
                        (vars[outputPlaceEntry.first])->getExpressionVariable() + outputPlaceEntry.second - trans.getInputPlaces().at(outputPlaceEntry.first));
                }
            }
            destguard = destguard.simplify();
            guard = guard || destguard;

            oas.emplace_back(assignments);
            destinationLocations.emplace_back(locId);
            probabilities.emplace_back(
                storm::expressions::ite(destguard, (expressionManager->rational(trans.getWeight()) / totalWeight), expressionManager->rational(0.0)));
        }

        std::shared_ptr<storm::jani::TemplateEdge> templateEdge = std::make_shared<storm::jani::TemplateEdge>((priorityGuard && guard).simplify());
        automaton.registerTemplateEdge(templateEdge);

        for (auto const& oa : oas) {
            templateEdge->addDestination(storm::jani::TemplateEdgeDestination(oa));
        }
        storm::jani::Edge e(locId, storm::jani::Model::SILENT_ACTION_INDEX, boost::none, templateEdge, destinationLocations, probabilities);
        automaton.addEdge(e);
        lastPriorityGuard = lastPriorityGuard || guard;
    }
    for (auto const& trans : gspn.getTimedTransitions()) {
        if (storm::utility::isZero(trans.getRate())) {
            STORM_LOG_WARN("Transitions with rate zero are not allowed in JANI. Skipping this transition");
            continue;
        }
        storm::expressions::Expression guard = expressionManager->boolean(true);

        std::vector<storm::jani::Assignment> assignments;
        for (auto const& inPlaceEntry : trans.getInputPlaces()) {
            guard = guard && (vars[inPlaceEntry.first]->getExpressionVariable() >= inPlaceEntry.second);
            if (trans.getOutputPlaces().count(inPlaceEntry.first) == 0) {
                assignments.emplace_back(storm::jani::LValue(*vars[inPlaceEntry.first]),
                                         (vars[inPlaceEntry.first])->getExpressionVariable() - inPlaceEntry.second);
            }
        }
        for (auto const& inhibPlaceEntry : trans.getInhibitionPlaces()) {
            guard = guard && (vars[inhibPlaceEntry.first]->getExpressionVariable() < inhibPlaceEntry.second);
        }
        for (auto const& outputPlaceEntry : trans.getOutputPlaces()) {
            if (trans.getInputPlaces().count(outputPlaceEntry.first) == 0) {
                assignments.emplace_back(storm::jani::LValue(*vars[outputPlaceEntry.first]),
                                         (vars[outputPlaceEntry.first])->getExpressionVariable() + outputPlaceEntry.second);
            } else {
                assignments.emplace_back(
                    storm::jani::LValue(*vars[outputPlaceEntry.first]),
                    (vars[outputPlaceEntry.first])->getExpressionVariable() + outputPlaceEntry.second - trans.getInputPlaces().at(outputPlaceEntry.first));
            }
        }

        std::shared_ptr<storm::jani::TemplateEdge> templateEdge = std::make_shared<storm::jani::TemplateEdge>(guard);
        automaton.registerTemplateEdge(templateEdge);

        storm::expressions::Expression rate = expressionManager->rational(trans.getRate());
        if (trans.hasInfiniteServerSemantics() || (trans.hasKServerSemantics() && !trans.hasSingleServerSemantics())) {
            STORM_LOG_THROW(trans.hasKServerSemantics() || !trans.getInputPlaces().empty(), storm::exceptions::InvalidModelException,
                            "Unclear semantics: Found a transition with infinite-server semantics and without input place.");
            storm::expressions::Expression enablingDegree;
            bool firstArgumentOfMinExpression = true;
            if (trans.hasKServerSemantics()) {
                enablingDegree = expressionManager->integer(trans.getNumberOfServers());
                firstArgumentOfMinExpression = false;
            }
            for (auto const& inPlaceEntry : trans.getInputPlaces()) {
                storm::expressions::Expression enablingDegreeInPlace =
                    vars[inPlaceEntry.first]->getExpressionVariable() / expressionManager->integer(inPlaceEntry.second);  // Integer division!
                if (firstArgumentOfMinExpression == true) {
                    enablingDegree = enablingDegreeInPlace;
                    firstArgumentOfMinExpression = false;
                } else {
                    enablingDegree = storm::expressions::minimum(enablingDegree, enablingDegreeInPlace);
                }
            }
            rate = rate * enablingDegree;
        }

        templateEdge->addDestination(assignments);
        storm::jani::Edge e(locId, storm::jani::Model::SILENT_ACTION_INDEX, rate, templateEdge, {locId}, {expressionManager->integer(1)});
        automaton.addEdge(e);
    }
}

storm::jani::Variable const& JaniGSPNBuilder::addDeadlockTransientVariable(storm::jani::Model* model, std::string name, bool ignoreCapacities,
                                                                           bool ignoreInhibitorArcs, bool ignoreEmptyPlaces) {
    storm::expressions::Expression transientValue = expressionManager->boolean(true);

    // build the conjunction over all transitions
    std::vector<storm::gspn::Transition const*> transitions;
    transitions.reserve(gspn.getNumberOfImmediateTransitions() + gspn.getNumberOfTimedTransitions());
    for (auto const& t : gspn.getImmediateTransitions()) {
        transitions.push_back(&t);
    }
    for (auto const& t : gspn.getTimedTransitions()) {
        transitions.push_back(&t);
    }
    bool firstTransition = true;
    for (auto const& transition : transitions) {
        // build the disjunction over all in/out places and inhibitor arcs
        storm::expressions::Expression transitionDisabled = expressionManager->boolean(false);
        bool firstPlace = true;
        if (!ignoreEmptyPlaces) {
            for (auto const& placeIdMult : transition->getInputPlaces()) {
                storm::expressions::Expression placeBlocksTransition =
                    (vars.at(placeIdMult.first)->getExpressionVariable() < expressionManager->integer(placeIdMult.second));
                if (firstPlace) {
                    transitionDisabled = placeBlocksTransition;
                    firstPlace = false;
                } else {
                    transitionDisabled = transitionDisabled || placeBlocksTransition;
                }
            }
        }
        if (!ignoreInhibitorArcs) {
            for (auto const& placeIdMult : transition->getInhibitionPlaces()) {
                storm::expressions::Expression placeBlocksTransition =
                    (vars.at(placeIdMult.first)->getExpressionVariable() >= expressionManager->integer(placeIdMult.second));
                if (firstPlace) {
                    transitionDisabled = placeBlocksTransition;
                    firstPlace = false;
                } else {
                    transitionDisabled = transitionDisabled || placeBlocksTransition;
                }
            }
        }
        if (!ignoreCapacities) {
            for (auto const& placeIdMult : transition->getOutputPlaces()) {
                auto const& place = gspn.getPlace(placeIdMult.first);
                if (place->hasRestrictedCapacity()) {
                    storm::expressions::Expression placeBlocksTransition =
                        (vars.at(placeIdMult.first)->getExpressionVariable() + expressionManager->integer(placeIdMult.second) >
                         expressionManager->integer(place->getCapacity()));
                    if (firstPlace) {
                        transitionDisabled = placeBlocksTransition;
                        firstPlace = false;
                    } else {
                        transitionDisabled = transitionDisabled || placeBlocksTransition;
                    }
                }
            }
        }

        if (firstTransition) {
            transientValue = transitionDisabled;
            firstTransition = false;
        } else {
            transientValue = transientValue && transitionDisabled;
        }
    }

    return addTransientVariable(model, name, transientValue);
}

storm::jani::Variable const& JaniGSPNBuilder::addTransientVariable(storm::jani::Model* model, std::string name, storm::expressions::Expression expression) {
    auto exprVar = expressionManager->declareBooleanVariable(name);
    auto const& janiVar = model->addVariable(*storm::jani::Variable::makeBooleanVariable(name, exprVar, expressionManager->boolean(false), true));
    storm::jani::Assignment assignment(storm::jani::LValue(janiVar), expression);
    model->getAutomata().front().getLocations().front().addTransientAssignment(assignment);
    return janiVar;
}

std::string getUniqueVarName(storm::expressions::ExpressionManager const& manager, std::string name) {
    std::string res = name;
    while (manager.hasVariable(res)) {
        res.append("_");
    }
    return res;
}

std::vector<storm::jani::Property> JaniGSPNBuilder::getStandardProperties(storm::jani::Model* model,
                                                                          std::shared_ptr<storm::logic::AtomicExpressionFormula> atomicFormula,
                                                                          std::string name, std::string description, bool maximal) {
    std::vector<storm::jani::Property> standardProperties;
    std::string dirShort = maximal ? "Max" : "Min";
    std::string dirLong = maximal ? "maximal" : "minimal";
    storm::solver::OptimizationDirection optimizationDirection =
        maximal ? storm::solver::OptimizationDirection::Maximize : storm::solver::OptimizationDirection::Minimize;
    std::set<storm::expressions::Variable> emptySet;

    // Build reachability property
    auto reachFormula = std::make_shared<storm::logic::ProbabilityOperatorFormula>(
        std::make_shared<storm::logic::EventuallyFormula>(atomicFormula, storm::logic::FormulaContext::Probability),
        storm::logic::OperatorInformation(optimizationDirection));
    standardProperties.emplace_back(dirShort + "PrReach" + name, reachFormula, emptySet,
                                    "The " + dirLong + " probability to eventually reach " + description + ".");

    // Build time bounded reachability property
    // Add variable for time bound
    auto exprTB = expressionManager->declareRationalVariable(getUniqueVarName(*expressionManager, "TIME_BOUND"));
    auto janiTB = storm::jani::Constant(exprTB.getName(), exprTB);
    model->addConstant(janiTB);
    storm::logic::TimeBound tb(false, janiTB.getExpressionVariable().getExpression());
    storm::logic::TimeBoundReference tbr(storm::logic::TimeBoundType::Time);

    auto trueFormula = std::make_shared<storm::logic::BooleanLiteralFormula>(true);
    auto reachTimeBoundFormula = std::make_shared<storm::logic::ProbabilityOperatorFormula>(
        std::make_shared<storm::logic::BoundedUntilFormula>(trueFormula, atomicFormula, boost::none, tb, tbr),
        storm::logic::OperatorInformation(optimizationDirection));
    standardProperties.emplace_back(dirShort + "PrReach" + name + "TB", reachTimeBoundFormula, emptySet,
                                    "The " + dirLong + " probability to reach " + description + " within 'TIME_BOUND' steps.");

    // Use complementary direction for expected time
    dirShort = maximal ? "Min" : "Max";
    dirLong = maximal ? "minimal" : "maximal";
    optimizationDirection = maximal ? storm::solver::OptimizationDirection::Minimize : storm::solver::OptimizationDirection::Maximize;

    // Build expected time property
    auto expTimeFormula = std::make_shared<storm::logic::TimeOperatorFormula>(
        std::make_shared<storm::logic::EventuallyFormula>(atomicFormula, storm::logic::FormulaContext::Time),
        storm::logic::OperatorInformation(optimizationDirection));
    standardProperties.emplace_back(dirShort + "ExpTime" + name, expTimeFormula, emptySet, "The " + dirLong + " expected time to reach " + description + ".");
    return standardProperties;
}

std::vector<storm::jani::Property> JaniGSPNBuilder::getDeadlockProperties(storm::jani::Model* model) {
    auto const& deadlockVar = addDeadlockTransientVariable(model, getUniqueVarName(*expressionManager, "deadl"));
    auto deadlockFormula = std::make_shared<storm::logic::AtomicExpressionFormula>(deadlockVar.getExpressionVariable().getExpression());
    return getStandardProperties(model, deadlockFormula, "Deadlock", "a deadlock", true);
}

}  // namespace builder
}  // namespace storm
