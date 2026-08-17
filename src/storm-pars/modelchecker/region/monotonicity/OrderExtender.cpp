#include "OrderExtender.h"
#include <vector>

#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"

#include "storm-pars/api/export.h"
#include "storm-pars/api/region.h"
#include "storm-pars/modelchecker/region/SparseDtmcParameterLiftingModelChecker.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityHelper.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"

#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"

namespace storm {
namespace analysis {

template<typename ValueType, typename ConstantType>
OrderExtender<ValueType, ConstantType>::OrderExtender(std::shared_ptr<models::sparse::Model<ValueType>> model, std::shared_ptr<logic::Formula const> formula)
    : monotonicityChecker(MonotonicityChecker<ValueType>(model->getTransitionMatrix())) {
    this->model = model;
    this->matrix = model->getTransitionMatrix();
    this->numberOfStates = this->model->getNumberOfStates();
    this->formula = formula;
    this->assumptionMaker = new analysis::AssumptionMaker<ValueType, ConstantType>(matrix);
}

template<typename ValueType, typename ConstantType>
OrderExtender<ValueType, ConstantType>::OrderExtender(storm::storage::BitVector const& topStates, storm::storage::BitVector const& bottomStates,
                                                      storm::storage::SparseMatrix<ValueType> matrix)
    : monotonicityChecker(MonotonicityChecker<ValueType>(matrix)) {
    this->matrix = matrix;
    this->model = nullptr;
    this->monotonicityChecker = MonotonicityChecker<ValueType>(matrix);

    storm::storage::StronglyConnectedComponentDecompositionOptions options;
    options.forceTopologicalSort();

    this->numberOfStates = matrix.getColumnCount();
    std::vector<uint64_t> firstStates;

    storm::storage::BitVector subStates(topStates.size(), true);
    for (uint64_t state : topStates) {
        firstStates.push_back(state);
        subStates.set(state, false);
    }
    for (uint64_t state : bottomStates) {
        firstStates.push_back(state);
        subStates.set(state, false);
    }
    cyclic = storm::utility::graph::hasCycle(matrix, subStates);
    storm::storage::StronglyConnectedComponentDecomposition<ValueType> decomposition;
    if (cyclic) {
        decomposition = storm::storage::StronglyConnectedComponentDecomposition<ValueType>(matrix, options);
    }

    auto statesSorted = storm::utility::graph::getTopologicalSort(matrix.transpose(), firstStates);
    this->bottomTopOrder = std::make_shared<Order>(topStates, bottomStates, numberOfStates, std::move(decomposition), std::move(statesSorted));

    // Build stateMap
    for (uint_fast64_t state = 0; state < numberOfStates; ++state) {
        auto const& row = matrix.getRow(state);
        stateMap[state] = std::vector<uint_fast64_t>();
        std::set<VariableType> occurringVariables;

        for (auto& entry : matrix.getRow(state)) {
            // ignore self-loops when there are more transitions
            if (state != entry.getColumn() || row.getNumberOfEntries() == 1) {
                if (!subStates[entry.getColumn()] && !bottomTopOrder->contains(state)) {
                    bottomTopOrder->add(state);
                }
                stateMap[state].push_back(entry.getColumn());
            }
            storm::utility::parametric::gatherOccurringVariables(entry.getValue(), occurringVariables);
        }
        if (occurringVariables.empty()) {
            nonParametricStates.insert(state);
        }

        for (auto& var : occurringVariables) {
            occuringStatesAtVariable[var].push_back(state);
        }
        occuringVariablesAtState.push_back(std::move(occurringVariables));
    }

    this->assumptionMaker = new analysis::AssumptionMaker<ValueType, ConstantType>(matrix);
}

template<typename ValueType, typename ConstantType>
std::shared_ptr<Order> OrderExtender<ValueType, ConstantType>::getBottomTopOrder() {
    if (bottomTopOrder == nullptr) {
        STORM_LOG_ASSERT(model != nullptr, "Model is null.");
        STORM_LOG_THROW(matrix.getRowCount() == matrix.getColumnCount(), exceptions::NotSupportedException,
                        "Creating order not supported for non-square matrix.");
        modelchecker::SparsePropositionalModelChecker<models::sparse::Model<ValueType>> propositionalChecker(*model);
        storage::BitVector phiStates;
        storage::BitVector psiStates;
        STORM_LOG_ASSERT(formula->isProbabilityOperatorFormula(), "Expected probability operator formula.");
        if (formula->asProbabilityOperatorFormula().getSubformula().isUntilFormula()) {
            phiStates = propositionalChecker.check(formula->asProbabilityOperatorFormula().getSubformula().asUntilFormula().getLeftSubformula())
                            ->template asExplicitQualitativeCheckResult<ValueType>()
                            .getTruthValuesVector();
            psiStates = propositionalChecker.check(formula->asProbabilityOperatorFormula().getSubformula().asUntilFormula().getRightSubformula())
                            ->template asExplicitQualitativeCheckResult<ValueType>()
                            .getTruthValuesVector();
        } else {
            STORM_LOG_ASSERT(formula->asProbabilityOperatorFormula().getSubformula().isEventuallyFormula(), "Expected eventually formula.");
            phiStates = storage::BitVector(numberOfStates, true);
            psiStates = propositionalChecker.check(formula->asProbabilityOperatorFormula().getSubformula().asEventuallyFormula().getSubformula())
                            ->template asExplicitQualitativeCheckResult<ValueType>()
                            .getTruthValuesVector();
        }
        // Get the maybeStates
        std::pair<storage::BitVector, storage::BitVector> statesWithProbability01 =
            utility::graph::performProb01(this->model->getBackwardTransitions(), phiStates, psiStates);
        storage::BitVector topStates = statesWithProbability01.second;
        storage::BitVector bottomStates = statesWithProbability01.first;

        STORM_LOG_THROW(topStates.begin() != topStates.end(), exceptions::NotSupportedException, "Formula yields to no 1 states.");
        STORM_LOG_THROW(bottomStates.begin() != bottomStates.end(), exceptions::NotSupportedException, "Formula yields to no zero states.");
        auto& matrix = this->model->getTransitionMatrix();
        std::vector<uint64_t> firstStates;

        storm::storage::BitVector subStates(topStates.size(), true);
        for (auto state : topStates) {
            firstStates.push_back(state);
            subStates.set(state, false);
        }
        for (auto state : bottomStates) {
            firstStates.push_back(state);
            subStates.set(state, false);
        }
        cyclic = storm::utility::graph::hasCycle(matrix, subStates);
        storm::storage::StronglyConnectedComponentDecomposition<ValueType> decomposition;
        if (cyclic) {
            storm::storage::StronglyConnectedComponentDecompositionOptions options;
            options.forceTopologicalSort();
            decomposition = storm::storage::StronglyConnectedComponentDecomposition<ValueType>(matrix, options);
        }
        auto statesSorted = storm::utility::graph::getTopologicalSort(matrix.transpose(), firstStates);
        bottomTopOrder = std::make_shared<Order>(topStates, bottomStates, numberOfStates, std::move(decomposition), std::move(statesSorted));

        // Build stateMap
        for (uint_fast64_t state = 0; state < numberOfStates; ++state) {
            auto const& row = matrix.getRow(state);
            stateMap[state] = std::vector<uint_fast64_t>();
            std::set<VariableType> occurringVariables;

            for (auto& entry : matrix.getRow(state)) {
                // ignore self-loops when there are more transitions
                if (state != entry.getColumn() || row.getNumberOfEntries() == 1) {
                    //                            if (!subStates[entry.getColumn()] && !bottomTopOrder->contains(state)) {
                    //                                bottomTopOrder->add(state);
                    //                            }
                    stateMap[state].push_back(entry.getColumn());
                }
                storm::utility::parametric::gatherOccurringVariables(entry.getValue(), occurringVariables);
            }
            if (occurringVariables.empty()) {
                nonParametricStates.insert(state);
            }

            for (auto& var : occurringVariables) {
                occuringStatesAtVariable[var].push_back(state);
            }
            occuringVariablesAtState.push_back(std::move(occurringVariables));
        }
    }

    if (minValuesInit && maxValuesInit) {
        continueExtending[bottomTopOrder] = true;
        usePLA[bottomTopOrder] = true;
        minValues[bottomTopOrder] = std::move(minValuesInit.get());
        maxValues[bottomTopOrder] = std::move(maxValuesInit.get());
    } else {
        usePLA[bottomTopOrder] = false;
    }
    return bottomTopOrder;
}

template<typename ValueType, typename ConstantType>
std::tuple<std::shared_ptr<Order>, uint_fast64_t, uint_fast64_t> OrderExtender<ValueType, ConstantType>::toOrder(
    storage::ParameterRegion<ValueType> region, std::shared_ptr<MonotonicityResult<VariableType>> monRes) {
    return this->extendOrder(nullptr, region, monRes, nullptr);
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::handleAssumption(std::shared_ptr<Order> order,
                                                              std::shared_ptr<expressions::BinaryRelationExpression> assumption) const {
    STORM_LOG_ASSERT(assumption != nullptr, "Assumption is null.");
    STORM_LOG_ASSERT(assumption->getFirstOperand()->isVariable() && assumption->getSecondOperand()->isVariable(), "Expected variable operands.");

    expressions::Variable var1 = assumption->getFirstOperand()->asVariableExpression().getVariable();
    expressions::Variable var2 = assumption->getSecondOperand()->asVariableExpression().getVariable();
    auto const& val1 = std::stoul(var1.getName(), nullptr, 0);
    auto const& val2 = std::stoul(var2.getName(), nullptr, 0);

    STORM_LOG_ASSERT(order->compare(val1, val2) == Order::UNKNOWN, "Order should be UNKNOWN");

    Order::Node* n1 = order->getNode(val1);
    Order::Node* n2 = order->getNode(val2);

    if (assumption->getRelationType() == expressions::RelationType::Equal) {
        if (n1 != nullptr && n2 != nullptr) {
            order->mergeNodes(n1, n2);
        } else if (n1 != nullptr) {
            order->addToNode(val2, n1);
        } else if (n2 != nullptr) {
            order->addToNode(val1, n2);
        } else {
            order->add(val1);
            order->addToNode(val2, order->getNode(val1));
        }
    } else {
        STORM_LOG_ASSERT(assumption->getRelationType() == expressions::RelationType::Greater, "Expected Greater relation.");
        if (n1 != nullptr && n2 != nullptr) {
            order->addRelationNodes(n1, n2);
        } else if (n1 != nullptr) {
            order->addBetween(val2, n1, order->getBottom());
        } else if (n2 != nullptr) {
            order->addBetween(val1, order->getTop(), n2);
        } else {
            order->add(val1);
            order->addBetween(val2, order->getNode(val1), order->getBottom());
        }
    }
}

template<typename ValueType, typename ConstantType>
std::tuple<std::shared_ptr<Order>, uint_fast64_t, uint_fast64_t> OrderExtender<ValueType, ConstantType>::extendOrder(
    std::shared_ptr<Order> order, storm::storage::ParameterRegion<ValueType> region, std::shared_ptr<MonotonicityResult<VariableType>> monRes,
    std::shared_ptr<expressions::BinaryRelationExpression> assumption) {
    this->region = region;
    if (order == nullptr) {
        order = getBottomTopOrder();
        if (usePLA[order]) {
            auto& min = minValues[order];
            auto& max = maxValues[order];
            // Try to make the order as complete as possible based on pla results
            auto& statesSorted = order->getStatesSorted();
            auto itr = statesSorted.begin();
            while (itr != statesSorted.end()) {
                auto state = *itr;
                auto& successors = stateMap[state];
                bool all = true;
                for (uint_fast64_t i = 0; i < successors.size(); ++i) {
                    auto state1 = successors[i];
                    for (uint_fast64_t j = i + 1; j < successors.size(); ++j) {
                        auto state2 = successors[j];
                        if (min[state1] > max[state2]) {
                            if (!order->contains(state1)) {
                                order->add(state1);
                            }
                            if (!order->contains(state2)) {
                                order->add(state2);
                            }
                            order->addRelation(state1, state2, false);
                        } else if (min[state2] > max[state1]) {
                            if (!order->contains(state1)) {
                                order->add(state1);
                            }
                            if (!order->contains(state2)) {
                                order->add(state2);
                            }
                            order->addRelation(state2, state1, false);
                        } else if (min[state1] == max[state2] && max[state1] == min[state2]) {
                            if (!order->contains(state1) && !order->contains(state2)) {
                                order->add(state1);
                                order->addToNode(state2, order->getNode(state1));
                            } else if (!order->contains(state1)) {
                                order->addToNode(state1, order->getNode(state2));
                            } else if (!order->contains(state2)) {
                                order->addToNode(state2, order->getNode(state1));
                            } else {
                                order->merge(state1, state2);
                                STORM_LOG_ASSERT(!order->isInvalid(), "Order is invalid.");
                            }
                        } else {
                            all = false;
                        }
                    }
                }
                if (all) {
                    STORM_LOG_INFO("All successors of state " << state << " sorted based on min max values");
                }
                ++itr;
            }
        }
        continueExtending[order] = true;
    }
    if (continueExtending[order] || assumption != nullptr) {
        return extendOrder(order, monRes, assumption);
    } else {
        auto& res = unknownStatesMap[order];
        continueExtending[order] = false;
        return {order, res.first, res.second};
    }
}

template<typename ValueType, typename ConstantType>
std::tuple<std::shared_ptr<Order>, uint_fast64_t, uint_fast64_t> OrderExtender<ValueType, ConstantType>::extendOrder(
    std::shared_ptr<Order> order, std::shared_ptr<MonotonicityResult<VariableType>> monRes, std::shared_ptr<expressions::BinaryRelationExpression> assumption) {
    if (assumption != nullptr) {
        STORM_LOG_INFO("Handling assumption " << *assumption << '\n');
        handleAssumption(order, assumption);
    }

    auto currentStateMode = getNextState(order, numberOfStates, false);
    while (currentStateMode.first != numberOfStates) {
        STORM_LOG_ASSERT(currentStateMode.first < numberOfStates, "State out of range.");
        auto& currentState = currentStateMode.first;
        auto& successors = stateMap[currentState];
        std::pair<uint_fast64_t, uint_fast64_t> result = {numberOfStates, numberOfStates};

        if (successors.size() == 1) {
            STORM_LOG_ASSERT(order->contains(successors[0]), "Order does not contain successor.");
            handleOneSuccessor(order, currentState, successors[0]);
        } else if (!successors.empty()) {
            if (order->isOnlyBottomTopOrder()) {
                order->add(currentState);
                if (!order->isTrivial(currentState)) {
                    // This state is part of an scc, therefore, we could do forward reasoning here
                    result = extendByForwardReasoning(order, currentState, successors, assumption != nullptr);
                } else {
                    result = {numberOfStates, numberOfStates};
                }
            } else {
                result = extendNormal(order, currentState, successors, assumption != nullptr);
            }
        }

        if (result.first == numberOfStates) {
            // We did extend the order
            STORM_LOG_ASSERT(result.second == numberOfStates, "Result second mismatch.");
            STORM_LOG_ASSERT(order->sortStates(&successors).size() == successors.size(), "Sort states size mismatch.");
            STORM_LOG_ASSERT(order->contains(currentState) && order->getNode(currentState) != nullptr, "Current state not in order.");

            if (monRes != nullptr) {
                for (auto& param : occuringVariablesAtState[currentState]) {
                    checkParOnStateMonRes(currentState, order, param, monRes);
                }
            }
            // Get the next state
            currentStateMode = getNextState(order, currentState, true);
        } else {
            STORM_LOG_ASSERT(result.first < numberOfStates, "Result first out of range.");
            STORM_LOG_ASSERT(result.second < numberOfStates, "Result second out of range.");
            STORM_LOG_ASSERT(order->compare(result.first, result.second) == Order::UNKNOWN, "Comparison should be UNKNOWN");
            STORM_LOG_ASSERT(order->compare(result.second, result.first) == Order::UNKNOWN, "Comparison should be UNKNOWN");
            // Try to add states based on min/max and assumptions, only if we are not in statesToHandle mode
            if (currentStateMode.second && extendByAssumption(order, result.first, result.second)) {
                continue;
            }
            // We couldn't extend the order
            if (nonParametricStates.find(currentState) != nonParametricStates.end()) {
                if (!order->contains(currentState)) {
                    // State is not parametric, so we hope that just adding it between =) and =( will help us
                    order->add(currentState);
                }
                currentStateMode = getNextState(order, currentState, true);
                continue;
            } else {
                if (!currentStateMode.second) {
                    // The state was based on statesToHandle, so it is not bad if we cannot continue with this.
                    currentStateMode = getNextState(order, currentState, false);
                    continue;
                } else {
                    // The state was based on the topological sorting, so we need to return, but first add this state to the states Sorted as we are not done
                    // with it
                    order->addStateSorted(currentState);
                    continueExtending[order] = false;
                    return {order, result.first, result.second};
                }
            }
        }
        STORM_LOG_ASSERT(order->sortStates(&successors).size() == successors.size(), "Sort states size mismatch after extension.");
    }

    STORM_LOG_ASSERT(order->getDoneBuilding(), "Order building not done.");
    if (monRes != nullptr) {
        // monotonicity result for the in-build checking of monotonicity
        monRes->setDone();
    }
    return std::make_tuple(order, numberOfStates, numberOfStates);
}

template<typename ValueType, typename ConstantType>
std::pair<uint_fast64_t, uint_fast64_t> OrderExtender<ValueType, ConstantType>::extendNormal(std::shared_ptr<Order> order, uint_fast64_t currentState,
                                                                                             const std::vector<uint_fast64_t>& successors, bool allowMerge) {
    // when it is cyclic and the current state is part of an SCC we do forwardreasoning
    if (cyclic && !order->isTrivial(currentState) && order->contains(currentState)) {
        // Try to extend the order for this scc
        return extendByForwardReasoning(order, currentState, successors, allowMerge);
    } else {
        STORM_LOG_ASSERT(order->isTrivial(currentState) || !order->contains(currentState), "State is neither trivial nor missing.");
        // Do backward reasoning, all successor states must be in the order
        return extendByBackwardReasoning(order, currentState, successors, allowMerge);
    }
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::handleOneSuccessor(std::shared_ptr<Order> order, uint_fast64_t currentState, uint_fast64_t successor) {
    STORM_LOG_ASSERT(order->contains(successor), "Order does not contain successor.");
    if (currentState != successor) {
        if (order->contains(currentState)) {
            order->merge(currentState, successor);
        } else {
            order->addToNode(currentState, order->getNode(successor));
        }
    }
}

template<typename ValueType, typename ConstantType>
std::pair<uint_fast64_t, uint_fast64_t> OrderExtender<ValueType, ConstantType>::extendByBackwardReasoning(std::shared_ptr<Order> order,
                                                                                                          uint_fast64_t currentState,
                                                                                                          std::vector<uint_fast64_t> const& successors,
                                                                                                          bool allowMerge) {
    STORM_LOG_ASSERT(!order->isOnlyBottomTopOrder(), "Order is only bottom-top.");
    STORM_LOG_ASSERT(successors.size() > 1, "Expected multiple successors.");

    bool pla = (usePLA.find(order) != usePLA.end() && usePLA.at(order));
    std::vector<uint_fast64_t> sortedSuccs;

    if (pla && (continueExtending.find(order) == continueExtending.end() || continueExtending.at(order))) {
        for (auto& state1 : successors) {
            if (sortedSuccs.size() == 0) {
                sortedSuccs.push_back(state1);
            } else {
                bool added = false;
                for (auto itr = sortedSuccs.begin(); itr != sortedSuccs.end(); ++itr) {
                    auto& state2 = *itr;
                    auto compareRes = order->compareFast(state1, state2);
                    if (compareRes == Order::NodeComparison::UNKNOWN) {
                        compareRes = addStatesBasedOnMinMax(order, state1, state2);
                    }
                    if (compareRes == Order::NodeComparison::UNKNOWN) {
                        // If fast comparison did not help, we continue by checking "slow" comparison
                        compareRes = order->compare(state1, state2);
                    }
                    if (compareRes == Order::NodeComparison::ABOVE || compareRes == Order::NodeComparison::SAME) {
                        // insert at current pointer (while keeping other values)
                        sortedSuccs.insert(itr, state1);
                        added = true;
                        break;
                    } else if (compareRes == Order::NodeComparison::UNKNOWN) {
                        continueExtending[order] = false;
                        return {state1, state2};
                    }
                }
                if (!added) {
                    sortedSuccs.push_back(state1);
                }
            }
        }
    } else {
        auto temp = order->sortStatesUnorderedPair(&successors);
        if (temp.first.first != numberOfStates) {
            return temp.first;
        } else {
            sortedSuccs = std::move(temp.second);
        }
    }

    if (order->compare(sortedSuccs[0], sortedSuccs[sortedSuccs.size() - 1]) == Order::SAME) {
        if (!order->contains(currentState)) {
            order->addToNode(currentState, order->getNode(sortedSuccs[0]));
        } else {
            order->merge(currentState, sortedSuccs[0]);
        }
    } else {
        if (!order->contains(sortedSuccs[0])) {
            STORM_LOG_ASSERT(order->isBottomState(sortedSuccs[sortedSuccs.size() - 1]), "Expected bottom state.");
            STORM_LOG_ASSERT(sortedSuccs.size() == 2, "Expected 2 successors.");
            order->addAbove(sortedSuccs[0], order->getBottom());
        }
        if (!order->contains(sortedSuccs[sortedSuccs.size() - 1])) {
            STORM_LOG_ASSERT(order->isTopState(sortedSuccs[0]), "Expected top state.");
            STORM_LOG_ASSERT(sortedSuccs.size() == 2, "Expected 2 successors.");
            order->addBelow(sortedSuccs[sortedSuccs.size() - 1], order->getTop());
        }
        // sortedSuccs[0] is highest
        if (!order->contains(currentState)) {
            order->addBetween(currentState, sortedSuccs[0], sortedSuccs[sortedSuccs.size() - 1]);
        } else {
            order->addRelation(sortedSuccs[0], currentState, allowMerge);
            order->addRelation(currentState, sortedSuccs[sortedSuccs.size() - 1], allowMerge);
        }
    }
    STORM_LOG_ASSERT(order->contains(currentState) && order->compare(order->getNode(currentState), order->getBottom()) == Order::ABOVE &&
                         order->compare(order->getNode(currentState), order->getTop()) == Order::BELOW,
                     "Order is not as expected.");
    return {numberOfStates, numberOfStates};
}

template<typename ValueType, typename ConstantType>
std::pair<uint_fast64_t, uint_fast64_t> OrderExtender<ValueType, ConstantType>::extendByForwardReasoning(std::shared_ptr<Order> order,
                                                                                                         uint_fast64_t currentState,
                                                                                                         std::vector<uint_fast64_t> const& successors,
                                                                                                         bool allowMerge) {
    STORM_LOG_ASSERT(successors.size() > 1, "Expected multiple successors.");
    STORM_LOG_ASSERT(order->contains(currentState), "Current state not in order.");
    STORM_LOG_ASSERT(cyclic, "Expected cyclic.");

    std::vector<uint_fast64_t> statesSorted;
    statesSorted.push_back(currentState);
    bool pla = (usePLA.find(order) != usePLA.end() && usePLA.at(order));
    // Go over all states
    bool oneUnknown = false;
    bool unknown = false;
    uint_fast64_t s1 = numberOfStates;
    uint_fast64_t s2 = numberOfStates;
    for (auto& state : successors) {
        unknown = false;
        bool added = false;
        for (auto itr = statesSorted.begin(); itr != statesSorted.end(); ++itr) {
            auto compareRes = order->compareFast(state, (*itr));
            if (pla && compareRes == Order::NodeComparison::UNKNOWN) {
                compareRes = addStatesBasedOnMinMax(order, state, (*itr));
            }
            if (compareRes == Order::NodeComparison::UNKNOWN) {
                compareRes = order->compare(state, *itr);
            }
            if (compareRes == Order::NodeComparison::ABOVE || compareRes == Order::NodeComparison::SAME) {
                if (!order->contains(state) && compareRes == Order::NodeComparison::ABOVE) {
                    order->add(state);
                    order->addStateToHandle(state);
                }
                added = true;
                // insert at current pointer (while keeping other values)
                statesSorted.insert(itr, state);
                break;
            } else if (compareRes == Order::NodeComparison::UNKNOWN && !oneUnknown) {
                // We miss state in the result.
                s1 = state;
                s2 = *itr;
                oneUnknown = true;
                added = true;
                break;
            } else if (compareRes == Order::NodeComparison::UNKNOWN && oneUnknown) {
                unknown = true;
                added = true;
                break;
            }
        }
        if (!(unknown && oneUnknown) && !added) {
            // State will be last in the list
            statesSorted.push_back(state);
        }
        if (unknown && oneUnknown) {
            break;
        }
    }
    if (!unknown && oneUnknown) {
        STORM_LOG_ASSERT(statesSorted.size() == successors.size(), "States sorted size mismatch.");
        s2 = numberOfStates;
    }

    if (s1 == numberOfStates) {
        STORM_LOG_ASSERT(statesSorted.size() == successors.size() + 1, "States sorted size mismatch.");
        // all could be sorted, no need to do anything
    } else if (s2 == numberOfStates) {
        if (!order->contains(s1)) {
            order->add(s1);
        }

        if (statesSorted[0] == currentState) {
            order->addRelation(s1, statesSorted[0], allowMerge);
            STORM_LOG_ASSERT((order->compare(s1, statesSorted[0]) == Order::ABOVE) ||
                                 (allowMerge && (order->compare(s1, statesSorted[statesSorted.size() - 1]) == Order::SAME)),
                             "Order is not as expected.");
            order->addRelation(s1, statesSorted[statesSorted.size() - 1], allowMerge);
            STORM_LOG_ASSERT((order->compare(s1, statesSorted[statesSorted.size() - 1]) == Order::ABOVE) ||
                                 (allowMerge && (order->compare(s1, statesSorted[statesSorted.size() - 1]) == Order::SAME)),
                             "Order is not as expected.");
            order->addStateToHandle(s1);
        } else if (statesSorted[statesSorted.size() - 1] == currentState) {
            order->addRelation(statesSorted[0], s1, allowMerge);
            STORM_LOG_ASSERT((order->compare(s1, statesSorted[0]) == Order::BELOW) ||
                                 (allowMerge && (order->compare(s1, statesSorted[statesSorted.size() - 1]) == Order::SAME)),
                             "Order is not as expected.");
            order->addRelation(statesSorted[statesSorted.size() - 1], s1, allowMerge);
            STORM_LOG_ASSERT((order->compare(s1, statesSorted[statesSorted.size() - 1]) == Order::BELOW) ||
                                 (allowMerge && (order->compare(s1, statesSorted[statesSorted.size() - 1]) == Order::SAME)),
                             "Order is not as expected.");
            order->addStateToHandle(s1);
        } else {
            bool continueSearch = true;
            for (auto& entry : matrix.getRow(currentState)) {
                if (entry.getColumn() == s1) {
                    if (entry.getValue().isConstant()) {
                        continueSearch = false;
                    }
                }
            }
            if (continueSearch) {
                for (auto& i : statesSorted) {
                    if (order->compare(i, s1) == Order::UNKNOWN) {
                        return {i, s1};
                    }
                }
            }
        }
    } else {
        return {s1, s2};
    }
    STORM_LOG_ASSERT(order->contains(currentState) && order->compare(order->getNode(currentState), order->getBottom()) == Order::ABOVE &&
                         order->compare(order->getNode(currentState), order->getTop()) == Order::BELOW,
                     "Order is not as expected.");
    return {numberOfStates, numberOfStates};
}

template<typename ValueType, typename ConstantType>
bool OrderExtender<ValueType, ConstantType>::extendByAssumption(std::shared_ptr<Order> order, uint_fast64_t state1, uint_fast64_t state2) {
    bool usePLANow = usePLA.find(order) != usePLA.end() && usePLA[order];
    STORM_LOG_ASSERT(order->compare(state1, state2) == Order::UNKNOWN, "Comparison should be UNKNOWN");
    auto assumptions = usePLANow ? assumptionMaker->createAndCheckAssumptions(state1, state2, order, region, minValues[order], maxValues[order])
                                 : assumptionMaker->createAndCheckAssumptions(state1, state2, order, region);
    if (assumptions.size() == 1 && assumptions.begin()->second == AssumptionStatus::VALID) {
        handleAssumption(order, assumptions.begin()->first);
        // Assumptions worked, we continue
        return true;
    }
    return false;
}

template<typename ValueType, typename ConstantType>
Order::NodeComparison OrderExtender<ValueType, ConstantType>::addStatesBasedOnMinMax(std::shared_ptr<Order> order, uint_fast64_t state1,
                                                                                     uint_fast64_t state2) const {
    STORM_LOG_ASSERT(order->compareFast(state1, state2) == Order::UNKNOWN, "Fast comparison should be UNKNOWN");
    STORM_LOG_ASSERT(minValues.find(order) != minValues.end(), "MinValues missing for order.");
    std::vector<ConstantType> const& mins = minValues.at(order);
    std::vector<ConstantType> const& maxs = maxValues.at(order);
    if (mins[state1] == maxs[state1] && mins[state2] == maxs[state2] && mins[state1] == mins[state2]) {
        if (order->contains(state1)) {
            if (order->contains(state2)) {
                order->merge(state1, state2);
                STORM_LOG_ASSERT(!order->isInvalid(), "Order is invalid after merge.");
            } else {
                order->addToNode(state2, order->getNode(state1));
            }
        }
        return Order::SAME;
    } else if (mins[state1] > maxs[state2]) {
        // state 1 will always be larger than state2
        if (!order->contains(state1)) {
            order->add(state1);
        }
        if (!order->contains(state2)) {
            order->add(state2);
        }
        STORM_LOG_ASSERT(order->compare(state1, state2) != Order::BELOW, "State1 should not be below state2");
        STORM_LOG_ASSERT(order->compare(state1, state2) != Order::SAME, "State1 should not be same as state2");
        order->addRelation(state1, state2);

        return Order::ABOVE;
    } else if (mins[state2] > maxs[state1]) {
        // state2 will always be larger than state1
        if (!order->contains(state1)) {
            order->add(state1);
        }
        if (!order->contains(state2)) {
            order->add(state2);
        }
        STORM_LOG_ASSERT(order->compare(state2, state1) != Order::BELOW, "State2 should not be below state1");
        STORM_LOG_ASSERT(order->compare(state2, state1) != Order::SAME, "State2 should not be same as state1");
        order->addRelation(state2, state1);
        return Order::BELOW;
    } else {
        // Couldn't add relation between state1 and state 2 based on min/max values;
        return Order::UNKNOWN;
    }
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::initializeMinMaxValues(storage::ParameterRegion<ValueType> region) {
    if (model != nullptr) {
        // Use parameter lifting modelchecker to get initial min/max values for order creation
        modelchecker::SparseDtmcParameterLiftingModelChecker<models::sparse::Dtmc<ValueType>, ConstantType> plaModelChecker;
        std::unique_ptr<modelchecker::CheckResult> checkResult;
        auto env = Environment();
        boost::optional<modelchecker::CheckTask<logic::Formula, ValueType>> checkTask;
        if (this->formula->hasQuantitativeResult()) {
            checkTask = modelchecker::CheckTask<logic::Formula, ValueType>(*formula);
        } else {
            storm::logic::OperatorInformation opInfo(boost::none, boost::none);
            auto newFormula =
                std::make_shared<storm::logic::ProbabilityOperatorFormula>(formula->asProbabilityOperatorFormula().getSubformula().asSharedPointer(), opInfo);
            checkTask = modelchecker::CheckTask<logic::Formula, ValueType>(*newFormula);
        }
        STORM_LOG_THROW(plaModelChecker.canHandle(model, checkTask.get()), exceptions::NotSupportedException, "Cannot handle this formula.");
        bool const allowModelSimplification = false;  // make sure that the results align with the input model
        plaModelChecker.specify(env, model, checkTask.get(), std::nullopt, nullptr, allowModelSimplification);

        storm::modelchecker::AnnotatedRegion<ValueType> annotatedRegion{region};
        modelchecker::ExplicitQuantitativeCheckResult<ConstantType> minCheck =
            plaModelChecker.check(env, annotatedRegion, solver::OptimizationDirection::Minimize)->template asExplicitQuantitativeCheckResult<ConstantType>();
        modelchecker::ExplicitQuantitativeCheckResult<ConstantType> maxCheck =
            plaModelChecker.check(env, annotatedRegion, solver::OptimizationDirection::Maximize)->template asExplicitQuantitativeCheckResult<ConstantType>();
        minValuesInit = minCheck.getValueVector();
        maxValuesInit = maxCheck.getValueVector();
        STORM_LOG_ASSERT(minValuesInit->size() == numberOfStates, "MinValuesInit size mismatch.");
        STORM_LOG_ASSERT(maxValuesInit->size() == numberOfStates, "MaxValuesInit size mismatch.");
    }
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::setMinMaxValues(std::shared_ptr<Order> order, std::vector<ConstantType>&& minValues,
                                                             std::vector<ConstantType>&& maxValues) {
    STORM_LOG_ASSERT(minValues.size() == numberOfStates, "MinValues size mismatch.");
    STORM_LOG_ASSERT(maxValues.size() == numberOfStates, "MaxValues size mismatch.");
    usePLA[order] = true;
    if (unknownStatesMap.find(order) != unknownStatesMap.end()) {
        auto& unknownStates = unknownStatesMap[order];
        if (unknownStates.first != numberOfStates) {
            continueExtending[order] =
                minValues[unknownStates.first] >= maxValues[unknownStates.second] || minValues[unknownStates.second] >= maxValues[unknownStates.first];
        } else {
            continueExtending[order] = true;
        }
    } else {
        continueExtending[order] = true;
    }
    this->minValues[order] = std::move(minValues);
    this->maxValues[order] = std::move(maxValues);
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::setMinValues(std::shared_ptr<Order> order, std::vector<ConstantType>&& minValues) {
    STORM_LOG_ASSERT(minValues.size() == numberOfStates, "MinValues size mismatch.");
    auto& maxValues = this->maxValues[order];
    usePLA[order] = this->maxValues.find(order) != this->maxValues.end();
    if (maxValues.size() == 0) {
        continueExtending[order] = false;
    } else if (unknownStatesMap.find(order) != unknownStatesMap.end()) {
        auto& unknownStates = unknownStatesMap[order];
        if (unknownStates.first != numberOfStates) {
            continueExtending[order] =
                minValues[unknownStates.first] >= maxValues[unknownStates.second] || minValues[unknownStates.second] >= maxValues[unknownStates.first];
        } else {
            continueExtending[order] = true;
        }
    } else {
        continueExtending[order] = true;
    }
    this->minValues[order] = std::move(minValues);
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::setMaxValues(std::shared_ptr<Order> order, std::vector<ConstantType>&& maxValues) {
    STORM_LOG_ASSERT(maxValues.size() == numberOfStates, "MaxValues size mismatch.");
    usePLA[order] = this->minValues.find(order) != this->minValues.end();
    auto& minValues = this->minValues[order];
    if (minValues.size() == 0) {
        continueExtending[order] = false;
    } else if (unknownStatesMap.find(order) != unknownStatesMap.end()) {
        auto& unknownStates = unknownStatesMap[order];
        if (unknownStates.first != numberOfStates) {
            continueExtending[order] =
                minValues[unknownStates.first] >= maxValues[unknownStates.second] || minValues[unknownStates.second] >= maxValues[unknownStates.first];
        } else {
            continueExtending[order] = true;
        }
    } else {
        continueExtending[order] = true;
    }
    this->maxValues[order] = std::move(maxValues);  // maxCheck->asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
}
template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::setMinValuesInit(std::vector<ConstantType>&& minValues) {
    STORM_LOG_ASSERT(minValues.size() == numberOfStates, "MinValues size mismatch.");
    this->minValuesInit = std::move(minValues);
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::setMaxValuesInit(std::vector<ConstantType>&& maxValues) {
    STORM_LOG_ASSERT(maxValues.size() == numberOfStates, "MaxValues size mismatch.");
    this->maxValuesInit = std::move(maxValues);  // maxCheck->asExplicitQuantitativeCheckResult<ConstantType>().getValueVector();
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::checkParOnStateMonRes(uint_fast64_t s, std::shared_ptr<Order> order,
                                                                   typename OrderExtender<ValueType, ConstantType>::VariableType param,
                                                                   std::shared_ptr<MonotonicityResult<VariableType>> monResult) {
    auto mon = monotonicityChecker.checkLocalMonotonicity(order, s, param, region);
    monResult->updateMonotonicityResult(param, mon);
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::setUnknownStates(std::shared_ptr<Order> order, uint_fast64_t state1, uint_fast64_t state2) {
    STORM_LOG_ASSERT(state1 != numberOfStates && state2 != numberOfStates, "States should not be numberOfStates.");
    unknownStatesMap[order] = {state1, state2};
}

template<typename ValueType, typename ConstantType>
std::pair<uint_fast64_t, uint_fast64_t> OrderExtender<ValueType, ConstantType>::getUnknownStates(std::shared_ptr<Order> order) const {
    if (unknownStatesMap.find(order) != unknownStatesMap.end()) {
        return unknownStatesMap.at(order);
    }
    return {numberOfStates, numberOfStates};
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::setUnknownStates(std::shared_ptr<Order> orderOriginal, std::shared_ptr<Order> orderCopy) {
    STORM_LOG_ASSERT(unknownStatesMap.find(orderCopy) == unknownStatesMap.end(), "OrderCopy already in unknownStatesMap.");
    unknownStatesMap.insert({orderCopy, {unknownStatesMap[orderOriginal].first, unknownStatesMap[orderOriginal].second}});
}

template<typename ValueType, typename ConstantType>
void OrderExtender<ValueType, ConstantType>::copyMinMax(std::shared_ptr<Order> orderOriginal, std::shared_ptr<Order> orderCopy) {
    usePLA[orderCopy] = usePLA[orderOriginal];
    if (usePLA[orderCopy]) {
        minValues[orderCopy] = minValues[orderOriginal];
        STORM_LOG_ASSERT(maxValues.find(orderOriginal) != maxValues.end(), "MaxValues missing for orderOriginal.");
        maxValues[orderCopy] = maxValues[orderOriginal];
    }
    continueExtending[orderCopy] = continueExtending[orderOriginal];
}

template<typename ValueType, typename ConstantType>
std::pair<uint_fast64_t, bool> OrderExtender<ValueType, ConstantType>::getNextState(std::shared_ptr<Order> order, uint_fast64_t currentState, bool done) {
    if (done && currentState != numberOfStates) {
        order->setDoneState(currentState);
    }
    if (cyclic && order->existsStateToHandle()) {
        return order->getStateToHandle();
    }
    if (currentState == numberOfStates) {
        return order->getNextStateNumber();
    }
    if (currentState != numberOfStates) {
        return order->getNextStateNumber();
    }
    return {numberOfStates, true};
}

template<typename ValueType, typename ConstantType>
bool OrderExtender<ValueType, ConstantType>::isHope(std::shared_ptr<Order> order) {
    STORM_LOG_ASSERT(unknownStatesMap.find(order) != unknownStatesMap.end(), "Order not in unknownStatesMap.");
    STORM_LOG_ASSERT(!order->getDoneBuilding(), "Order building unexpectedly done.");
    // First check if bounds helped us
    bool yesThereIsHope = continueExtending[order];
    return yesThereIsHope;
}
template<typename ValueType, typename ConstantType>
MonotonicityChecker<ValueType>& OrderExtender<ValueType, ConstantType>::getMonotoncityChecker() {
    return monotonicityChecker;
}
template<typename ValueType, typename ConstantType>
const std::vector<std::set<typename OrderExtender<ValueType, ConstantType>::VariableType>>&
OrderExtender<ValueType, ConstantType>::getVariablesOccuringAtState() {
    return occuringVariablesAtState;
}

template class OrderExtender<RationalFunction, double>;
template class OrderExtender<RationalFunction, RationalNumber>;
}  // namespace analysis
}  // namespace storm
