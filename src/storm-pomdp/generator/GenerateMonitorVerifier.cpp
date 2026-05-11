#include "storm-pomdp/generator/GenerateMonitorVerifier.h"
#include <sys/types.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace generator {

template<typename ValueType>
MonitorVerifier<ValueType>::MonitorVerifier(const models::sparse::Pomdp<ValueType>& product,
                                            const std::map<std::pair<uint32_t, bool>, uint32_t>& observationMap,
                                            std::map<uint32_t, std::string> observationDefaultAction)
    : product(storm::models::sparse::Pomdp<ValueType>(product)), observationMap(observationMap), observationDefaultAction(observationDefaultAction) {}

template<typename ValueType>
const std::map<std::pair<uint32_t, bool>, uint32_t>& MonitorVerifier<ValueType>::getObservationMap() {
    return observationMap;
}

template<typename ValueType>
const models::sparse::Pomdp<ValueType>& MonitorVerifier<ValueType>::getProduct() {
    return product;
}

template<typename ValueType>
const std::map<uint32_t, std::string>& MonitorVerifier<ValueType>::getObservationDefaultAction() {
    return observationDefaultAction;
}

template<typename ValueType>
GenerateMonitorVerifier<ValueType>::GenerateMonitorVerifier(models::sparse::Dtmc<ValueType> const& mc, models::sparse::Mdp<ValueType> const& monitor,
                                                            std::shared_ptr<storm::expressions::ExpressionManager>& exprManager, Options const& options)
    : mc(mc), monitor(monitor), risk(), exprManager(exprManager), options(options) {
    monvar = exprManager->declareFreshIntegerVariable(false, "_mon");
    mcvar = exprManager->declareFreshIntegerVariable(false, "_mc");
}

template<typename ValueType>
std::shared_ptr<MonitorVerifier<ValueType>> GenerateMonitorVerifier<ValueType>::createProduct() {
    typedef storm::storage::sparse::state_type state_type;
    typedef std::pair<state_type, state_type> product_state_type;

    STORM_LOG_THROW(monitor.hasChoiceLabeling(), storm::exceptions::InvalidArgumentException, "The monitor should contain choice labeling");

    const std::set<std::string>& actions = monitor.getChoiceLabeling().getLabels();

    // Build choice label map of monitor choices
    std::vector<std::string> monitorChoiceLabels;
    for (typename storm::storage::SparseMatrix<ValueType>::index_type i = 0; i < monitor.getTransitionMatrix().getRowCount(); i++) {
        auto const& monitorLabels = monitor.getChoiceLabeling().getLabelsOfChoice(i);
        STORM_LOG_THROW(monitorLabels.size() == 1, storm::exceptions::InvalidArgumentException, "Monitor choice has not exactly one choice label");
        monitorChoiceLabels.push_back(*monitorLabels.begin());
    }

    uint32_t nextObservation = 0;
    std::map<std::pair<uint32_t, bool>, uint32_t> observationMap;
    std::vector<uint32_t> observations;

    std::map<std::pair<const std::string, uint32_t>, storage::BitVector> rowActionObservationMap;
    std::vector<std::set<std::string>> observationUsedActions;

    storm::storage::SparseMatrixBuilder<ValueType> builder(0, 0, 0, false, true);
    std::size_t currentRow = 0;
    state_type nextStateId = 0;

    state_type goalIndex = nextStateId++;
    builder.newRowGroup(currentRow);
    rowActionObservationMap[std::make_pair("end", nextObservation)].grow(currentRow + 1);
    rowActionObservationMap[std::make_pair("end", nextObservation)].set(currentRow);
    observationUsedActions.push_back({"end"});
    builder.addDiagonalEntry(currentRow++, utility::one<ValueType>());
    observations.push_back(nextObservation++);

    state_type stopIndex = nextStateId++;
    builder.newRowGroup(currentRow);
    rowActionObservationMap[std::make_pair("end", nextObservation)].grow(currentRow + 1);
    rowActionObservationMap[std::make_pair("end", nextObservation)].set(currentRow);
    observationUsedActions.push_back({"end"});
    builder.addDiagonalEntry(currentRow++, utility::one<ValueType>());
    observations.push_back(nextObservation++);

    std::map<product_state_type, state_type> prodToIndexMap;
    std::vector<state_type> rejectToStates;

    state_type rejectionIndex;
    if (!options.useRestartSemantics) {
        // Add sink state where all invalid transitions go
        rejectionIndex = nextStateId++;
        builder.newRowGroup(currentRow);
        rowActionObservationMap[std::make_pair("end", nextObservation)].grow(currentRow + 1);
        rowActionObservationMap[std::make_pair("end", nextObservation)].set(currentRow);
        observationUsedActions.push_back({"end"});
        builder.addDiagonalEntry(currentRow++, utility::one<ValueType>());
        observations.push_back(nextObservation++);
        rejectToStates.push_back(rejectionIndex);
    }

    std::vector<state_type> initialStates;

    std::deque<product_state_type> todo;
    for (state_type mc_s_0 : mc.getInitialStates()) {
        for (state_type mon_s_0 : monitor.getInitialStates()) {
            product_state_type prod_s(mc_s_0, mon_s_0);
            state_type index = nextStateId++;
            prodToIndexMap[prod_s] = index;
            initialStates.push_back(index);
            if (options.useRestartSemantics)
                rejectToStates.push_back(index);
            todo.push_back(prod_s);
        }
    }

    while (!todo.empty()) {
        auto const [mc_from, mon_from] = std::move(todo.front());
        todo.pop_front();

        // Set observations for from
        bool accepting = monitor.getStateLabeling().getStateHasLabel(options.acceptingLabel, mon_from);
        uint32_t step;
        for (auto& label : monitor.getStateLabeling().getLabelsOfState(mon_from)) {
            if (label.starts_with(options.stepPrefix)) {
                step = std::stoi(label.substr(options.stepPrefix.length()));
            }
        }
        std::pair obsPair(step, accepting);
        if (!observationMap.contains(obsPair)) {
            observationMap[obsPair] = nextObservation++;
            observationUsedActions.push_back(std::set<std::string>());
        }
        uint32_t currentObservation = observationMap.at(obsPair);
        observations.push_back(currentObservation);

        // Set transitions for from and add new states to todo
        builder.newRowGroup(currentRow);
        if (monitor.getStateLabeling().getLabelsOfState(mon_from).contains(options.horizonLabel)) {
            const auto& action = *actions.begin();
            for (state_type initState : rejectToStates) {
                builder.addNextValue(currentRow, initState, storm::utility::one<ValueType>() / rejectToStates.size());
            }
            rowActionObservationMap[std::make_pair(action, currentObservation)].grow(currentRow + 1);
            rowActionObservationMap[std::make_pair(action, currentObservation)].set(currentRow);
            observationUsedActions[currentObservation].emplace(action);
            currentRow++;
        } else {
            std::size_t numMonRows = monitor.getTransitionMatrix().getRowGroupSize(mon_from);
            std::size_t monGroupStart = monitor.getTransitionMatrix().getRowGroupIndices()[mon_from];
            std::set<std::string> actionsNotTaken(actions);
            for (std::size_t i = 0; i < numMonRows; i++) {
                // Remove labels of monitor choice from the labels we still have to take

                const auto action = monitorChoiceLabels[monGroupStart + i];
                actionsNotTaken.erase(action);

                const auto& monitorRow = monitor.getTransitionMatrix().getRow(mon_from, i);
                STORM_LOG_ASSERT(monitorRow.getNumberOfEntries() == 1, "Monitor is not fully deterministic");
                const auto& monitorEntry = monitorRow.begin();

                const auto& mcRow = mc.getTransitionMatrix().getRow(mc_from);

                // Find total probability of the transitions to a state with label action
                auto totalProbability = utility::zero<ValueType>();
                for (const auto& mcEntry : mcRow) {
                    if (mc.getStateLabeling().getStateHasLabel(action, mcEntry.getColumn())) {
                        totalProbability += mcEntry.getValue();
                    }
                }

                // Add new entries to an unsorted vector containing possible duplicate indexes
                std::map<state_type, ValueType> newRow;

                // Direct probability not used towards the initial states
                if (totalProbability < storm::utility::one<ValueType>()) {
                    for (state_type initState : rejectToStates) {
                        if (newRow.contains(initState))
                            newRow[initState] = newRow[initState] + (1 - totalProbability) / rejectToStates.size();
                        else
                            newRow[initState] = (1 - totalProbability) / rejectToStates.size();
                    }
                }

                // Add transitions to the successors, if the successor has not yet been added, add it to the todo list
                if (totalProbability > storm::utility::zero<ValueType>()) {
                    for (const auto& mcEntry : mcRow) {
                        if (mc.getStateLabeling().getStateHasLabel(action, mcEntry.getColumn())) {
                            const product_state_type to_pair(mcEntry.getColumn(), monitorEntry->getColumn());
                            state_type indexTo;
                            if (auto it = prodToIndexMap.find(to_pair); it != prodToIndexMap.end()) {
                                indexTo = it->second;
                            } else {
                                indexTo = nextStateId++;
                                todo.push_back(to_pair);
                                prodToIndexMap[to_pair] = indexTo;
                            }
                            if (newRow.contains(indexTo))
                                newRow[indexTo] = newRow[indexTo] + mcEntry.getValue();
                            else
                                newRow[indexTo] = mcEntry.getValue();
                        }
                    }

                    // Set action to used for this observation
                    observationUsedActions[currentObservation].emplace(action);
                }

                // Insert new entries
                for (const auto& entry : newRow) {
                    builder.addNextValue(currentRow, entry.first, entry.second);
                }
                auto& rowBitVec = rowActionObservationMap[std::make_pair(action, currentObservation)];
                rowBitVec.grow(currentRow + 1);
                rowBitVec.set(currentRow);
                currentRow++;
            }

            for (const auto& action : actionsNotTaken) {
                for (state_type initState : rejectToStates) {
                    builder.addNextValue(currentRow, initState, storm::utility::one<ValueType>() / rejectToStates.size());
                }
                auto& rowBitVec = rowActionObservationMap[std::make_pair(action, currentObservation)];
                rowBitVec.grow(currentRow + 1);
                rowBitVec.set(currentRow);
                currentRow++;
            }
        }

        if (monitor.getStateLabeling().getStateHasLabel(options.acceptingLabel, mon_from)) {
            STORM_LOG_THROW(risk[mc_from] >= -utility::convertNumber<ValueType>(1e-12) && risk[mc_from] <= utility::convertNumber<ValueType>(1.0 + 1e-12),
                            exceptions::IllegalArgumentException, "Risk for state " + std::to_string(mc_from) + " is not in [0, 1]");
            if (utility::isAlmostZero(risk[mc_from])) {
                builder.addNextValue(currentRow, stopIndex, utility::one<ValueType>());
            } else if (utility::isAlmostOne(risk[mc_from])) {
                builder.addNextValue(currentRow, goalIndex, utility::one<ValueType>());
            } else {
                builder.addNextValue(currentRow, goalIndex, risk[mc_from]);
                builder.addNextValue(currentRow, stopIndex, utility::one<ValueType>() - risk[mc_from]);
            }
            observationUsedActions[currentObservation].emplace("end");
            auto& rowBitVec = rowActionObservationMap[std::make_pair("end", currentObservation)];
            rowBitVec.grow(currentRow + 1);
            rowBitVec.set(currentRow);
            currentRow++;
        }
    }

    size_t numberOfRows = currentRow;

    // Make all observation action bitvectors of size numberOfRows
    for (auto& [labelObsPair, vec] : rowActionObservationMap) {
        vec.resize(numberOfRows);
    }

    // Calculate which rows belong to action which don't all return for an observation and only keep these
    storm::storage::SparseMatrix<ValueType> transMatrix = builder.build();
    storm::storage::BitVector rowsToKeep(transMatrix.getRowCount());
    std::map<uint32_t, std::string> observationDefaultAction;
    u_int32_t currentObservation = 0;
    for (auto const& actionsInObs : observationUsedActions) {
        if (actionsInObs.size() == 1) {
            observationDefaultAction[currentObservation] = *actionsInObs.begin();
        }

        for (auto const& action : actionsInObs) {
            // std::cout << "Keeping action obs (" << action << ", " << currentObservation << ")" << std::endl;
            rowsToKeep |= rowActionObservationMap[std::make_pair(action, currentObservation)];
        }
        currentObservation++;
    }
    // std::cout << "Kept " << rowsToKeep.getNumberOfSetBits() << " out of " << numberOfRows << " rows." << std::endl;
    // rowsToKeep.setMultiple(0, numberOfRows);
    numberOfRows = rowsToKeep.getNumberOfSetBits();
    storm::storage::SparseMatrix<ValueType> reducedTransitionMatrix = transMatrix.restrictRows(rowsToKeep);

    // Create state labeling
    const state_type numberOfStates = nextStateId;
    storm::models::sparse::StateLabeling stateLabeling(numberOfStates);
    stateLabeling.addLabel("init", storm::storage::BitVector(numberOfStates, initialStates.begin(), initialStates.end()));

    stateLabeling.addLabel("goal", storm::storage::BitVector(numberOfStates));
    stateLabeling.addLabelToState("goal", goalIndex);

    stateLabeling.addLabel("stop", storm::storage::BitVector(numberOfStates));
    stateLabeling.addLabelToState("stop", stopIndex);

    stateLabeling.addLabel("condition", storm::storage::BitVector(numberOfStates));
    stateLabeling.addLabelToState("condition", goalIndex);
    stateLabeling.addLabelToState("condition", stopIndex);

    if (!options.useRestartSemantics) {
        stateLabeling.addLabel("sink", storm::storage::BitVector(numberOfStates));
        stateLabeling.addLabelToState("sink", rejectionIndex);
    }

    storm::storage::sparse::ModelComponents<ValueType> components(reducedTransitionMatrix, std::move(stateLabeling));
    components.observabilityClasses = std::move(observations);

    // Add choice labeling
    const std::vector<uint64_t> rowMapping = rowsToKeep.getNumberOfSetBitsBeforeIndices();  // Vector which maps old row id to new row id
    storm::models::sparse::ChoiceLabeling choiceLabeling(numberOfRows);
    for (const auto& [labelObsPair, bitvec] : rowActionObservationMap) {
        // Rebuild bitvec with restricted rows
        storm::storage::BitVector newBitVec(numberOfRows);
        for (const auto& setbit : bitvec) {
            if (rowsToKeep[setbit])
                newBitVec.set(rowMapping[setbit]);
        }
        // auto newBitVec = bitvec;

        if (choiceLabeling.containsLabel(labelObsPair.first)) {
            choiceLabeling.setChoices(labelObsPair.first, newBitVec | choiceLabeling.getChoices(labelObsPair.first));
        } else {
            choiceLabeling.addLabel(labelObsPair.first, newBitVec);
        }
    }

    components.choiceLabeling = std::move(choiceLabeling);

    if (mc.hasStateValuations()) {
        // Add state valuations
        storm::storage::sparse::StateValuationsBuilder svBuilder;
        svBuilder.addVariable(monvar);
        svBuilder.addVariable(mcvar);
        std::set<expressions::Variable> variables;
        for (uint64_t i = 0; i < mc.getNumberOfStates(); i++) {
            const auto& valAssignment = mc.getStateValuations().at(i);
            for (auto val = valAssignment.begin(); val != valAssignment.end(); ++val) {
                if (val.isVariableAssignment() && !variables.contains(val.getVariable())) {
                    variables.emplace(val.getVariable());
                    svBuilder.addVariable(val.getVariable());
                }
            }
        }

        for (uint64_t i = 0; i < mc.getNumberOfStates(); i++) {
            for (uint64_t j = 0; j < monitor.getNumberOfStates(); j++) {
                product_state_type s(i, j);
                if (!prodToIndexMap.contains(s))
                    continue;

                std::vector<bool> booleanValues;
                std::vector<int64_t> integerValues;
                std::vector<storm::RationalNumber> rationalValues;

                integerValues.push_back(j);  // Set monvar
                integerValues.push_back(i);  // Set mcvar

                const auto& valAssignment = mc.getStateValuations().at(i);

                for (auto& var : variables) {
                    for (auto val = valAssignment.begin(); val != valAssignment.end(); ++val) {
                        if (var == val.getVariable()) {
                            if (val.isBoolean()) {
                                booleanValues.push_back(val.getBooleanValue());
                            } else if (val.isInteger()) {
                                integerValues.push_back(val.getIntegerValue());
                            } else if (val.isRational()) {
                                rationalValues.push_back(val.getRationalValue());
                            }
                            break;
                        }
                    }
                }
                svBuilder.addState(prodToIndexMap[std::make_pair(i, j)], std::move(booleanValues), std::move(integerValues), std::move(rationalValues));
            }
        }

        std::vector<bool> goalBooleanValues;
        std::vector<int64_t> goalIntegerValues(2, -1);
        std::vector<storm::RationalNumber> goalRationalValues;
        for (auto& var : variables) {
            if (var.hasBooleanType()) {
                goalBooleanValues.push_back(false);
            } else if (var.hasIntegerType()) {
                goalIntegerValues.push_back(-1);
            } else if (var.hasRationalType()) {
                goalRationalValues.emplace_back(-1);
            }
        }
        svBuilder.addState(goalIndex, std::move(goalBooleanValues), std::move(goalIntegerValues), std::move(goalRationalValues));

        std::vector<bool> stopBooleanValues;
        std::vector<int64_t> stopIntegerValues(2, -1);
        std::vector<storm::RationalNumber> stopRationalValues;
        for (auto& var : variables) {
            if (var.hasBooleanType()) {
                stopBooleanValues.push_back(false);
            } else if (var.hasIntegerType()) {
                stopIntegerValues.push_back(-1);
            } else if (var.hasRationalType()) {
                stopRationalValues.emplace_back(-1);
            }
        }
        svBuilder.addState(stopIndex, std::move(stopBooleanValues), std::move(stopIntegerValues), std::move(stopRationalValues));

        components.stateValuations = svBuilder.build();
    }

    // Store model
    storm::models::sparse::Pomdp<ValueType> product(std::move(components));
    auto mv = std::make_shared<MonitorVerifier<ValueType>>(std::move(product), std::move(observationMap), std::move(observationDefaultAction));
    return mv;
}

template<typename ValueType>
void GenerateMonitorVerifier<ValueType>::setRisk(std::vector<ValueType> const& risk) {
    this->risk = risk;
}

template class MonitorVerifier<double>;
template class MonitorVerifier<storm::RationalNumber>;
template class GenerateMonitorVerifier<double>;
template class GenerateMonitorVerifier<storm::RationalNumber>;

}  // namespace generator
}  // namespace storm
