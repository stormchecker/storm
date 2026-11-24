#include "ObservationBasedFiniteStateController.h"

#include <algorithm>
#include <ostream>
#include <sstream>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/Distribution.h"
#include "storm/storage/valuations/Valuations.h"
#include "storm/storage/valuations/ValuationsStorage.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::pomdp::policy {

ObservationSignature ObservationSignature::fromValuations(storm::storage::sparse::Valuations const& observationValuations, uint64_t const observationId) {
    ObservationSignature result;
    observationValuations.getStorage().readCallback(observationId, [&result](auto, auto const& variable, auto&& rawValue) {
        using RawValue = std::remove_cvref_t<decltype(rawValue)>;
        if constexpr (std::is_same_v<RawValue, std::nullopt_t>) {
            result.values.emplace(variable.getName(), std::monostate{});
        } else if constexpr (std::is_same_v<RawValue, std::string_view>) {
            result.values.emplace(variable.getName(), std::string(rawValue));
        } else if constexpr (std::is_same_v<RawValue, storm::NumberTraits<storm::RationalNumber>::IntegerType>) {
            result.values.emplace(variable.getName(), storm::RationalNumber(rawValue));
        } else {
            result.values.emplace(variable.getName(), std::forward<decltype(rawValue)>(rawValue));
        }
    });
    return result;
}

std::string observationSignatureToString(ObservationSignature const& signature) {
    std::stringstream stream;
    stream << "[";
    bool first = true;
    for (auto const& [observable, value] : signature.values) {
        if (!first) {
            stream << "\t& ";
        }
        first = false;
        stream << observable << "=";
        std::visit(
            [&stream](auto const& typedValue) {
                using TypedValue = std::decay_t<decltype(typedValue)>;
                if constexpr (std::is_same_v<TypedValue, std::monostate>) {
                    stream << "none";
                } else {
                    stream << typedValue;
                }
            },
            value);
    }
    return stream.str() + "]";
}

bool FSCOutputUpdate::randomisedActionOutput() const {
    return false;
}

template<typename ValueType>
bool RandomisedActionUpdate<ValueType>::randomisedActionOutput() const {
    return true;
}

static std::string escapeDotLabel(std::string const& label) {
    std::string result;
    result.reserve(label.size());
    for (char character : label) {
        switch (character) {
            case '\\':
                result += "\\\\";
                break;
            case '"':
                result += "\\\"";
                break;
            case '\n':
                result += "\\n";
                break;
            case '\r':
                result += "\\r";
                break;
            case '\t':
                result += "\\t";
                break;
            default:
                result += character;
                break;
        }
    }
    return result;
}

template<typename ValueType>
ObservationBasedFiniteStateController<ValueType>::ObservationBasedFiniteStateController(uint64_t const initialNode) : initialNodeId(initialNode) {
    // Intentionally left empty
}

template<typename ValueType>
ObservationBasedFiniteStateController<ValueType>::ObservationBasedFiniteStateController(
    uint64_t const initialNode, std::optional<std::unordered_map<uint64_t, ObservationSignature>> idToObservationSignatureMap,
    std::optional<std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>>> idToActionNameMap)
    : initialNodeId(initialNode), idToObservationSignature(std::move(idToObservationSignatureMap)), idToActionName(std::move(idToActionNameMap)) {
    // Intentionally left empty
}

template<typename ValueType>
void ObservationBasedFiniteStateController<ValueType>::addDeterministicActionTransition(uint64_t const originId, uint64_t const observationId,
                                                                                        uint64_t const actionId, uint64_t const targetId) {
    auto update = std::make_unique<DeterministicActionUpdate>();
    update->action = actionId;
    update->nextMemoryNode = targetId;
    addActionOutputUpdate(originId, observationId, std::move(update));
}

template<typename ValueType>
void ObservationBasedFiniteStateController<ValueType>::addRandomisedActionTransition(uint64_t const originId, uint64_t const observationId,
                                                                                     storm::storage::Distribution<ValueType, uint64_t> actionDistribution,
                                                                                     uint64_t targetId) {
    auto update = std::make_unique<RandomisedActionUpdate<ValueType>>();
    update->actionDistribution = std::move(actionDistribution);
    update->nextMemoryNode = targetId;
    addActionOutputUpdate(originId, observationId, std::move(update));
}

template<typename ValueType>
std::pair<uint64_t, uint64_t> ObservationBasedFiniteStateController<ValueType>::getActionAndSuccessorForObservationInNode(uint64_t const originId,
                                                                                                                          uint64_t const observationId) const {
    FSCOutputUpdate const& outputUpdate = getActionOutputUpdate(originId, observationId);
    STORM_LOG_ASSERT(!outputUpdate.randomisedActionOutput(),
                     "Output for observation (ID " << observationId << ") in node (ID " << originId << ") is randomised.");
    auto const& detActionUpdate = dynamic_cast<DeterministicActionUpdate const&>(outputUpdate);
    return {detActionUpdate.action, detActionUpdate.nextMemoryNode};
}

template<typename ValueType>
uint64_t ObservationBasedFiniteStateController<ValueType>::getActionForObservationInNode(uint64_t const originId, uint64_t const observationId) const {
    return getActionAndSuccessorForObservationInNode(originId, observationId).first;
}

template<typename ValueType>
uint64_t ObservationBasedFiniteStateController<ValueType>::getSuccessorForObservationInNode(uint64_t const originId, uint64_t const observationId) const {
    return getActionOutputUpdate(originId, observationId).nextMemoryNode;
}

template<typename ValueType>
std::pair<storm::storage::Distribution<ValueType, uint64_t>, uint64_t>
ObservationBasedFiniteStateController<ValueType>::getActionDistributionAndSuccessorForObservationInNode(uint64_t const originId,
                                                                                                        uint64_t const observationId) const {
    if (FSCOutputUpdate const& outputUpdate = getActionOutputUpdate(originId, observationId); outputUpdate.randomisedActionOutput()) {
        auto const& randActionUpdate = dynamic_cast<RandomisedActionUpdate<ValueType> const&>(outputUpdate);
        return {randActionUpdate.actionDistribution, randActionUpdate.nextMemoryNode};
    } else {
        auto const& detActionUpdate = dynamic_cast<DeterministicActionUpdate const&>(outputUpdate);
        storm::storage::Distribution<ValueType, uint64_t> diracForAction;
        diracForAction.addProbability(detActionUpdate.action, storm::utility::one<ValueType>());
        return {diracForAction, detActionUpdate.nextMemoryNode};
    }
}

template<typename ValueType>
storm::storage::Distribution<ValueType, uint64_t> ObservationBasedFiniteStateController<ValueType>::getActionDistributionForObservationInNode(
    uint64_t const originId, uint64_t const observationId) const {
    return getActionDistributionAndSuccessorForObservationInNode(originId, observationId).first;
}

template<typename ValueType>
void ObservationBasedFiniteStateController<ValueType>::setIdToObservationSignatureMap(
    std::unordered_map<uint64_t, ObservationSignature> const& idToObservationSignatureMap) {
    idToObservationSignature = idToObservationSignatureMap;
}

template<typename ValueType>
void ObservationBasedFiniteStateController<ValueType>::setIdToActionNameMap(
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, std::string>> const& idToActionNameMap) {
    idToActionName = idToActionNameMap;
}

template<typename ValueType>
ObservationSignature const& ObservationBasedFiniteStateController<ValueType>::getObservationSignature(uint64_t const observationId) const {
    STORM_LOG_ASSERT(idToObservationSignature, "idToObservationSignature is not set.");
    STORM_LOG_ASSERT(!idToObservationSignature->empty(), "idToObservationSignature is empty.");
    STORM_LOG_ASSERT(idToObservationSignature->contains(observationId), "Observation ID " << observationId << " not found.");
    return idToObservationSignature->at(observationId);
}

template<typename ValueType>
std::string ObservationBasedFiniteStateController<ValueType>::getActionName(uint64_t const observationId, uint64_t const actionId) const {
    STORM_LOG_ASSERT(idToActionName, "idToActionName is not set.");
    STORM_LOG_ASSERT(!idToActionName->empty(), "idToActionName is empty.");
    STORM_LOG_ASSERT(idToActionName->contains(observationId), "Observation ID " << observationId << " not found.");
    return idToActionName->at(observationId).at(actionId);
}

template<typename ValueType>
bool ObservationBasedFiniteStateController<ValueType>::hasActionName(uint64_t const observationId, uint64_t const actionId) const {
    return idToActionName && idToActionName->contains(observationId) && idToActionName->at(observationId).contains(actionId) &&
           !idToActionName->at(observationId).at(actionId).empty();
}

template<typename ValueType>
std::optional<uint64_t> ObservationBasedFiniteStateController<ValueType>::getObservationIdBySignature(ObservationSignature const& observationSignature) const {
    if (!idToObservationSignature) {
        return std::nullopt;
    }
    for (auto const& [observationId, signature] : *idToObservationSignature) {
        if (signature == observationSignature) {
            return observationId;
        }
    }
    return std::nullopt;
}

template<typename ValueType>
std::optional<uint64_t> ObservationBasedFiniteStateController<ValueType>::getActionIdByName(uint64_t const observationId, std::string const& actionName) const {
    if (!idToActionName || !idToActionName->contains(observationId)) {
        return std::nullopt;
    }
    for (auto const& [actionId, name] : idToActionName->at(observationId)) {
        if (name == actionName) {
            return actionId;
        }
    }
    return std::nullopt;
}

template<typename ValueType>
void ObservationBasedFiniteStateController<ValueType>::addActionOutputUpdate(uint64_t const originId, uint64_t const observationId,
                                                                             std::unique_ptr<FSCOutputUpdate> update) {
    auto& outputsForNode = transitions[originId];
    if (auto const existingUpdate = outputsForNode.find(observationId);
        existingUpdate != outputsForNode.end() && existingUpdate->second->randomisedActionOutput()) {
        --numberOfRandomisedOutputs;
    }
    if (update->randomisedActionOutput()) {
        ++numberOfRandomisedOutputs;
    }
    outputsForNode[observationId] = std::move(update);
    isDeterministicPolicy = numberOfRandomisedOutputs == 0;
}

template<typename ValueType>
bool ObservationBasedFiniteStateController<ValueType>::hasOutputForObservationInNode(uint64_t const originId, uint64_t const observationId) const {
    return transitions.contains(originId) && transitions.at(originId).contains(observationId);
}

template<typename ValueType>
FSCOutputUpdate const& ObservationBasedFiniteStateController<ValueType>::getActionOutputUpdate(uint64_t const originId, uint64_t const observationId) const {
    STORM_LOG_ASSERT(transitions.contains(originId), "Origin node (ID " << originId << " not found.");
    STORM_LOG_ASSERT(hasOutputForObservationInNode(originId, observationId),
                     "No output for observation (ID " << observationId << ") in node (ID " << originId << ").");
    return *transitions.at(originId).at(observationId);
}

template<typename ValueType>
bool ObservationBasedFiniteStateController<ValueType>::isDeterministic() const {
    return isDeterministicPolicy;
}

template<typename ValueType>
bool ObservationBasedFiniteStateController<ValueType>::outputIsRandomised(uint64_t const originId, uint64_t const observationId) const {
    return transitions.at(originId).at(observationId)->randomisedActionOutput();
}

template<typename ValueType>
uint64_t ObservationBasedFiniteStateController<ValueType>::getInitialNodeId() const {
    return initialNodeId;
}

template<typename ValueType>
bool ObservationBasedFiniteStateController<ValueType>::hasIdToObservationSignatureMap() const {
    return static_cast<bool>(idToObservationSignature);
}

template<typename ValueType>
bool ObservationBasedFiniteStateController<ValueType>::hasIdToActionNameMap() const {
    return static_cast<bool>(idToActionName);
}

template<typename ValueType>
std::string ObservationBasedFiniteStateController<ValueType>::toString() const {
    std::string result = "Observation-Based Finite State Controller:\n";
    result += "Initial Node ID: " + std::to_string(initialNodeId) + "\n";
    for (auto const& [originId, actionOutputMap] : transitions) {
        result += "Node " + std::to_string(originId) + ":\n";
        for (auto const& [observationId, outputUpdatePtr] : actionOutputMap) {
            if (idToObservationSignature && idToObservationSignature->contains(observationId)) {
                result += "(" + std::to_string(observationId) + ")" + observationSignatureToString(idToObservationSignature->at(observationId)) + " -> ";
            } else {
                result += std::to_string(observationId) + " -> ";
            }
            if (outputUpdatePtr->randomisedActionOutput()) {
                result += "({ ";
                for (auto randomisedOutputUpdatePtr = std::static_pointer_cast<RandomisedActionUpdate<ValueType> const>(outputUpdatePtr);
                     auto const& [actionId, prob] : randomisedOutputUpdatePtr->actionDistribution) {
                    if (idToActionName && idToActionName->contains(observationId) && idToActionName->at(observationId).contains(actionId)) {
                        result += "(" + std::to_string(actionId) + ") " + idToActionName->at(observationId).at(actionId) + " : " +
                                  storm::utility::to_string(prob) + ", ";
                    } else {
                        result += "(" + std::to_string(actionId) + ") : " + storm::utility::to_string(prob) + ", ";
                    }
                }
                result += "}, " + std::to_string(outputUpdatePtr->nextMemoryNode) + ")\n";
            } else {
                auto deterministicOutputUpdatePtr = std::static_pointer_cast<DeterministicActionUpdate const>(outputUpdatePtr);
                result += "(";
                if (idToActionName && idToActionName->contains(observationId) &&
                    idToActionName->at(observationId).contains(deterministicOutputUpdatePtr->action)) {
                    result += "(" + std::to_string(deterministicOutputUpdatePtr->action) + ") " +
                              idToActionName->at(observationId).at(deterministicOutputUpdatePtr->action) + ", " +
                              std::to_string(outputUpdatePtr->nextMemoryNode) + ")\n";
                } else {
                    result += "(" + std::to_string(deterministicOutputUpdatePtr->action) + "), " + std::to_string(outputUpdatePtr->nextMemoryNode) + ")\n";
                }
            }
        }
    }
    return result;
}

template<typename ValueType>
void ObservationBasedFiniteStateController<ValueType>::writeDotToStream(std::ostream& outStream) const {
    outStream << "digraph fsc {\n";
    outStream << "\trankdir = LR;\n";
    outStream << "\tnode [shape = circle];\n";
    outStream << "\tinit [shape = point];\n";
    outStream << "\tinit -> " << initialNodeId << ";\n";

    std::unordered_set<uint64_t> nodes;
    nodes.insert(initialNodeId);
    for (auto const& [originId, actionOutputMap] : transitions) {
        nodes.insert(originId);
        for (auto const& [observationId, outputUpdatePtr] : actionOutputMap) {
            nodes.insert(outputUpdatePtr->nextMemoryNode);
            outStream << "\t" << originId << " -> " << outputUpdatePtr->nextMemoryNode << " [label=\"";
            if (idToObservationSignature && idToObservationSignature->contains(observationId)) {
                outStream << escapeDotLabel(observationSignatureToString(getObservationSignature(observationId)));
            } else {
                outStream << observationId;
            }
            outStream << " / ";
            if (outputUpdatePtr->randomisedActionOutput()) {
                auto const& randActionUpdate = dynamic_cast<RandomisedActionUpdate<ValueType> const&>(*outputUpdatePtr);
                outStream << "{";
                bool first = true;
                for (auto const& [actionId, prob] : randActionUpdate.actionDistribution) {
                    if (!first) {
                        outStream << ", ";
                    }
                    first = false;
                    if (idToActionName && idToActionName->contains(observationId) && idToActionName->at(observationId).contains(actionId)) {
                        outStream << escapeDotLabel(idToActionName->at(observationId).at(actionId));
                    } else {
                        outStream << actionId;
                    }
                    outStream << ":" << storm::utility::to_string(prob);
                }
                outStream << "}";
            } else {
                auto const& detActionUpdate = dynamic_cast<DeterministicActionUpdate const&>(*outputUpdatePtr);
                if (idToActionName && idToActionName->contains(observationId) && idToActionName->at(observationId).contains(detActionUpdate.action)) {
                    outStream << escapeDotLabel(idToActionName->at(observationId).at(detActionUpdate.action));
                } else {
                    outStream << detActionUpdate.action;
                }
            }
            outStream << "\"];\n";
        }
    }
    for (auto nodeId : nodes) {
        if (nodeId == initialNodeId) {
            outStream << "\t" << nodeId << " [shape = doublecircle];\n";
        }
    }
    outStream << "}\n";
}

template<typename ValueType>
void ObservationBasedFiniteStateController<ValueType>::writeJsonToStream(std::ostream& outStream) const {
    storm::json<ValueType> output;
    output["initial-node"] = initialNodeId;
    output["deterministic"] = isDeterministicPolicy;
    output["nodes"] = storm::json<ValueType>::array();

    std::vector<uint64_t> nodeIds;
    nodeIds.push_back(initialNodeId);
    for (auto const& [originId, actionOutputMap] : transitions) {
        nodeIds.push_back(originId);
        for (auto const& [observationId, outputUpdatePtr] : actionOutputMap) {
            nodeIds.push_back(outputUpdatePtr->nextMemoryNode);
        }
    }
    std::ranges::sort(nodeIds);
    nodeIds.erase(std::unique(nodeIds.begin(), nodeIds.end()), nodeIds.end());

    for (uint64_t nodeId : nodeIds) {
        storm::json<ValueType> node;
        node["id"] = nodeId;
        node["transitions"] = storm::json<ValueType>::array();
        if (transitions.contains(nodeId)) {
            std::vector<uint64_t> observationIds;
            observationIds.reserve(transitions.at(nodeId).size());
            for (auto const& [observationId, outputUpdatePtr] : transitions.at(nodeId)) {
                observationIds.push_back(observationId);
            }
            std::ranges::sort(observationIds);

            for (uint64_t observationId : observationIds) {
                auto const& update = *transitions.at(nodeId).at(observationId);
                storm::json<ValueType> transition;
                transition["observation"]["id"] = observationId;
                if (idToObservationSignature && idToObservationSignature->contains(observationId)) {
                    auto& signatureJson = transition["observation"]["signature"];
                    signatureJson = storm::json<ValueType>::array();
                    for (auto const& [observable, value] : idToObservationSignature->at(observationId).values) {
                        storm::json<ValueType> observableJson;
                        observableJson["name"] = observable;
                        std::visit(
                            [&observableJson](auto const& typedValue) {
                                using TypedValue = std::decay_t<decltype(typedValue)>;
                                if constexpr (std::is_same_v<TypedValue, std::monostate>) {
                                    observableJson["type"] = "unset";
                                    observableJson["value"] = nullptr;
                                } else if constexpr (std::is_same_v<TypedValue, storm::RationalNumber>) {
                                    observableJson["type"] = "rational";
                                    observableJson["value"] = storm::utility::to_string(typedValue);
                                } else if constexpr (std::is_same_v<TypedValue, bool>) {
                                    observableJson["type"] = "boolean";
                                    observableJson["value"] = typedValue;
                                } else if constexpr (std::is_same_v<TypedValue, uint64_t>) {
                                    observableJson["type"] = "unsigned-integer";
                                    observableJson["value"] = typedValue;
                                } else if constexpr (std::is_same_v<TypedValue, int64_t>) {
                                    observableJson["type"] = "integer";
                                    observableJson["value"] = typedValue;
                                } else if constexpr (std::is_same_v<TypedValue, double>) {
                                    observableJson["type"] = "double";
                                    observableJson["value"] = typedValue;
                                } else {
                                    static_assert(std::is_same_v<TypedValue, std::string>);
                                    observableJson["type"] = "string";
                                    observableJson["value"] = typedValue;
                                }
                            },
                            value);
                        signatureJson.push_back(std::move(observableJson));
                    }
                }
                transition["successor"] = update.nextMemoryNode;
                transition["actions"] = storm::json<ValueType>::array();

                std::vector<std::pair<uint64_t, ValueType>> actions;
                if (update.randomisedActionOutput()) {
                    auto const& randomizedUpdate = dynamic_cast<RandomisedActionUpdate<ValueType> const&>(update);
                    for (auto const& [actionId, probability] : randomizedUpdate.actionDistribution) {
                        actions.emplace_back(actionId, probability);
                    }
                } else {
                    auto const& deterministicUpdate = dynamic_cast<DeterministicActionUpdate const&>(update);
                    actions.emplace_back(deterministicUpdate.action, storm::utility::one<ValueType>());
                }
                std::ranges::sort(actions, {}, &std::pair<uint64_t, ValueType>::first);

                for (auto const& [actionId, probability] : actions) {
                    storm::json<ValueType> action;
                    action["id"] = actionId;
                    if (idToActionName && idToActionName->contains(observationId) && idToActionName->at(observationId).contains(actionId)) {
                        action["name"] = idToActionName->at(observationId).at(actionId);
                    }
                    action["probability"] = probability;
                    transition["actions"].push_back(std::move(action));
                }
                node["transitions"].push_back(std::move(transition));
            }
        }
        output["nodes"].push_back(std::move(node));
    }
    outStream << storm::dumpJson(output) << '\n';
}

template struct RandomisedActionUpdate<double>;
template struct RandomisedActionUpdate<storm::RationalNumber>;

template class ObservationBasedFiniteStateController<double>;
template class ObservationBasedFiniteStateController<storm::RationalNumber>;

}  // namespace storm::pomdp::policy
