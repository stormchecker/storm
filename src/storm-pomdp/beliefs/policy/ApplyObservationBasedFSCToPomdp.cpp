#include "storm-pomdp/beliefs/policy/ApplyObservationBasedFSCToPomdp.h"

#include <optional>
#include <queue>
#include <ranges>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace pomdp::policy {
template<typename PomdpValueType>
ObservationSignature getObservationSignature(storm::models::sparse::Pomdp<PomdpValueType> const& pomdp, uint64_t const observationId) {
    return ObservationSignature::fromValuations(pomdp.getObservationValuations(), observationId);
}

template<typename PomdpValueType, typename FscValueType>
storm::models::sparse::Dtmc<PomdpValueType> applyObservationBasedFSCToPomdp(
    storm::models::sparse::Pomdp<PomdpValueType> const& pomdp, storm::pomdp::policy::ObservationBasedFiniteStateController<FscValueType> const& fsc,
    bool treatUnspecifiedChoiceAsDontCare) {
    STORM_LOG_THROW(pomdp.isCanonic(), storm::exceptions::IllegalArgumentException, "Applying an FSC requires a canonic POMDP.");
    STORM_LOG_THROW(
        pomdp.getInitialStates().hasUniqueSetBit(), storm::exceptions::NotSupportedException,
        "Applying an FSC requires exactly one initial state, but the POMDP contains " << pomdp.getInitialStates().getNumberOfSetBits() << " initial states.");
    for (auto const& [rewardModelName, rewardModel] : pomdp.getRewardModels()) {
        STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                        "Applying an FSC does not support transition rewards, but reward model \"" << rewardModelName << "\" contains them.");
    }
    std::unordered_map<uint64_t, std::pair<uint64_t, uint64_t>> idToPomdpStateAndFscNode;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint64_t>> pomdpStateAndFscNodeToNewStateId;
    storm::storage::SparseMatrixBuilder<PomdpValueType> matrixBuilder;
    std::unordered_map<std::string, std::vector<PomdpValueType>> stateRewards;

    uint64_t initialStateId = 0ul;
    uint64_t nextStateId = 1ul;
    uint64_t initialPomdpState = pomdp.getInitialStates().getNextSetIndex(0ul);
    idToPomdpStateAndFscNode[initialStateId] = {initialPomdpState, fsc.getInitialNodeId()};
    pomdpStateAndFscNodeToNewStateId[initialPomdpState][fsc.getInitialNodeId()] = initialStateId;
    STORM_LOG_DEBUG("Add initial state " << initialStateId << " (P: " << initialPomdpState << ", F: " << fsc.getInitialNodeId() << ")");

    std::optional<std::unordered_map<uint64_t, uint64_t>> pomdpObservationIdToFscObservationId = std::nullopt;
    std::optional<std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint64_t>>> fscActionIdToPomdpActionIdMap = std::nullopt;
    // If the POMDP and FSC have matching metadata, use it to create mappings between actions and observations.
    // Otherwise, we assume that IDs correspond directly.
    if (fsc.hasIdToObservationSignatureMap()) {
        STORM_LOG_THROW(pomdp.hasObservationValuations(), storm::exceptions::UnexpectedException,
                        "The FSC uses observation signatures, but the POMDP has no observation valuations. Cannot apply the FSC robustly.");
        std::unordered_map<uint64_t, uint64_t> observationMap;
        for (uint64_t pomdpObservationId = 0; pomdpObservationId < pomdp.getNrObservations(); ++pomdpObservationId) {
            auto const observationSignature = getObservationSignature(pomdp, pomdpObservationId);
            auto fscObservationId = fsc.getObservationIdBySignature(observationSignature);
            STORM_LOG_THROW(fscObservationId.has_value(), storm::exceptions::UnexpectedException,
                            "Could not map POMDP observation " << pomdpObservationId << " (" << observationSignatureToString(observationSignature)
                                                               << ") to an FSC observation.");
            observationMap[pomdpObservationId] = *fscObservationId;
        }
        pomdpObservationIdToFscObservationId = std::move(observationMap);
    }

    if (fsc.hasIdToActionNameMap()) {
        STORM_LOG_THROW(pomdp.hasChoiceLabeling(), storm::exceptions::UnexpectedException,
                        "The FSC uses action names, but the POMDP has no choice labeling. Cannot apply the FSC robustly.");
        std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint64_t>> actionMap;
        auto const nrStates = pomdp.getNumberOfStates();
        std::vector<uint64_t> representativeStates(pomdp.getNrObservations(), nrStates);
        for (uint64_t state = 0; state < nrStates; ++state) {
            auto& representativeState = representativeStates[pomdp.getObservation(state)];
            if (representativeState == nrStates) {
                representativeState = state;
            }
        }
        for (uint64_t pomdpObservationId = 0; pomdpObservationId < pomdp.getNrObservations(); ++pomdpObservationId) {
            auto const representativeState = representativeStates[pomdpObservationId];
            if (representativeState == nrStates) {
                continue;
            }
            uint64_t const pomdpRowGroupIndex = pomdp.getTransitionMatrix().getRowGroupIndices()[representativeState];
            uint64_t const pomdpRowGroupSize = pomdp.getTransitionMatrix().getRowGroupSize(representativeState);
            uint64_t const fscObservationId = pomdpObservationIdToFscObservationId && pomdpObservationIdToFscObservationId->contains(pomdpObservationId)
                                                  ? pomdpObservationIdToFscObservationId->at(pomdpObservationId)
                                                  : pomdpObservationId;
            for (uint64_t localActionIndex = 0; localActionIndex < pomdpRowGroupSize; ++localActionIndex) {
                // Numeric fallbacks are only valid for FSC actions without a name, regardless of the POMDP choice labels.
                if (!fsc.hasActionName(fscObservationId, localActionIndex)) {
                    actionMap[fscObservationId].try_emplace(localActionIndex, localActionIndex);
                }
                auto const& labels = pomdp.getChoiceLabeling().getLabelsOfChoice(pomdpRowGroupIndex + localActionIndex);
                if (labels.empty()) {
                    continue;
                }
                std::string const& actionName = *labels.begin();
                if (auto fscActionId = fsc.getActionIdByName(fscObservationId, actionName)) {
                    actionMap[fscObservationId][*fscActionId] = localActionIndex;
                }
            }
        }
        fscActionIdToPomdpActionIdMap = std::move(actionMap);
    }

    if (!pomdpObservationIdToFscObservationId && !fscActionIdToPomdpActionIdMap) {
        STORM_LOG_WARN(
            "The POMDP or the FSC is missing naming information for actions or observations. Assuming that IDs correspond directly (this might lead to "
            "unexpected results if they don't).");
    }
    std::queue<uint64_t> statesToExplore;
    statesToExplore.push(initialStateId);
    while (!statesToExplore.empty()) {
        uint64_t currentStateId = statesToExplore.front();
        auto [currentPomdpState, currentFscNode] = idToPomdpStateAndFscNode[currentStateId];
        statesToExplore.pop();
        STORM_LOG_DEBUG("Explore state " << currentStateId << " (P: " << currentPomdpState << ", F: " << currentFscNode << ")");
        storm::storage::Distribution<FscValueType, uint64_t> actionDistribution;
        uint64_t nextFSCNode;
        uint64_t const pomdpObservationId = pomdp.getObservation(currentPomdpState);
        uint64_t const fscObservationId = pomdpObservationIdToFscObservationId && pomdpObservationIdToFscObservationId->contains(pomdpObservationId)
                                              ? pomdpObservationIdToFscObservationId->at(pomdpObservationId)
                                              : pomdpObservationId;
        bool const hasFscOutput = fsc.hasOutputForObservationInNode(currentFscNode, fscObservationId);
        if (hasFscOutput) {
            std::tie(actionDistribution, nextFSCNode) = fsc.getActionDistributionAndSuccessorForObservationInNode(currentFscNode, fscObservationId);
        } else {
            STORM_LOG_THROW(treatUnspecifiedChoiceAsDontCare, storm::exceptions::UnexpectedException,
                            "Expected FSC to have an output for observation " << fscObservationId << " in FSC node " << currentFscNode << ", but it does not.");
            // If requested, pick POMDP local action zero and stay in the same FSC node.
            nextFSCNode = currentFscNode;
            actionDistribution = storm::storage::Distribution<FscValueType, uint64_t>();
            actionDistribution.addProbability(0ul, storm::utility::one<FscValueType>());
        }
        for (auto const& [rewardModelName, rewardModel] : pomdp.getRewardModels()) {
            auto& rewards = stateRewards[rewardModelName];
            rewards.resize(currentStateId + 1, storm::utility::zero<PomdpValueType>());
            if (rewardModel.hasStateRewards()) {
                rewards[currentStateId] += rewardModel.getStateRewardVector()[currentPomdpState];
            }
        }
        // Accumulate contributions from different actions to the same successor before inserting the row into the matrix.
        // This avoids repeated matrix lookups and insertions while the builder keeps the row sorted.
        std::unordered_map<uint64_t, PomdpValueType> successorProbabilities;
        for (auto const& actionEntry : actionDistribution) {
            auto const actionProbability = storm::utility::convertNumber<PomdpValueType>(actionEntry.second);
            uint64_t pomdpActionId = actionEntry.first;
            if (hasFscOutput && fscActionIdToPomdpActionIdMap) {
                STORM_LOG_THROW(fscActionIdToPomdpActionIdMap->contains(fscObservationId) &&
                                    fscActionIdToPomdpActionIdMap->at(fscObservationId).contains(actionEntry.first),
                                storm::exceptions::UnexpectedException,
                                "Could not map FSC action " << actionEntry.first << " for FSC observation " << fscObservationId << " to a POMDP action.");
                pomdpActionId = fscActionIdToPomdpActionIdMap->at(fscObservationId).at(actionEntry.first);
            }
            for (auto const& [rewardModelName, rewardModel] : pomdp.getRewardModels()) {
                // We take the expected reward over the action distribution, so not all properties are preserved.
                if (rewardModel.hasStateActionRewards()) {
                    uint64_t globalActionIndex = pomdp.getTransitionMatrix().getRowGroupIndices()[currentPomdpState] + pomdpActionId;
                    stateRewards[rewardModelName][currentStateId] += rewardModel.getStateActionRewardVector()[globalActionIndex] * actionProbability;
                }
            }
            for (auto const& transitionEntry : pomdp.getTransitionMatrix().getRow(currentPomdpState, pomdpActionId)) {
                uint64_t successorPomdpState = transitionEntry.getColumn();
                uint64_t successorNewStateId;
                if (pomdpStateAndFscNodeToNewStateId.contains(successorPomdpState) &&
                    pomdpStateAndFscNodeToNewStateId[successorPomdpState].contains(nextFSCNode)) {
                    successorNewStateId = pomdpStateAndFscNodeToNewStateId[successorPomdpState][nextFSCNode];
                } else {
                    successorNewStateId = nextStateId;
                    idToPomdpStateAndFscNode[successorNewStateId] = {
                        successorPomdpState,
                        nextFSCNode,
                    };
                    pomdpStateAndFscNodeToNewStateId[successorPomdpState][nextFSCNode] = successorNewStateId;
                    STORM_LOG_DEBUG("Add state " << successorNewStateId << " (P: " << successorPomdpState << ", F: " << nextFSCNode << ")");
                    ++nextStateId;
                    statesToExplore.push(successorNewStateId);
                }
                if (successorProbabilities.contains(successorNewStateId)) {
                    // Add transition from currentStateId to successorNewStateId with probability from POMDP
                    successorProbabilities[successorNewStateId] += transitionEntry.getValue() * actionProbability;
                } else {
                    successorProbabilities[successorNewStateId] = transitionEntry.getValue() * actionProbability;
                }
            }
        }
        for (auto const& [successorStateId, probability] : successorProbabilities) {
            matrixBuilder.addNextValue(currentStateId, successorStateId, probability);
        }
    }
    storm::storage::sparse::ModelComponents<PomdpValueType> modelComponents;
    modelComponents.transitionMatrix = matrixBuilder.build(nextStateId, nextStateId);

    // Add state labeling
    storm::models::sparse::StateLabeling stateLabeling(nextStateId);
    // Eagerly add all labels to preserve labels that may be unreachable under the FSC, but relevant for a property
    for (auto const& label : pomdp.getStateLabeling().getLabels()) {
        stateLabeling.addLabel(label);
    }
    stateLabeling.addLabelToState("init", initialStateId);
    for (uint64_t pomdpState = 0; pomdpState < pomdp.getNumberOfStates(); ++pomdpState) {
        if (pomdpStateAndFscNodeToNewStateId.contains(pomdpState)) {
            for (const auto& newStateId : pomdpStateAndFscNodeToNewStateId[pomdpState] | std::views::values) {
                for (auto const& label : pomdp.getStateLabeling().getLabelsOfState(pomdpState)) {
                    if (label != "init") {
                        stateLabeling.addLabelToState(label, newStateId);
                    }
                }
            }
        }
    }
    modelComponents.stateLabeling = stateLabeling;

    // build reward models
    for (auto const& [rewardModelName, rewardValues] : stateRewards) {
        storm::models::sparse::StandardRewardModel<PomdpValueType> rewardModel(rewardValues);
        modelComponents.rewardModels[rewardModelName] = rewardModel;
    }

    return storm::models::sparse::Dtmc<PomdpValueType>(modelComponents);
}

template ObservationSignature getObservationSignature(storm::models::sparse::Pomdp<double> const&, uint64_t);
template ObservationSignature getObservationSignature(storm::models::sparse::Pomdp<storm::RationalNumber> const&, uint64_t);

template storm::models::sparse::Dtmc<double> applyObservationBasedFSCToPomdp(storm::models::sparse::Pomdp<double> const&,
                                                                             storm::pomdp::policy::ObservationBasedFiniteStateController<double> const&, bool);
template storm::models::sparse::Dtmc<double> applyObservationBasedFSCToPomdp(
    storm::models::sparse::Pomdp<double> const&, storm::pomdp::policy::ObservationBasedFiniteStateController<storm::RationalNumber> const&, bool);
template storm::models::sparse::Dtmc<storm::RationalNumber> applyObservationBasedFSCToPomdp(
    storm::models::sparse::Pomdp<storm::RationalNumber> const&, storm::pomdp::policy::ObservationBasedFiniteStateController<double> const&, bool);
template storm::models::sparse::Dtmc<storm::RationalNumber> applyObservationBasedFSCToPomdp(
    storm::models::sparse::Pomdp<storm::RationalNumber> const&, storm::pomdp::policy::ObservationBasedFiniteStateController<storm::RationalNumber> const&,
    bool);
}  // namespace pomdp::policy
}  // namespace storm
