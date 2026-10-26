#include "PolicyExtractor.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/storage/scheduler/Scheduler.h"
#include "storm/storage/sparse/ModelComponents.h"

#include <algorithm>
#include <charconv>
#include <queue>
#include <string_view>
#include <unordered_set>

namespace storm::pomdp::policy {

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType, typename PolicyValueType>
PolicyExtractor<PomdpModelType, BeliefValueType, BeliefMdpValueType, PolicyValueType>::PolicyExtractor(
    PomdpModelType const& pomdp, BeliefMdpType const& beliefMdp, std::unordered_map<uint64_t, uint32_t> const& beliefStateToObservationMap,
    storm::storage::Scheduler<BeliefMdpValueType> const& beliefMdpScheduler,
    std::optional<std::vector<storm::storage::Scheduler<typename PomdpModelType::ValueType>>> const& pomdpApproximationSchedulers)
    : pomdp(pomdp),
      beliefMdp(beliefMdp),
      beliefMdpScheduler(beliefMdpScheduler),
      beliefStateToObservationMap(beliefStateToObservationMap),
      pomdpApproximationSchedulers(pomdpApproximationSchedulers) {
    // Intentionally left empty
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType, typename PolicyValueType>
ObservationBasedFiniteStateController<PolicyValueType>
PolicyExtractor<PomdpModelType, BeliefValueType, BeliefMdpValueType, PolicyValueType>::exportPolicyAsFiniteStateController() const {
    std::optional<std::unordered_map<uint64_t, ObservationSignature>> optionalIdToObservationSignature = std::nullopt;
    std::optional<std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>> optionalIdToActionNameMap = std::nullopt;
    // Target splitting can introduce observations without extending their valuations.
    if (pomdp.hasObservationValuations() && pomdp.getObservationValuations().getNumberOfEntities() >= pomdp.getNrObservations()) {
        auto const& obsValuations = pomdp.getObservationValuations();
        std::unordered_map<uint64_t, ObservationSignature> idToObservationSignatureMap;
        std::vector<ObservationSignature> usedObservationSignatures;
        bool observationSignaturesAreUnique = true;
        for (uint64_t i = 0; i < pomdp.getNrObservations(); ++i) {
            auto observationSignature = ObservationSignature::fromValuations(obsValuations, i);
            if (observationSignature.values.empty() || std::ranges::find(usedObservationSignatures, observationSignature) != usedObservationSignatures.end()) {
                observationSignaturesAreUnique = false;
                break;
            }
            usedObservationSignatures.push_back(observationSignature);
            idToObservationSignatureMap[i] = std::move(observationSignature);
        }
        if (observationSignaturesAreUnique) {
            optionalIdToObservationSignature = std::move(idToObservationSignatureMap);
        }
    } else if (pomdp.hasObservationValuations()) {
        STORM_LOG_WARN("Omitting observation signatures from the exported FSC because observation valuations cover only "
                       << pomdp.getObservationValuations().getNumberOfEntities() << " of " << pomdp.getNrObservations()
                       << " observations. Observation matching will use numeric IDs.");
    }
    if (pomdp.hasChoiceLabeling()) {
        auto const& choiceLabeling = pomdp.getChoiceLabeling();
        std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>> idToActionNameMap;
        auto const nrStates = pomdp.getNumberOfStates();
        std::vector<uint64_t> representativeStates(pomdp.getNrObservations(), nrStates);
        for (uint64_t state = 0; state < nrStates; ++state) {
            auto& representativeState = representativeStates[pomdp.getObservation(state)];
            if (representativeState == nrStates) {
                representativeState = state;
            }
        }
        for (uint64_t obs = 0; obs < pomdp.getNrObservations(); ++obs) {
            auto const representativeState = representativeStates[obs];
            if (representativeState == nrStates) {
                continue;
            }
            auto const rowGroupIndex = pomdp.getTransitionMatrix().getRowGroupIndices()[representativeState];
            auto const rowGroupSize = pomdp.getTransitionMatrix().getRowGroupSize(representativeState);
            for (uint64_t localChoice = 0; localChoice < rowGroupSize; ++localChoice) {
                auto const globalChoiceIndex = rowGroupIndex + localChoice;
                STORM_LOG_ASSERT(choiceLabeling.getLabelsOfChoice(globalChoiceIndex).size() <= 1,
                                 "Expected choice labeling to have at most one label per choice, but choice ID "
                                     << globalChoiceIndex << " has " << choiceLabeling.getLabelsOfChoice(globalChoiceIndex).size() << " labels.");
                if (choiceLabeling.getLabelsOfChoice(globalChoiceIndex).empty()) {
                    idToActionNameMap[obs][localChoice] = "";
                } else {
                    idToActionNameMap[obs][localChoice] = *choiceLabeling.getLabelsOfChoice(globalChoiceIndex).begin();
                }
            }
        }
        optionalIdToActionNameMap = idToActionNameMap;
    }
    ObservationBasedFiniteStateController<PolicyValueType> fsc(0ul, optionalIdToObservationSignature, optionalIdToActionNameMap);

    std::unordered_map<uint64_t, uint64_t> beliefStateToFscNodeMap;
    std::unordered_map<uint64_t, uint64_t> cutoffPolicyToFscNodeMap;
    using ObservationToActionMap = std::unordered_map<uint64_t, storm::storage::Distribution<PolicyValueType, uint64_t>>;
    std::unordered_map<uint64_t, ObservationToActionMap> cutoffPolicyToObservationMap;
    std::queue<uint64_t> statesToProcess;
    uint64_t nextId = 1ul;
    auto const addCutoffTransition = [&](uint64_t originNode, uint64_t cutoffBeliefState) {
        // State has been cut off. Add transition to self-looping cut-off policy node.
        auto const localCutoffChoice = beliefMdpScheduler.getChoice(cutoffBeliefState).getDeterministicChoice();
        auto const globalCutoffChoice = beliefMdp.getTransitionMatrix().getRowGroupIndices()[cutoffBeliefState] + localCutoffChoice;
        auto const& cutoffChoiceLabels = beliefMdp.getChoiceLabeling().getLabelsOfChoice(globalCutoffChoice);
        STORM_LOG_THROW(cutoffChoiceLabels.size() == 1, storm::exceptions::UnexpectedException,
                        "Expected the selected cutoff choice to have exactly one scheduler label.");
        std::string const& cutoffChoiceLabel = *cutoffChoiceLabels.begin();
        constexpr std::string_view schedulerLabelPrefix = "__sched_";
        STORM_LOG_THROW(cutoffChoiceLabel.starts_with(schedulerLabelPrefix), storm::exceptions::UnexpectedException,
                        "Expected cutoff scheduler label, but found '" << cutoffChoiceLabel << "'.");
        uint64_t chosenCutoffPolicy = 0;
        auto const schedulerIndex = std::string_view(cutoffChoiceLabel).substr(schedulerLabelPrefix.size());
        auto const [parseEnd, parseError] = std::from_chars(schedulerIndex.begin(), schedulerIndex.end(), chosenCutoffPolicy);
        STORM_LOG_THROW(parseError == std::errc() && parseEnd == schedulerIndex.end(), storm::exceptions::UnexpectedException,
                        "Could not parse cutoff scheduler label '" << cutoffChoiceLabel << "'.");
        STORM_LOG_THROW(pomdpApproximationSchedulers.has_value() && chosenCutoffPolicy < pomdpApproximationSchedulers->size(),
                        storm::exceptions::UnexpectedException,
                        "Cutoff scheduler label '" << cutoffChoiceLabel << "' does not refer to an available approximation scheduler.");
        if (!cutoffPolicyToFscNodeMap.contains(chosenCutoffPolicy)) {
            // TODO For finite-memory policies, create a node per scheduler memory state and preserve its memory updates.
            cutoffPolicyToFscNodeMap[chosenCutoffPolicy] = nextId;
            cutoffPolicyToObservationMap.emplace(chosenCutoffPolicy,
                                                 pomdpSchedulerToObservationBasedMap(pomdpApproximationSchedulers->at(chosenCutoffPolicy), 0));
            ++nextId;
        }
        auto const& dist = cutoffPolicyToObservationMap.at(chosenCutoffPolicy).at(beliefStateToObservationMap.at(cutoffBeliefState));
        if (dist.size() == 1) {
            fsc.addDeterministicActionTransition(originNode, beliefStateToObservationMap.at(cutoffBeliefState), dist.begin()->first,
                                                 cutoffPolicyToFscNodeMap[chosenCutoffPolicy]);
        } else {
            fsc.addRandomisedActionTransition(originNode, beliefStateToObservationMap.at(cutoffBeliefState), dist,
                                              cutoffPolicyToFscNodeMap[chosenCutoffPolicy]);
        }
    };

    // Add initial transition, using a continuation scheduler if the initial belief was cut off.
    auto const initialBeliefState = beliefMdp.getInitialStates().getNextSetIndex(0);
    if (beliefMdp.getStateLabeling().getStateHasLabel("truncated", initialBeliefState)) {
        addCutoffTransition(0ul, initialBeliefState);
    } else {
        auto const initialChoice = beliefMdpScheduler.getChoice(initialBeliefState).getDeterministicChoice();
        fsc.addDeterministicActionTransition(0ul, beliefStateToObservationMap.at(initialBeliefState), initialChoice, nextId);
        beliefStateToFscNodeMap[initialBeliefState] = nextId++;
        statesToProcess.push(initialBeliefState);
    }

    while (!statesToProcess.empty()) {
        uint64_t currentState = statesToProcess.front();
        statesToProcess.pop();
        auto const currentChoice = beliefMdpScheduler.getChoice(currentState).getDeterministicChoice();
        for (auto const& entry : beliefMdp.getTransitionMatrix().getRow(currentState, currentChoice)) {
            auto successorBeliefStateId = entry.getColumn();
            if (beliefMdp.getStateLabeling().getStateHasLabel("truncated", successorBeliefStateId)) {
                addCutoffTransition(beliefStateToFscNodeMap.at(currentState), successorBeliefStateId);
            } else {
                if (!beliefStateToFscNodeMap.contains(successorBeliefStateId)) {
                    beliefStateToFscNodeMap[successorBeliefStateId] = nextId;
                    ++nextId;
                    statesToProcess.push(successorBeliefStateId);
                }
                if (beliefStateToObservationMap.contains(successorBeliefStateId)) {
                    auto const successorChoice = beliefMdpScheduler.getChoice(successorBeliefStateId).getDeterministicChoice();
                    fsc.addDeterministicActionTransition(beliefStateToFscNodeMap[currentState], beliefStateToObservationMap.at(successorBeliefStateId),
                                                         successorChoice, beliefStateToFscNodeMap[successorBeliefStateId]);
                }  // else, the state does not actually correspond to a belief, and we don't care what to do afterwards
            }
        }
    }

    if (pomdpApproximationSchedulers.has_value()) {
        for (uint64_t i = 0; i < pomdpApproximationSchedulers->size(); ++i) {
            // TODO extend for fm-policies
            if (cutoffPolicyToFscNodeMap.contains(i)) {
                for (auto const& [observationId, actionDist] : cutoffPolicyToObservationMap.at(i)) {
                    if (actionDist.size() == 1) {
                        fsc.addDeterministicActionTransition(cutoffPolicyToFscNodeMap[i], observationId, actionDist.begin()->first,
                                                             cutoffPolicyToFscNodeMap[i]);
                    } else {
                        fsc.addRandomisedActionTransition(cutoffPolicyToFscNodeMap[i], observationId, actionDist, cutoffPolicyToFscNodeMap[i]);
                    }
                }
            }
        }
    }
    return fsc;
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType, typename PolicyValueType>
std::shared_ptr<storm::models::sparse::Model<BeliefMdpValueType>>
PolicyExtractor<PomdpModelType, BeliefValueType, BeliefMdpValueType, PolicyValueType>::exportPolicyAsInducedMarkovChain() const {
    storm::models::sparse::StateLabeling newLabeling(beliefMdp.getStateLabeling());
    if (pomdpApproximationSchedulers.has_value()) {
        for (uint64_t i = 0; i < pomdpApproximationSchedulers->size(); ++i) {
            newLabeling.addLabel("sched_" + std::to_string(i));
        }
    }
    for (uint64_t i = 0; i < pomdp.getNrObservations(); ++i) {
        newLabeling.addLabel("obs_" + std::to_string(i));
    }
    newLabeling.addLabel("cutoff");
    newLabeling.addLabel("clipping");
    newLabeling.addLabel("finite_mem");

    storm::storage::SparseMatrix<BeliefMdpValueType> transMatrix = beliefMdp.getTransitionMatrix();
    for (uint64_t i = 0; i < beliefMdp.getNumberOfStates(); ++i) {
        if (beliefStateToObservationMap.contains(i)) {
            newLabeling.addLabelToState("obs_" + std::to_string(beliefStateToObservationMap.at(i)), i);
        }
        if (newLabeling.getStateHasLabel("truncated", i)) {
            uint64_t localChosenActionIndex = beliefMdpScheduler.getChoice(i).getDeterministicChoice();
            auto const chosenRowIndex = transMatrix.getRowGroupIndices()[i] + localChosenActionIndex;
            if (beliefMdp.getChoiceLabeling().getLabelsOfChoice(chosenRowIndex).size() > 0) {
                if (auto label = *(beliefMdp.getChoiceLabeling().getLabelsOfChoice(chosenRowIndex).begin()); label.rfind("clip", 0) == 0) {
                    newLabeling.addLabelToState("clipping", i);
                    auto chosenRow = transMatrix.getRow(chosenRowIndex);
                    auto candidateIndex = (chosenRow.end() - 1)->getColumn();
                    transMatrix.makeRowDirac(chosenRowIndex, candidateIndex);
                } else if (label.rfind("__mem_node", 0) == 0) {
                    newLabeling.addLabelToState("finite_mem", i);
                    newLabeling.addLabelToState("cutoff", i);
                } else if (pomdpApproximationSchedulers.has_value()) {
                    // Label has form "__sched_X"
                    newLabeling.addLabelToState(label.substr(2), i);
                    newLabeling.addLabelToState("cutoff", i);
                }
            }
        }
    }
    newLabeling.removeLabel("truncated");

    transMatrix.dropZeroEntries();
    storm::storage::sparse::ModelComponents<BeliefMdpValueType> modelComponents(transMatrix, newLabeling, beliefMdp.getRewardModels());
    if (beliefMdp.hasChoiceLabeling()) {
        // Add empty string as label of all choices without labels
        storm::models::sparse::ChoiceLabeling extendedChoiceLabeling(beliefMdp.getChoiceLabeling());
        extendedChoiceLabeling.addLabel("");
        for (uint64_t i = 0; i < beliefMdp.getNumberOfChoices(); ++i) {
            if (beliefMdp.getChoiceLabeling().getLabelsOfChoice(i).empty()) {
                extendedChoiceLabeling.addLabelToChoice("", i);
            }
        }
        modelComponents.choiceLabeling = extendedChoiceLabeling;
    }
    storm::models::sparse::Mdp<BeliefMdpValueType> newMDP(modelComponents);
    auto inducedMC = newMDP.applyScheduler(beliefMdpScheduler, true);
    return std::static_pointer_cast<storm::models::sparse::Model<BeliefMdpValueType>>(inducedMC);
}

template<typename PomdpModelType, typename BeliefValueType, typename BeliefMdpValueType, typename PolicyValueType>
std::unordered_map<uint64_t, storm::storage::Distribution<PolicyValueType, uint64_t>>
PolicyExtractor<PomdpModelType, BeliefValueType, BeliefMdpValueType, PolicyValueType>::pomdpSchedulerToObservationBasedMap(
    storm::storage::Scheduler<typename PomdpModelType::ValueType> const& scheduler, uint64_t memoryState) const {
    std::unordered_map<uint64_t, storm::storage::Distribution<PolicyValueType, uint64_t>> result;

    // Iterate all POMDP states and group by observation id.
    const uint64_t numStates = pomdp.getNumberOfStates();
    std::vector<uint64_t> representativeStates(pomdp.getNrObservations(), numStates);
    for (uint64_t state = 0; state < numStates; ++state) {
        // Get the observation id for this state.
        uint64_t obs = pomdp.getObservation(state);

        // Within a fixed scheduler memory state, a single FSC output must represent all states of this observation.
        auto& representativeState = representativeStates[obs];
        if (representativeState != numStates) {
            STORM_LOG_THROW(std::ranges::equal(scheduler.getChoice(state, memoryState).getChoiceAsDistribution(),
                                               scheduler.getChoice(representativeState, memoryState).getChoiceAsDistribution()),
                            storm::exceptions::IllegalArgumentException,
                            "Cut-off approximation schedulers must be observation-based, but states "
                                << representativeState << " and " << state << " have different action distributions for observation " << obs
                                << " in memory state " << memoryState << ".");
            continue;
        }
        representativeState = state;

        // Build distribution from the scheduler choice for this state.
        auto choice = scheduler.getChoice(state, memoryState).template toValueType<PolicyValueType>();
        storm::storage::Distribution<PolicyValueType, uint64_t> dist = choice.getChoiceAsDistribution();

        result.emplace(obs, std::move(dist));
    }

    return result;
}

template class PolicyExtractor<models::sparse::Pomdp<double>, double, double>;
template class PolicyExtractor<storm::models::sparse::Pomdp<double>, storm::RationalNumber, double>;
template class PolicyExtractor<storm::models::sparse::Pomdp<storm::RationalNumber>, storm::RationalNumber, storm::RationalNumber>;
template class PolicyExtractor<storm::models::sparse::Pomdp<storm::RationalNumber>, storm::RationalNumber, double>;
template class PolicyExtractor<storm::models::sparse::Pomdp<double>, double, storm::RationalNumber>;
template class PolicyExtractor<storm::models::sparse::Pomdp<double>, double, double, storm::RationalNumber>;

}  // namespace storm::pomdp::policy
