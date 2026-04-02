#include "storm-pomdp/transformer/ObservationTraceUnfolder.h"
#include <algorithm>

#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/ConstantsComparator.h"

#undef _VERBOSE_OBSERVATION_UNFOLDING

namespace storm {
namespace pomdp {
template<typename ValueType>
ObservationTraceUnfolder<ValueType>::ObservationTraceUnfolder(storm::models::sparse::Pomdp<ValueType> const& model, std::vector<ValueType> const& risk,
                                                              std::shared_ptr<storm::expressions::ExpressionManager>& exprManager,
                                                              ObservationTraceUnfolderOptions const& options)
    : model(model), risk(risk), exprManager(exprManager), options(options) {
    // std::cout << "Starting observation trace unfolder with n obervations " << model.getNrObservations() << "\n";
    // statesPerObservation = std::vector<storm::storage::BitVector>(model.getNrObservations() + 1, storm::storage::BitVector(model.getNumberOfStates()));
    // for (uint64_t state = 0; state < model.getNumberOfStates(); ++state) {
    //     statesPerObservation[model.getObservation(state)].set(state, true);
    // }
    svvar = exprManager->declareFreshIntegerVariable(false, "_s");
    tsvar = exprManager->declareFreshIntegerVariable(false, "_t");
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Mdp<ValueType>> ObservationTraceUnfolder<ValueType>::transform(const std::vector<uint32_t>& observations) {
    std::vector<uint32_t> modifiedObservations = observations;
    // First observation should be special.
    // This just makes the algorithm simpler because we do not treat the first step as a special case later.
    // We overwrite the observation with a non-existing obs z*
    modifiedObservations[0] = model.getNrObservations();

    storm::storage::BitVector initialStates = model.getInitialStates();
    storm::storage::BitVector actualInitialStates = initialStates;
    for (uint64_t state : initialStates) {
        if (model.getObservation(state) != observations[0]) {
            actualInitialStates.set(state, false);
        }
    }
    STORM_LOG_THROW(actualInitialStates.getNumberOfSetBits() == 1, storm::exceptions::InvalidArgumentException,
                    "Must have unique initial state matching the observation");
    // For this z* that only exists in the initial state, we now also define the states for this observation.
    // statesPerObservation[model.getNrObservations()] = actualInitialStates;

#ifdef _VERBOSE_OBSERVATION_UNFOLDING
    std::cout << "build valution builder..\n";
#endif
    storm::storage::sparse::StateValuationsBuilder svbuilder;
    svbuilder.addVariable(svvar);
    svbuilder.addVariable(tsvar);

    std::unordered_map<uint64_t, uint64_t> unfoldedToOld;
    std::unordered_map<uint64_t, uint64_t> unfoldedToOldNextStep;
    std::unordered_map<uint64_t, uint64_t> oldToUnfolded;

#ifdef _VERBOSE_OBSERVATION_UNFOLDING
    std::cout << "start buildiing matrix...\n";
#endif

    uint64_t newStateIndex = 0;
    uint64_t const violatedState = newStateIndex;
    if (!options.rejectionSampling) {
        // The violated state is only used if we do no use the rejection semantics.
        ++newStateIndex;
    }
    // Add this initial state:
    uint64_t const initialState = newStateIndex;
    ++newStateIndex;

    unfoldedToOldNextStep[initialState] = actualInitialStates.getNextSetIndex(0);

    uint64_t const resetDestination = options.rejectionSampling ? initialState : violatedState;  // Should be initial state for the standard semantics.
    storm::storage::SparseMatrixBuilder<ValueType> transitionMatrixBuilder(0, 0, 0, true, true);

    // TODO only add this state if it is actually reachable / rejection sampling
    if (!options.rejectionSampling) {
        // the violated state (only used when no rejection sampling) is a sink state
        transitionMatrixBuilder.newRowGroup(violatedState);
        transitionMatrixBuilder.addNextValue(violatedState, violatedState, storm::utility::one<ValueType>());
        svbuilder.addState(violatedState, {}, {-1, -1});
    }

    // Now we are starting to build the MDP from the initial state onwards.
    uint64_t newRowGroupStart = initialState;
    uint64_t newRowCount = initialState;

    // Notice that we are going to use a special last step
    for (uint64_t step = 0; step < observations.size() - 1; ++step) {
        oldToUnfolded.clear();
        unfoldedToOld = unfoldedToOldNextStep;
        unfoldedToOldNextStep.clear();

        for (auto const& unfoldedToOldEntry : unfoldedToOld) {
            transitionMatrixBuilder.newRowGroup(newRowGroupStart);
#ifdef _VERBOSE_OBSERVATION_UNFOLDING
            std::cout << "\tconsider new state " << unfoldedToOldEntry.first << '\n';
#endif
            STORM_LOG_ASSERT(step == 0 || newRowCount == transitionMatrixBuilder.getLastRow() + 1,
                             "step " << step << " newRowCount " << newRowCount << " lastRow " << transitionMatrixBuilder.getLastRow());
            svbuilder.addState(unfoldedToOldEntry.first, {}, {static_cast<int64_t>(unfoldedToOldEntry.second), static_cast<int64_t>(step)});
            uint64_t oldRowIndexStart = model.getNondeterministicChoiceIndices()[unfoldedToOldEntry.second];
            uint64_t oldRowIndexEnd = model.getNondeterministicChoiceIndices()[unfoldedToOldEntry.second + 1];

            for (uint64_t oldRowIndex = oldRowIndexStart; oldRowIndex != oldRowIndexEnd; oldRowIndex++) {
#ifdef _VERBOSE_OBSERVATION_UNFOLDING
                std::cout << "\t\tconsider old action " << oldRowIndex << '\n';
                std::cout << "\t\tconsider new row nr " << newRowCount << '\n';
#endif

                ValueType resetProb = storm::utility::zero<ValueType>();
                // We first find the reset probability
                for (auto const& oldRowEntry : model.getTransitionMatrix().getRow(oldRowIndex)) {
                    if (model.getObservation(oldRowEntry.getColumn()) != observations[step + 1]) {
                        resetProb += oldRowEntry.getValue();
                        if constexpr (std::is_same_v<ValueType, storm::Interval>) {
                            resetProb.setUpper(std::min(resetProb.upper(), 1.0));
                            resetProb.setLower(std::max(resetProb.lower(), 0.0));
                        } else if constexpr (std::is_same_v<ValueType, storm::RationalInterval>) {
                            resetProb.setUpper(std::min(resetProb.upper(), utility::one<storm::RationalNumber>()));
                            resetProb.setLower(std::max(resetProb.lower(), utility::zero<storm::RationalNumber>()));
                        }
                    }
                }
#ifdef _VERBOSE_OBSERVATION_UNFOLDING
                std::cout << "\t\t\t add reset with probability " << resetProb << '\n';
#endif

                // Add the resets
                if (resetProb != storm::utility::zero<ValueType>()) {
                    transitionMatrixBuilder.addNextValue(newRowCount, resetDestination, resetProb);
                }
#ifdef _VERBOSE_OBSERVATION_UNFOLDING
                std::cout << "\t\t\t add other transitions...\n";
#endif

                // Now, we build the outgoing transitions.
                for (auto const& oldRowEntry : model.getTransitionMatrix().getRow(oldRowIndex)) {
                    if (model.getObservation(oldRowEntry.getColumn()) != observations[step + 1]) {
                        continue;  // already handled.
                    }
                    uint64_t column = 0;

                    auto entryIt = oldToUnfolded.find(oldRowEntry.getColumn());
                    if (entryIt == oldToUnfolded.end()) {
                        column = newStateIndex;
                        oldToUnfolded[oldRowEntry.getColumn()] = column;
                        unfoldedToOldNextStep[column] = oldRowEntry.getColumn();
                        newStateIndex++;
                    } else {
                        column = entryIt->second;
                    }
#ifdef _VERBOSE_OBSERVATION_UNFOLDING
                    std::cout << "\t\t\t\t transition to " << column << "with probability " << oldRowEntry.getValue() << '\n';
#endif
                    transitionMatrixBuilder.addNextValue(newRowCount, column, oldRowEntry.getValue());
                }
                newRowCount++;
            }
            newRowGroupStart = transitionMatrixBuilder.getLastRow() + 1;
        }
    }
    // Now, take care of the last step.
    uint64_t sinkState = newStateIndex;
    uint64_t targetState = newStateIndex + 1;
    for (auto const& unfoldedToOldEntry : unfoldedToOldNextStep) {
        svbuilder.addState(unfoldedToOldEntry.first, {}, {static_cast<int64_t>(unfoldedToOldEntry.second), static_cast<int64_t>(observations.size() - 1)});

        transitionMatrixBuilder.newRowGroup(newRowGroupStart);
        STORM_LOG_ASSERT(risk.size() > unfoldedToOldEntry.second, "Must be a state");
        STORM_LOG_ASSERT(!storm::utility::isBetween(storm::utility::zero<ValueType>(), risk[unfoldedToOldEntry.second], storm::utility::one<ValueType>()),
                         "Risk must be a probability");
        // std::cout << "risk is" <<  risk[unfoldedToOldEntry.second] << '\n';
        if (!storm::utility::isOne(risk[unfoldedToOldEntry.second])) {
            transitionMatrixBuilder.addNextValue(newRowGroupStart, sinkState, storm::utility::one<ValueType>() - risk[unfoldedToOldEntry.second]);
        }
        if (!storm::utility::isZero(risk[unfoldedToOldEntry.second])) {
            transitionMatrixBuilder.addNextValue(newRowGroupStart, targetState, risk[unfoldedToOldEntry.second]);
        }
        newRowGroupStart++;
    }
    // sink state
    transitionMatrixBuilder.newRowGroup(newRowGroupStart);
    transitionMatrixBuilder.addNextValue(newRowGroupStart, sinkState, storm::utility::one<ValueType>());
    svbuilder.addState(sinkState, {}, {-1, -1});

    newRowGroupStart++;
    transitionMatrixBuilder.newRowGroup(newRowGroupStart);
    // target state
    transitionMatrixBuilder.addNextValue(newRowGroupStart, targetState, storm::utility::one<ValueType>());
    svbuilder.addState(targetState, {}, {-1, -1});
#ifdef _VERBOSE_OBSERVATION_UNFOLDING
    std::cout << "build matrix...\n";
#endif

    storm::storage::sparse::ModelComponents<ValueType> components;
    components.transitionMatrix = transitionMatrixBuilder.build();
#ifdef _VERBOSE_OBSERVATION_UNFOLDING
    // std::cout << components.transitionMatrix << '\n';
#endif

    storm::models::sparse::StateLabeling labeling(components.transitionMatrix.getRowGroupCount());
    labeling.addLabel("_goal");
    labeling.addLabelToState("_goal", targetState);
    if (!options.rejectionSampling) {
        labeling.addLabel("_violated");
        labeling.addLabelToState("_violated", violatedState);
    }
    labeling.addLabel("_end");
    labeling.addLabelToState("_end", sinkState);
    labeling.addLabelToState("_end", targetState);
    labeling.addLabel("init");
    labeling.addLabelToState("init", initialState);
    components.stateLabeling = labeling;
    components.stateValuations = svbuilder.build();
    return std::make_shared<storm::models::sparse::Mdp<ValueType>>(std::move(components));
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Mdp<ValueType>> ObservationTraceUnfolder<ValueType>::extend(std::vector<uint32_t> const& observations) {
    traceSoFar.insert(traceSoFar.end(), observations.begin(), observations.end());
    return transform(traceSoFar);
}

template<typename ValueType>
void ObservationTraceUnfolder<ValueType>::reset(uint32_t observation) {
    traceSoFar = {observation};
}

template<typename ValueType>
bool ObservationTraceUnfolder<ValueType>::isRejectionSamplingSet() const {
    return options.rejectionSampling;
}

template class ObservationTraceUnfolder<double>;
template class ObservationTraceUnfolder<storm::Interval>;
template class ObservationTraceUnfolder<storm::RationalInterval>;
template class ObservationTraceUnfolder<storm::RationalNumber>;
template class ObservationTraceUnfolder<storm::RationalFunction>;
}  // namespace pomdp
}  // namespace storm
