#include "storm/modelchecker/multiobjective/deterministicScheds/DeterministicSchedsLpChecker.h"

#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/modelchecker/multiobjective/deterministicScheds/VisitingTimesHelper.h"
#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/MaximalEndComponentDecomposition.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/solver.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm::modelchecker::multiobjective {

template<typename ModelType, typename GeometryValueType>
DeterministicSchedsLpChecker<ModelType, GeometryValueType>::DeterministicSchedsLpChecker(
    ModelType const& model, std::vector<DeterministicSchedsObjectiveHelper<ModelType>> const& objectiveHelper)
    : model(model), objectiveHelper(objectiveHelper), numLpQueries(0) {
    // intentionally left empty
}

template<typename ModelType, typename GeometryValueType>
void DeterministicSchedsLpChecker<ModelType, GeometryValueType>::initialize(Environment const& env) {
    if (!lpModel) {
        swInit.start();
        initializeLpModel(env);
        swInit.stop();
    }
}

template<typename ModelType, typename GeometryValueType>
std::string DeterministicSchedsLpChecker<ModelType, GeometryValueType>::getStatistics(std::string const& prefix) const {
    std::stringstream out;
    out << prefix << swAll << " seconds for LP Checker including... \n";
    out << prefix << "  " << swInit << " seconds for LP initialization\n";
    out << prefix << "  " << swCheckWeightVectors << " seconds for checking weight vectors\n";
    out << prefix << "  " << swCheckAreas << " seconds for checking areas\n";
    out << prefix << "  " << swValidate << " seconds for validating LP solutions\n";
    out << prefix << "  " << numLpQueries << " calls to LP optimization\n";
    return out.str();
}

template<typename ModelType, typename GeometryValueType>
void DeterministicSchedsLpChecker<ModelType, GeometryValueType>::setCurrentWeightVector(Environment const& env,
                                                                                        std::vector<GeometryValueType> const& weightVector) {
    swAll.start();
    initialize(env);
    STORM_LOG_ASSERT(weightVector.size() == objectiveHelper.size(), "Setting a weight vector with invalid number of entries.");
    if (!currentWeightVector.empty()) {
        // Pop information of the current weight vector.
        lpModel->pop();
        lpModel->update();
        currentObjectiveVariables.clear();
    }

    currentWeightVector = weightVector;

    lpModel->push();
    // set up objective function for the given weight vector
    for (uint64_t objIndex = 0; objIndex < initialStateResults.size(); ++objIndex) {
        currentObjectiveVariables.push_back(
            lpModel->addUnboundedContinuousVariable("w_" + std::to_string(objIndex), storm::utility::convertNumber<ValueType>(weightVector[objIndex])));
        lpModel->addConstraint("", currentObjectiveVariables.back().getExpression() == initialStateResults[objIndex]);
    }
    lpModel->update();
    swAll.stop();
}

template<typename ModelType, typename GeometryValueType>
std::optional<std::pair<std::vector<GeometryValueType>, GeometryValueType>> DeterministicSchedsLpChecker<ModelType, GeometryValueType>::check(
    storm::Environment const& env, Polytope overapproximation, Point const& eps) {
    swAll.start();
    initialize(env);
    STORM_LOG_ASSERT(!currentWeightVector.empty(), "Checking invoked before specifying a weight vector.");
    STORM_LOG_TRACE("Checking a vertex...");
    lpModel->push();
    auto areaConstraints = overapproximation->getConstraints(lpModel->getManager(), currentObjectiveVariables);
    for (auto const& c : areaConstraints) {
        lpModel->addConstraint("", c);
    }

    if (!eps.empty()) {
        STORM_LOG_ASSERT(currentWeightVector.size() == eps.size(), "Eps vector has unexpected size.");
        // Specify the allowed gap between the obtained lower/upper objective bounds.
        GeometryValueType milpGap = storm::utility::vector::dotProduct(currentWeightVector, eps);
        lpModel->setMaximalMILPGap(storm::utility::convertNumber<ValueType>(milpGap), false);
    }
    lpModel->update();
    swCheckWeightVectors.start();
    lpModel->optimize();
    swCheckWeightVectors.stop();
    ++numLpQueries;
    //    STORM_PRINT_AND_LOG("Writing model to file '" << std::to_string(numLpQueries) << ".lp'\n";);
    //    lpModel->writeModelToFile(std::to_string(numLpQueries) + ".lp");
    std::optional<std::pair<Point, GeometryValueType>> result;
    if (!lpModel->isInfeasible()) {
        STORM_LOG_ASSERT(!lpModel->isUnbounded(), "LP result is unbounded.");
        swValidate.start();
        auto resultPoint = validateCurrentModel(env);
        swValidate.stop();
        auto resultValue = storm::utility::vector::dotProduct(resultPoint, currentWeightVector);
        if (!eps.empty()) {
            resultValue += storm::utility::convertNumber<GeometryValueType>(lpModel->getMILPGap(false));
        }
        result = std::make_pair(resultPoint, resultValue);
    }
    lpModel->pop();
    STORM_LOG_TRACE("\t Done checking a vertex...");
    swAll.stop();
    return result;
}

template<typename ModelType, typename GeometryValueType>
std::pair<std::vector<std::vector<GeometryValueType>>, std::vector<std::shared_ptr<storm::storage::geometry::Polytope<GeometryValueType>>>>
DeterministicSchedsLpChecker<ModelType, GeometryValueType>::check(storm::Environment const& env,
                                                                  storm::storage::geometry::PolytopeTree<GeometryValueType>& polytopeTree, Point const& eps) {
    swAll.start();
    initialize(env);
    STORM_LOG_INFO("Checking " << polytopeTree.toString());
    STORM_LOG_ASSERT(!currentWeightVector.empty(), "Checking invoked before specifying a weight vector.");
    if (polytopeTree.isEmpty()) {
        return {{}, {}};
    }

    // Specify a gap between the obtained lower/upper objective bounds.
    // Let p be the found solution point, q be the optimal (unknown) solution point, and w be the current weight vector.
    // The gap between the solution p and q is |w*p - w*q| = |w*(p-q)|
    GeometryValueType milpGap = storm::utility::vector::dotProduct(currentWeightVector, eps);
    lpModel->setMaximalMILPGap(storm::utility::convertNumber<ValueType>(milpGap), false);
    lpModel->update();

    std::vector<Point> foundPoints;
    std::vector<Polytope> infeasableAreas;
    checkRecursive(env, polytopeTree, eps, foundPoints, infeasableAreas, 0);
    swAll.stop();
    return {foundPoints, infeasableAreas};
}

template<typename ValueType>
auto createChoiceVariables(storm::solver::LpSolver<ValueType>& lpModel, storm::storage::SparseMatrix<ValueType> const& matrix) {
    std::vector<storm::expressions::Expression> choiceVariables;
    choiceVariables.reserve(matrix.getRowCount());
    for (uint64_t state = 0; state < matrix.getRowGroupCount(); ++state) {
        auto choices = matrix.getRowGroupIndices(state);
        if (choices.size() == 1) {
            choiceVariables.push_back(lpModel.getConstant(storm::utility::one<ValueType>()));  // Unique choice; no variable necessary
        } else {
            std::vector<storm::expressions::Expression> localChoices;
            for (auto const choice : choices) {
                localChoices.push_back(lpModel.addBinaryVariable("a" + std::to_string(choice)));
                choiceVariables.push_back(localChoices.back());
            }
            lpModel.update();
            lpModel.addConstraint("", storm::expressions::sum(localChoices) == lpModel.getConstant(1));
        }
    }
    return choiceVariables;
}

template<typename ValueType, typename HelperType>
std::vector<storm::expressions::Expression> classicConstraints(storm::solver::LpSolver<ValueType>& lpModel, bool const& indicatorConstraints,
                                                               storm::storage::SparseMatrix<ValueType> const& matrix, uint64_t initialState, uint64_t objIndex,
                                                               HelperType const& objectiveHelper,
                                                               std::vector<storm::expressions::Expression> const& choiceVariables) {
    // Create variables
    std::vector<storm::expressions::Expression> objectiveValueVariables(matrix.getRowGroupCount());
    for (auto const& state : objectiveHelper.getMaybeStates()) {
        if (indicatorConstraints) {
            objectiveValueVariables[state] = lpModel.addContinuousVariable("x_" + std::to_string(objIndex) + "_" + std::to_string(state));
        } else {
            objectiveValueVariables[state] =
                lpModel.addBoundedContinuousVariable("x_" + std::to_string(objIndex) + "_" + std::to_string(state),
                                                     objectiveHelper.getLowerValueBoundAtState(state), objectiveHelper.getUpperValueBoundAtState(state));
        }
    }
    std::vector<storm::expressions::Expression> reachVars;
    if (objectiveHelper.getInfinityCase() == HelperType::InfinityCase::HasNegativeInfinite) {
        reachVars.assign(matrix.getRowGroupCount(), {});
        for (auto const& state : objectiveHelper.getRewMinusInfEStates()) {
            reachVars[state] = lpModel.addBinaryVariable("c_" + std::to_string(objIndex) + "_" + std::to_string(state));
        }
        STORM_LOG_ASSERT(objectiveHelper.getRewMinusInfEStates().get(initialState), "Initial state must be in RewMinusInfEStates.");
        lpModel.update();
        lpModel.addConstraint("", reachVars[initialState] == lpModel.getConstant(storm::utility::one<ValueType>()));
    }
    lpModel.update();
    for (auto const& state : objectiveHelper.getMaybeStates()) {
        bool const requireReachConstraints =
            objectiveHelper.getInfinityCase() == HelperType::InfinityCase::HasNegativeInfinite && objectiveHelper.getRewMinusInfEStates().get(state);
        for (auto choice : matrix.getRowGroupIndices(state)) {
            auto const& choiceVarAsExpression = choiceVariables.at(choice);
            STORM_LOG_ASSERT(choiceVarAsExpression.isVariable() ||
                                 (!choiceVarAsExpression.containsVariables() && storm::utility::isOne(choiceVarAsExpression.evaluateAsRational())),
                             "Unexpected kind of choice variable: " << choiceVarAsExpression);
            std::vector<storm::expressions::Expression> summands;
            if (!indicatorConstraints && choiceVarAsExpression.isVariable()) {
                summands.push_back((lpModel.getConstant(storm::utility::one<ValueType>()) - choiceVarAsExpression) *
                                   lpModel.getConstant(objectiveHelper.getUpperValueBoundAtState(state) - objectiveHelper.getLowerValueBoundAtState(state)));
            }
            if (auto findRes = objectiveHelper.getChoiceRewards().find(choice); findRes != objectiveHelper.getChoiceRewards().end()) {
                auto rewExpr = lpModel.getConstant(findRes->second);
                if (requireReachConstraints) {
                    summands.push_back(reachVars[state] * rewExpr);
                } else {
                    summands.push_back(rewExpr);
                }
            }
            for (auto const& succ : matrix.getRow(choice)) {
                if (objectiveHelper.getMaybeStates().get(succ.getColumn())) {
                    summands.push_back(lpModel.getConstant(succ.getValue()) * objectiveValueVariables.at(succ.getColumn()));
                }
                if (requireReachConstraints && objectiveHelper.getRewMinusInfEStates().get(succ.getColumn())) {
                    lpModel.addConstraint(
                        "", reachVars[state] <= reachVars[succ.getColumn()] + lpModel.getConstant(storm::utility::one<ValueType>()) - choiceVarAsExpression);
                }
            }
            if (summands.empty()) {
                summands.push_back(lpModel.getConstant(storm::utility::zero<ValueType>()));
            }
            if (indicatorConstraints && choiceVarAsExpression.isVariable()) {
                auto choiceVar = choiceVarAsExpression.getBaseExpression().asVariableExpression().getVariable();
                lpModel.addIndicatorConstraint("", choiceVar, true, objectiveValueVariables.at(state) <= storm::expressions::sum(summands));
            } else {
                lpModel.addConstraint("", objectiveValueVariables.at(state) <= storm::expressions::sum(summands));
            }
        }
    }
    return objectiveValueVariables;
}

/// Computes the set of problematic MECS with the objective indices that induced them
/// An EC is problematic if its contained in the "maybestates" of an objective and only considers zero-reward choices.
template<typename ValueType, typename ObjHelperType>
auto computeProblematicMecs(storm::storage::SparseMatrix<ValueType> const& matrix, storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                            std::vector<ObjHelperType> const& objectiveHelper) {
    std::vector<std::pair<storm::storage::MaximalEndComponent, std::vector<uint64_t>>> problMecs;
    for (uint64_t objIndex = 0; objIndex < objectiveHelper.size(); ++objIndex) {
        auto const& obj = objectiveHelper[objIndex];
        storm::storage::MaximalEndComponentDecomposition<ValueType> objMecs(matrix, backwardTransitions, obj.getMaybeStates(),
                                                                            obj.getRelevantZeroRewardChoices());
        for (auto& newMec : objMecs) {
            bool found = false;
            for (auto& problMec : problMecs) {
                if (problMec.first == newMec) {
                    problMec.second.push_back(objIndex);
                    found = true;
                    break;
                }
            }
            if (!found) {
                problMecs.emplace_back(std::move(newMec), std::vector<uint64_t>({objIndex}));
            }
        }
    }
    STORM_LOG_DEBUG("Found " << problMecs.size() << " problematic ECs." << std::endl);
    return problMecs;
}

template<typename ValueType, typename UpperBoundsGetterType>
auto problematicMecConstraintsExpVisits(storm::solver::LpSolver<ValueType>& lpModel, bool const& indicatorConstraints, bool const& redundantConstraints,
                                        storm::storage::SparseMatrix<ValueType> const& matrix, storm::storage::SparseMatrix<ValueType> const& backwardChoices,
                                        uint64_t mecIndex, storm::storage::MaximalEndComponent const& problematicMec,
                                        std::vector<uint64_t> const& relevantObjectiveIndices,
                                        std::vector<std::vector<storm::expressions::Expression>> const& objectiveValueVariables,
                                        std::vector<storm::expressions::Expression> const& choiceVariables,
                                        UpperBoundsGetterType const& objectiveStateUpperBoundGetter) {
    storm::expressions::Expression visitsUpperBound;
    if (!indicatorConstraints) {
        visitsUpperBound = lpModel.getConstant(VisitingTimesHelper<ValueType>::computeMecVisitsUpperBound(problematicMec, matrix, true));
    }

    // Create variables and basic lower/upper bounds
    storm::storage::BitVector mecChoices(matrix.getRowCount(), false);
    std::map<uint64_t, storm::expressions::Expression> expVisitsVars;           // z^C_{s,act}
    std::map<uint64_t, storm::expressions::Expression> botVars;                 // z^C_{s,bot}
    std::map<uint64_t, storm::expressions::Expression> bsccIndicatorVariables;  // b^C_{s}
    for (auto const& stateChoices : problematicMec) {
        auto const state = stateChoices.first;
        auto bsccIndicatorVar = lpModel.addBinaryVariable("b_" + std::to_string(mecIndex) + "_" + std::to_string(state));
        bsccIndicatorVariables.emplace(state, bsccIndicatorVar.getExpression());
        std::string visitsVarPref = "z_" + std::to_string(mecIndex) + "_";
        auto stateBotVisitsVar =
            lpModel.addLowerBoundedContinuousVariable(visitsVarPref + std::to_string(state) + "bot", storm::utility::zero<ValueType>()).getExpression();
        botVars.emplace(state, stateBotVisitsVar);
        lpModel.update();
        if (indicatorConstraints) {
            lpModel.addIndicatorConstraint("", bsccIndicatorVar, false, stateBotVisitsVar <= lpModel.getConstant(storm::utility::zero<ValueType>()));
        } else {
            lpModel.addConstraint("", stateBotVisitsVar <= bsccIndicatorVar.getExpression() * visitsUpperBound);
        }
        for (auto choice : matrix.getRowGroupIndices(state)) {
            auto stateActionVisitsVar =
                lpModel.addLowerBoundedContinuousVariable(visitsVarPref + std::to_string(choice), storm::utility::zero<ValueType>()).getExpression();
            lpModel.update();
            if (indicatorConstraints) {
                if (auto const& a = choiceVariables[choice]; a.isVariable()) {
                    auto aVar = a.getBaseExpression().asVariableExpression().getVariable();
                    lpModel.addIndicatorConstraint("", aVar, false, stateActionVisitsVar <= lpModel.getConstant(storm::utility::zero<ValueType>()));
                }
            } else {
                lpModel.addConstraint("", stateActionVisitsVar <= choiceVariables[choice] * visitsUpperBound);
            }
            expVisitsVars.emplace(choice, stateActionVisitsVar);
        }
        for (auto const& ecChoice : stateChoices.second) {
            mecChoices.set(ecChoice, true);
        }
        for (auto const& objIndex : relevantObjectiveIndices) {
            if (indicatorConstraints) {
                lpModel.addIndicatorConstraint("", bsccIndicatorVar, true,
                                               objectiveValueVariables[objIndex][state] <= lpModel.getConstant(storm::utility::zero<ValueType>()));
            } else {
                auto const upperBnd = lpModel.getConstant(objectiveStateUpperBoundGetter(objIndex, state));
                lpModel.addConstraint("", objectiveValueVariables[objIndex][state] <= upperBnd - upperBnd * bsccIndicatorVar.getExpression());
            }
        }
    }

    // Create visits constraints
    auto const initProb = lpModel.getConstant(storm::utility::one<ValueType>() / storm::utility::convertNumber<ValueType, uint64_t>(problematicMec.size()));
    std::vector<storm::expressions::Expression> outVisitsSummands;
    for (auto const& stateChoices : problematicMec) {
        auto const state = stateChoices.first;
        auto const& choices = stateChoices.second;
        auto const& stateBotVar = botVars.at(state);
        outVisitsSummands.push_back(stateBotVar);
        std::vector<storm::expressions::Expression> stateVisitsSummands;
        stateVisitsSummands.push_back(stateBotVar);
        for (auto choice : matrix.getRowGroupIndices(state)) {
            auto const& choiceVisitsVar = expVisitsVars.at(choice);
            stateVisitsSummands.push_back(choiceVisitsVar);
            if (choices.count(choice) != 0) {
                if (redundantConstraints) {
                    for (auto const& postElem : matrix.getRow(choice)) {
                        if (storm::utility::isZero(postElem.getValue())) {
                            continue;
                        }
                        auto succ = postElem.getColumn();
                        lpModel.addConstraint("", bsccIndicatorVariables.at(state) + choiceVariables.at(choice) <=
                                                      lpModel.getConstant(storm::utility::one<ValueType>()) + bsccIndicatorVariables.at(succ));
                    }
                }
            } else {
                outVisitsSummands.push_back(choiceVisitsVar);
            }
        }
        for (auto const& preEntry : backwardChoices.getRow(state)) {
            uint64_t const preChoice = preEntry.getColumn();
            if (mecChoices.get(preChoice)) {
                ValueType preProb =
                    storm::utility::one<ValueType>() / storm::utility::convertNumber<ValueType, uint64_t>(matrix.getRow(preChoice).getNumberOfEntries());
                stateVisitsSummands.push_back(lpModel.getConstant(-preProb) * expVisitsVars.at(preChoice));
            }
        }
        lpModel.addConstraint("", storm::expressions::sum(stateVisitsSummands) == initProb);
    }
    lpModel.addConstraint("", storm::expressions::sum(outVisitsSummands) == lpModel.getConstant(storm::utility::one<ValueType>()));
}

template<typename ValueType, typename UpperBoundsGetterType>
auto problematicMecConstraintsOrder(storm::solver::LpSolver<ValueType>& lpModel, bool const& indicatorConstraints, bool const& redundantConstraints,
                                    storm::storage::SparseMatrix<ValueType> const& matrix, uint64_t mecIndex,
                                    storm::storage::MaximalEndComponent const& problematicMec, std::vector<uint64_t> const& relevantObjectiveIndices,
                                    std::vector<storm::expressions::Expression> const& choiceVariables,
                                    std::vector<std::vector<storm::expressions::Expression>> const& objectiveValueVariables,
                                    UpperBoundsGetterType const& objectiveStateUpperBoundGetter) {
    // Create bscc indicator and order variables with basic lower/upper bounds
    storm::storage::BitVector mecChoices(matrix.getRowCount(), false);
    std::map<uint64_t, storm::expressions::Expression> bsccIndicatorVariables;  // b^C_{s}
    std::map<uint64_t, storm::expressions::Expression> orderVariables;          // r^C_{s}
    for (auto const& stateChoices : problematicMec) {
        auto const state = stateChoices.first;
        auto bsccIndicatorVar = lpModel.addBinaryVariable("b_" + std::to_string(mecIndex) + "_" + std::to_string(state));
        bsccIndicatorVariables.emplace(state, bsccIndicatorVar.getExpression());
        auto orderVar = lpModel
                            .addBoundedContinuousVariable("r_" + std::to_string(mecIndex) + "_" + std::to_string(state), storm::utility::zero<ValueType>(),
                                                          storm::utility::one<ValueType>())
                            .getExpression();
        lpModel.update();
        orderVariables.emplace(state, orderVar);
        for (auto const& ecChoice : stateChoices.second) {
            mecChoices.set(ecChoice, true);
        }
        for (auto const& objIndex : relevantObjectiveIndices) {
            if (indicatorConstraints) {
                lpModel.addIndicatorConstraint("", bsccIndicatorVar, true,
                                               objectiveValueVariables[objIndex][state] <= lpModel.getConstant(storm::utility::zero<ValueType>()));
            } else {
                auto const upperBnd = lpModel.getConstant(objectiveStateUpperBoundGetter(objIndex, state));
                lpModel.addConstraint("", objectiveValueVariables[objIndex][state] <= upperBnd - upperBnd * bsccIndicatorVar.getExpression());
            }
        }
    }

    // Create order constraints
    auto const minDiff = lpModel.getConstant(-storm::utility::one<ValueType>() / storm::utility::convertNumber<ValueType, uint64_t>(problematicMec.size()));
    for (auto const& stateChoices : problematicMec) {
        auto const state = stateChoices.first;
        auto const& choices = stateChoices.second;
        auto const& bsccIndicatorVar = bsccIndicatorVariables.at(state);
        auto const& orderVar = orderVariables.at(state);
        for (auto choice : choices) {
            auto const& choiceVariable = choiceVariables.at(choice);
            std::vector<storm::expressions::Expression> choiceConstraint;
            choiceConstraint.push_back(bsccIndicatorVar);
            std::string const transSelectPrefix = "d_" + std::to_string(mecIndex) + "_" + std::to_string(choice) + "_";
            for (auto const& postElem : matrix.getRow(choice)) {
                if (storm::utility::isZero(postElem.getValue())) {
                    continue;
                }
                auto succ = postElem.getColumn();
                if (redundantConstraints) {
                    lpModel.addConstraint(
                        "", bsccIndicatorVar + choiceVariable <= lpModel.getConstant(storm::utility::one<ValueType>()) + bsccIndicatorVariables.at(succ));
                }
                auto transVar = lpModel.addBinaryVariable(transSelectPrefix + std::to_string(succ)).getExpression();
                lpModel.update();
                choiceConstraint.push_back(transVar);
                lpModel.addConstraint("", transVar <= choiceVariable);
                lpModel.addConstraint("", orderVar <= minDiff + orderVariables.at(succ) + lpModel.getConstant(storm::utility::one<ValueType>()) - transVar);
            }
            lpModel.addConstraint("", choiceVariable <= storm::expressions::sum(choiceConstraint));
        }
    }
}

template<typename ValueType, typename HelperType>
std::vector<storm::expressions::Expression> expVisitsConstraints(storm::solver::LpSolver<ValueType>& lpModel, bool const& indicatorConstraints,
                                                                 storm::storage::SparseMatrix<ValueType> const& matrix,
                                                                 storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                 storm::storage::SparseMatrix<ValueType> const& backwardChoices, uint64_t initialState,
                                                                 std::vector<HelperType> const& objectiveHelper,
                                                                 std::vector<storm::expressions::Expression> const& choiceVariables) {
    auto objHelpIt = objectiveHelper.begin();
    storm::storage::BitVector anyMaybeStates = objHelpIt->getMaybeStates();
    for (++objHelpIt; objHelpIt != objectiveHelper.end(); ++objHelpIt) {
        anyMaybeStates |= objHelpIt->getMaybeStates();
    }
    storm::storage::BitVector allZeroRewardChoices(matrix.getRowCount(), true);
    for (auto const& oh : objectiveHelper) {
        for (auto const& rew : oh.getChoiceRewards()) {
            STORM_LOG_ASSERT(!storm::utility::isZero(rew.second), "Reward value is zero.");
            allZeroRewardChoices.set(rew.first, false);
        }
    }
    storm::storage::MaximalEndComponentDecomposition<ValueType> mecs(matrix, backwardTransitions, anyMaybeStates, allZeroRewardChoices);
    storm::storage::BitVector mecStates(matrix.getRowGroupCount(), false);
    for (auto const& mec : mecs) {
        for (auto const& sc : mec) {
            mecStates.set(sc.first, true);
        }
    }
    std::vector<ValueType> maxVisits;
    if (!indicatorConstraints) {
        maxVisits = VisitingTimesHelper<ValueType>::computeUpperBoundsOnExpectedVisitingTimes(anyMaybeStates, matrix, backwardTransitions);
    }

    // Create variables and basic bounds
    std::vector<storm::expressions::Expression> choiceVisitsVars(matrix.getRowCount()), botVisitsVars(matrix.getRowGroupCount()),
        bsccVars(matrix.getRowGroupCount());
    for (auto state : anyMaybeStates) {
        STORM_LOG_ASSERT(indicatorConstraints || maxVisits[state] >= storm::utility::zero<ValueType>(), "Unexpected negative max visits.");
        for (auto choice : matrix.getRowGroupIndices(state)) {
            choiceVisitsVars[choice] =
                lpModel.addLowerBoundedContinuousVariable("y_" + std::to_string(choice), storm::utility::zero<ValueType>()).getExpression();
            lpModel.update();
            if (indicatorConstraints) {
                if (auto const& a = choiceVariables[choice]; a.isVariable()) {
                    auto aVar = a.getBaseExpression().asVariableExpression().getVariable();
                    lpModel.addIndicatorConstraint("", aVar, false, choiceVisitsVars[choice] <= lpModel.getConstant(storm::utility::zero<ValueType>()));
                }
            } else {
                lpModel.addConstraint("", choiceVisitsVars[choice] <= choiceVariables.at(choice) * lpModel.getConstant(maxVisits[state]));
            }
        }
        if (mecStates.get(state)) {
            bsccVars[state] = lpModel.addBinaryVariable("b_" + std::to_string(state)).getExpression();
            botVisitsVars[state] =
                lpModel.addLowerBoundedContinuousVariable("y_" + std::to_string(state) + "bot", storm::utility::zero<ValueType>()).getExpression();
            lpModel.update();
            if (indicatorConstraints) {
                lpModel.addIndicatorConstraint("", bsccVars[state].getBaseExpression().asVariableExpression().getVariable(), false,
                                               botVisitsVars[state] <= lpModel.getConstant(storm::utility::zero<ValueType>()));
            } else {
                lpModel.addConstraint("", botVisitsVars[state] <= bsccVars[state] * lpModel.getConstant(maxVisits[state]));
            }
        }
    }

    // Add expected visiting times constraints
    auto notMaybe = ~anyMaybeStates;
    std::vector<storm::expressions::Expression> outSummands;
    for (auto state : anyMaybeStates) {
        std::vector<storm::expressions::Expression> visitsSummands;
        if (mecStates.get(state)) {
            visitsSummands.push_back(-botVisitsVars[state]);
            outSummands.push_back(botVisitsVars[state]);
        }
        for (auto choice : matrix.getRowGroupIndices(state)) {
            visitsSummands.push_back(-choiceVisitsVars[choice]);
            if (auto outProb = matrix.getConstrainedRowSum(choice, notMaybe); !storm::utility::isZero(outProb)) {
                outSummands.push_back(lpModel.getConstant(outProb) * choiceVisitsVars[choice]);
            }
        }
        if (state == initialState) {
            visitsSummands.push_back(lpModel.getConstant(storm::utility::one<ValueType>()));
        }
        for (auto const& preEntry : backwardChoices.getRow(state)) {
            STORM_LOG_ASSERT(choiceVisitsVars[preEntry.getColumn()].isInitialized(), "Choice visit variable not initialized.");
            visitsSummands.push_back(lpModel.getConstant(preEntry.getValue()) * choiceVisitsVars[preEntry.getColumn()]);
        }
        lpModel.addConstraint("", storm::expressions::sum(visitsSummands) == lpModel.getConstant(storm::utility::zero<ValueType>()));
    }
    lpModel.addConstraint("", storm::expressions::sum(outSummands) == lpModel.getConstant(storm::utility::one<ValueType>()));

    // Add bscc constraints
    for (auto const& mec : mecs) {
        for (auto const& stateChoices : mec) {
            auto const& state = stateChoices.first;
            for (auto choice : matrix.getRowGroupIndices(state)) {
                if (stateChoices.second.count(choice) != 0) {
                    for (auto const& succ : matrix.getRow(choice)) {
                        if (storm::utility::isZero(succ.getValue())) {
                            continue;
                        }
                        STORM_LOG_ASSERT(mecStates.get(succ.getColumn()), "MEC state not set for successor.");
                        lpModel.addConstraint("", bsccVars[state] <= bsccVars[succ.getColumn()] + lpModel.getConstant(storm::utility::one<ValueType>()) -
                                                                         choiceVariables[choice]);
                    }
                } else {
                    lpModel.addConstraint("", bsccVars[state] <= lpModel.getConstant(storm::utility::one<ValueType>()) - choiceVariables[choice]);
                }
            }
        }
    }

    // Add objective values
    std::vector<storm::expressions::Expression> objectiveValueVariables;
    for (uint64_t objIndex = 0; objIndex < objectiveHelper.size(); ++objIndex) {
        if (objectiveHelper[objIndex].getMaybeStates().get(initialState)) {
            objectiveValueVariables.push_back(lpModel.addUnboundedContinuousVariable("x_" + std::to_string(objIndex)).getExpression());
            lpModel.update();
            std::vector<storm::expressions::Expression> summands;
            for (auto const& objRew : objectiveHelper[objIndex].getChoiceRewards()) {
                STORM_LOG_ASSERT(choiceVisitsVars[objRew.first].isInitialized(), "Choice visit variable not initialized.");
                summands.push_back(choiceVisitsVars[objRew.first] * lpModel.getConstant(objRew.second));
            }
            lpModel.addConstraint("", objectiveValueVariables.back() == storm::expressions::sum(summands));
        } else {
            objectiveValueVariables.push_back(lpModel.getConstant(objectiveHelper[objIndex].getConstantInitialStateValue()));
        }
    }
    return objectiveValueVariables;
}

template<typename HelperType>
bool useFlowEncoding(storm::Environment const& env, std::vector<HelperType> const& objectiveHelper) {
    bool supportsFlowEncoding = std::all_of(objectiveHelper.begin(), objectiveHelper.end(), [](auto const& h) { return h.isTotalRewardObjective(); });
    switch (env.modelchecker().multi().getEncodingType()) {
        case storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Auto:
            return supportsFlowEncoding;
        case storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Flow:
            STORM_LOG_THROW(supportsFlowEncoding, storm::exceptions::InvalidOperationException,
                            "Flow encoding only applicable if all objectives are (transformable to) total reward objectives.");
            return true;
        default:
            return false;
    }
}

template<typename ModelType, typename GeometryValueType>
void DeterministicSchedsLpChecker<ModelType, GeometryValueType>::initializeLpModel(Environment const& env) {
    STORM_LOG_INFO("Initializing LP model with " << model.getNumberOfStates() << " states.");
    flowEncoding = useFlowEncoding(env, objectiveHelper);
    STORM_LOG_INFO("Using " << (flowEncoding ? "flow" : "classical") << " encoding.\n");
    uint64_t initialState = *model.getInitialStates().begin();
    auto backwardTransitions = model.getBackwardTransitions();
    auto backwardChoices = model.getTransitionMatrix().transpose();
    STORM_LOG_WARN_COND(!env.solver().isLpSolverTypeSetFromDefaultValue() || env.solver().getLpSolverType() == storm::solver::LpSolverType::Gurobi,
                        "The selected MILP solver might not perform well. Consider installing / using Gurobi.");
    lpModel = storm::utility::solver::getLpSolver<ValueType>(env, "model");

    lpModel->setOptimizationDirection(storm::solver::OptimizationDirection::Maximize);
    initialStateResults.clear();

    // Create choice variables.
    choiceVariables = createChoiceVariables(*lpModel, model.getTransitionMatrix());
    if (flowEncoding) {
        initialStateResults = expVisitsConstraints(*lpModel, env.modelchecker().multi().getUseIndicatorConstraints(), model.getTransitionMatrix(),
                                                   backwardTransitions, backwardChoices, initialState, objectiveHelper, choiceVariables);
    } else {
        std::vector<std::vector<storm::expressions::Expression>> objectiveValueVariables;
        initialStateResults.clear();
        for (uint64_t objIndex = 0; objIndex < objectiveHelper.size(); ++objIndex) {
            if (objectiveHelper[objIndex].getMaybeStates().get(initialState)) {
                objectiveValueVariables.push_back(classicConstraints(*lpModel, env.modelchecker().multi().getUseIndicatorConstraints(),
                                                                     model.getTransitionMatrix(), initialState, objIndex, objectiveHelper[objIndex],
                                                                     choiceVariables));
                initialStateResults.push_back(objectiveValueVariables.back()[initialState]);
            } else {
                initialStateResults.push_back(lpModel->getConstant(objectiveHelper[objIndex].getConstantInitialStateValue()));
            }
        }
        auto problematicMecs = computeProblematicMecs(model.getTransitionMatrix(), backwardTransitions, objectiveHelper);
        uint64_t mecIndex = 0;
        auto upperBoundsGetter = [&](uint64_t objIndex, uint64_t state) -> ValueType { return objectiveHelper[objIndex].getUpperValueBoundAtState(state); };
        for (auto const& mecObj : problematicMecs) {
            if (env.modelchecker().multi().getUseBsccOrderEncoding()) {
                problematicMecConstraintsOrder(*lpModel, env.modelchecker().multi().getUseIndicatorConstraints(),
                                               env.modelchecker().multi().getUseRedundantBsccConstraints(), model.getTransitionMatrix(), mecIndex, mecObj.first,
                                               mecObj.second, choiceVariables, objectiveValueVariables, upperBoundsGetter);
            } else {
                problematicMecConstraintsExpVisits(*lpModel, env.modelchecker().multi().getUseIndicatorConstraints(),
                                                   env.modelchecker().multi().getUseRedundantBsccConstraints(), model.getTransitionMatrix(), backwardChoices,
                                                   mecIndex, mecObj.first, mecObj.second, objectiveValueVariables, choiceVariables, upperBoundsGetter);
            }
            ++mecIndex;
        }
    }
    lpModel->update();
    STORM_LOG_INFO("Done initializing LP model.");
}

template<typename ModelType, typename GeometryValueType>
void DeterministicSchedsLpChecker<ModelType, GeometryValueType>::checkRecursive(Environment const& env,
                                                                                storm::storage::geometry::PolytopeTree<GeometryValueType>& polytopeTree,
                                                                                Point const& eps, std::vector<Point>& foundPoints,
                                                                                std::vector<Polytope>& infeasableAreas, uint64_t const& depth) {
    STORM_LOG_ASSERT(!polytopeTree.isEmpty(), "Tree node is empty.");
    STORM_LOG_ASSERT(!polytopeTree.getPolytope()->isEmpty(), "Tree node is empty.");
    STORM_LOG_TRACE("Checking at depth " << depth << ": " << polytopeTree.toString());

    lpModel->push();
    // Assert the constraints of the current polytope
    auto nodeConstraints = polytopeTree.getPolytope()->getConstraints(lpModel->getManager(), currentObjectiveVariables);
    for (auto const& constr : nodeConstraints) {
        lpModel->addConstraint("", constr);
    }
    lpModel->update();

    if (polytopeTree.getChildren().empty()) {
        // At leaf nodes we need to perform the actual check.

        // Numerical instabilities might yield a point that is not actually inside the nodeConstraints.
        // If the downward closure is disjoint from this tree node, we will not make further progress.
        // In this case, we sharpen the violating constraints just a little bit.
        // This way, valid solutions might be excluded, so technically, this can yield false negatives.
        // However, since we apparently are dealing with numerical algorithms, we can't be sure about correctness anyway.
        uint64_t num_sharpen = 0;
        auto halfspaces = polytopeTree.getPolytope()->getHalfspaces();
        while (true) {
            STORM_LOG_TRACE("\tSolving MILP...");
            swCheckAreas.start();
            lpModel->optimize();
            swCheckAreas.stop();
            ++numLpQueries;
            STORM_LOG_TRACE("\tDone solving MILP...");

            if (lpModel->isInfeasible()) {
                infeasableAreas.push_back(polytopeTree.getPolytope());
                polytopeTree.clear();
                break;
            } else {
                STORM_LOG_ASSERT(!lpModel->isUnbounded(), "LP result is unbounded.");
                swValidate.start();
                Point newPoint = validateCurrentModel(env);
                swValidate.stop();
                // Check whether this new point yields any progress.
                // There is no progress if (due to numerical inaccuracies) the downwardclosure (including points that are epsilon close to it) contained in this
                // polytope. We multiply eps by 0.999 so that points that lie on the boundary of polytope and downw. do not count in the intersection.
                Point newPointPlusEps = newPoint;
                storm::utility::vector::addScaledVector(newPointPlusEps, eps, storm::utility::convertNumber<GeometryValueType>(0.999));
                if (polytopeTree.getPolytope()->contains(newPoint) ||
                    !polytopeTree.getPolytope()
                         ->intersection(storm::storage::geometry::Polytope<GeometryValueType>::createDownwardClosure({newPointPlusEps}))
                         ->isEmpty()) {
                    GeometryValueType offset = storm::utility::convertNumber<GeometryValueType>(lpModel->getObjectiveValue());
                    // Get the gap between the found solution and the known bound.
                    offset += storm::utility::convertNumber<GeometryValueType>(lpModel->getMILPGap(false));
                    // we might want to shift the halfspace to guarantee that our point is included.
                    offset = std::max(offset, storm::utility::vector::dotProduct(currentWeightVector, newPoint));
                    auto halfspace = storm::storage::geometry::Halfspace<GeometryValueType>(currentWeightVector, offset).invert();
                    infeasableAreas.push_back(polytopeTree.getPolytope()->intersection(halfspace));
                    if (infeasableAreas.back()->isEmpty()) {
                        infeasableAreas.pop_back();
                    }
                    polytopeTree.setMinus(storm::storage::geometry::Polytope<GeometryValueType>::create({halfspace}));
                    foundPoints.push_back(newPoint);
                    polytopeTree.substractDownwardClosure(newPoint, eps);
                    if (!polytopeTree.isEmpty()) {
                        checkRecursive(env, polytopeTree, eps, foundPoints, infeasableAreas, depth);
                    }
                    break;
                } else {
                    // If we end up here, we have to sharpen the violated constraints for this polytope
                    for (auto& h : halfspaces) {
                        GeometryValueType distance = h.distance(newPoint);
                        // Check if the found point is outside of this halfspace
                        if (!storm::utility::isZero(distance)) {
                            // The issue has to be for some normal vector with a negative entry. Otherwise, the intersection with the downward closure wouldn't
                            // be empty
                            bool normalVectorContainsNegative = false;
                            for (auto const& hi : h.normalVector()) {
                                if (hi < storm::utility::zero<GeometryValueType>()) {
                                    normalVectorContainsNegative = true;
                                    break;
                                }
                            }
                            if (normalVectorContainsNegative) {
                                h.offset() -= distance / storm::utility::convertNumber<GeometryValueType, uint64_t>(2);
                                if (num_sharpen == 0) {
                                    lpModel->push();
                                }
                                lpModel->addConstraint("", h.toExpression(lpModel->getManager(), currentObjectiveVariables));
                                ++num_sharpen;
                                break;
                            }
                        }
                    }
                    STORM_LOG_TRACE("\tSharpened LP");
                }
            }
        }
        if (num_sharpen > 0) {
            STORM_LOG_WARN(
                "Numerical instabilities detected: LP Solver found an achievable point outside of the search area. The search area had to be sharpened "
                << num_sharpen << " times.");
            // pop sharpened constraints
            lpModel->pop();
        }

    } else {
        // Traverse all the non-empty children.
        for (uint64_t childId = 0; childId < polytopeTree.getChildren().size(); ++childId) {
            if (polytopeTree.getChildren()[childId].isEmpty()) {
                continue;
            }
            uint64_t newPointIndex = foundPoints.size();
            checkRecursive(env, polytopeTree.getChildren()[childId], eps, foundPoints, infeasableAreas, depth + 1);
            STORM_LOG_ASSERT(polytopeTree.getChildren()[childId].isEmpty(), "Expected empty children.");
            // Make the new points known to the right siblings
            for (; newPointIndex < foundPoints.size(); ++newPointIndex) {
                for (uint64_t siblingId = childId + 1; siblingId < polytopeTree.getChildren().size(); ++siblingId) {
                    polytopeTree.getChildren()[siblingId].substractDownwardClosure(foundPoints[newPointIndex], eps);
                }
            }
        }
        // All children are empty now, so this node becomes empty.
        polytopeTree.clear();
    }
    STORM_LOG_TRACE("Checking DONE at depth " << depth << " with node " << polytopeTree.toString());

    lpModel->pop();
    lpModel->update();
}

template<typename ModelType, typename GeometryValueType>
typename DeterministicSchedsLpChecker<ModelType, GeometryValueType>::Point DeterministicSchedsLpChecker<ModelType, GeometryValueType>::validateCurrentModel(
    Environment const& env) const {
    storm::storage::BitVector selectedChoices(model.getNumberOfChoices(), false);
    for (uint64_t state = 0; state < model.getNumberOfStates(); ++state) {
        auto choices = model.getTransitionMatrix().getRowGroupIndices(state);
        if (choices.size() == 1) {
            selectedChoices.set(*choices.begin());
        } else {
            bool choiceFound = false;
            for (auto choice : choices) {
                STORM_LOG_ASSERT(choiceVariables[choice].isVariable(), "Choice variable is not a variable.");
                if (lpModel->getBinaryValue(choiceVariables[choice].getBaseExpression().asVariableExpression().getVariable())) {
                    STORM_LOG_THROW(!choiceFound, storm::exceptions::UnexpectedException, "Multiple choices selected at state " << state << ".");
                    selectedChoices.set(choice, true);
                    choiceFound = true;
                }
            }
        }
    }

    Point inducedPoint;
    for (uint64_t objIndex = 0; objIndex < objectiveHelper.size(); ++objIndex) {
        ValueType inducedValue = objectiveHelper[objIndex].evaluateScheduler(env, selectedChoices);
        inducedPoint.push_back(storm::utility::convertNumber<GeometryValueType>(inducedValue));
        // If this objective has weight zero, the lp solution is not necessarily correct
        if (!storm::utility::isZero(currentWeightVector[objIndex])) {
            ValueType lpValue = lpModel->getContinuousValue(currentObjectiveVariables[objIndex]);
            double diff = storm::utility::convertNumber<double>(storm::utility::abs<ValueType>(inducedValue - lpValue));
            STORM_LOG_WARN_COND(diff <= 1e-4 * std::abs(storm::utility::convertNumber<double>(inducedValue)),
                                "Imprecise value for objective " << objIndex << ": LP says " << lpValue << " but scheduler induces " << inducedValue
                                                                 << " (difference is " << diff << ")");
        }
    }
    return inducedPoint;
}

template class DeterministicSchedsLpChecker<storm::models::sparse::Mdp<double>, storm::RationalNumber>;
template class DeterministicSchedsLpChecker<storm::models::sparse::Mdp<storm::RationalNumber>, storm::RationalNumber>;
template class DeterministicSchedsLpChecker<storm::models::sparse::MarkovAutomaton<double>, storm::RationalNumber>;
template class DeterministicSchedsLpChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>, storm::RationalNumber>;
}  // namespace storm::modelchecker::multiobjective
