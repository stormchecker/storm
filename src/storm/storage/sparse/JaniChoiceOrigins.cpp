#include "storm/storage/sparse/JaniChoiceOrigins.h"

#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/visitor/JSONExporter.h"

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {
namespace sparse {

JaniChoiceOrigins::JaniChoiceOrigins(std::shared_ptr<storm::jani::Model const> const& janiModel, std::vector<uint_fast64_t> const& indexToIdentifierMapping,
                                     std::vector<EdgeIndexSet> const& identifierToEdgeIndexSetMapping)
    : ChoiceOrigins(indexToIdentifierMapping), model(janiModel), identifierToEdgeIndexSet(identifierToEdgeIndexSetMapping) {
    STORM_LOG_THROW(identifierToEdgeIndexSet[this->getIdentifierForChoicesWithNoOrigin()].empty(), storm::exceptions::InvalidArgumentException,
                    "The given edge set for the choices without origin is non-empty.");
}

bool JaniChoiceOrigins::isJaniChoiceOrigins() const {
    return true;
}

uint_fast64_t JaniChoiceOrigins::getNumberOfIdentifiers() const {
    return identifierToEdgeIndexSet.size();
}

storm::jani::Model const& JaniChoiceOrigins::getModel() const {
    return *model;
}

JaniChoiceOrigins::EdgeIndexSet const& JaniChoiceOrigins::getEdgeIndexSet(uint_fast64_t choiceIndex) const {
    return identifierToEdgeIndexSet[this->getIdentifier(choiceIndex)];
}

std::shared_ptr<ChoiceOrigins> JaniChoiceOrigins::cloneWithNewIndexToIdentifierMapping(std::vector<uint_fast64_t>&& indexToIdentifierMapping) const {
    auto result = std::make_shared<JaniChoiceOrigins>(this->model, std::move(indexToIdentifierMapping), std::move(identifierToEdgeIndexSet));
    result->identifierToInfo = this->identifierToInfo;
    return result;
}

void JaniChoiceOrigins::computeIdentifierInfos() const {
    this->identifierToInfo.clear();
    this->identifierToInfo.reserve(this->getNumberOfIdentifiers());

    for (auto const& edgeSet : identifierToEdgeIndexSet) {
        std::stringstream ss;

        for (auto const& edgeIndex : edgeSet) {
            auto autAndEdgeOffset = storm::jani::Model::decodeAutomatonAndEdgeIndices(edgeIndex);
            ss << model->getAutomaton(autAndEdgeOffset.first).getEdge(autAndEdgeOffset.second).toString();
            ss << ",\n";
        }
        this->identifierToInfo.emplace_back(ss.str());
        ss.clear();
    }
}

void JaniChoiceOrigins::computeIdentifierJson() const {
    this->identifierToJson.clear();
    this->identifierToJson.reserve(this->getNumberOfIdentifiers());
    for (auto const& set : identifierToEdgeIndexSet) {
        Json setJson;
        if (set.empty()) {
            setJson = "No origin";
        } else {
            bool first = true;
            std::vector<Json> edgesJson;
            for (auto const& edgeIndex : set) {
                auto autAndEdgeOffset = storm::jani::Model::decodeAutomatonAndEdgeIndices(edgeIndex);
                auto const& automaton = model->getAutomaton(autAndEdgeOffset.first);
                auto const& edge = automaton.getEdge(autAndEdgeOffset.second);
                if (first) {
                    setJson["action-label"] = model->getAction(edge.getActionIndex()).getName();
                    first = false;
                }
                edgesJson.push_back(storm::jani::JsonExporter::getEdgeAsJson(*model, autAndEdgeOffset.first, autAndEdgeOffset.second));
                edgesJson.back()["automaton"] = automaton.getName();
            }
            setJson["transitions"] = std::move(edgesJson);
        }
        this->identifierToJson.push_back(std::move(setJson));
    }
}

std::size_t JaniChoiceOrigins::hash() const {
    return 0;
}
}  // namespace sparse
}  // namespace storage
}  // namespace storm
