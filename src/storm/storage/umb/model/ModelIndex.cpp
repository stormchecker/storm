#include "storm/storage/umb/model/ModelIndex.h"

#include <algorithm>
#include <ctime>
#include <sstream>

#include "storm/utility/macros.h"

namespace storm::umb {

std::string ModelIndex::FileData::creationDateAsString() const {
    if (creationDate) {
        std::time_t t = static_cast<std::time_t>(creationDate.value());
        return std::ctime(&t);
    } else {
        return "unknown";
    }
}
void ModelIndex::FileData::setCreationDateToNow() {
    if (std::time_t result; std::time(&result) == static_cast<std::time_t>(-1)) {
        STORM_LOG_WARN("No creation time is set for UMB index: unable to get the current time.");
        creationDate.reset();
    } else {
        creationDate.emplace(result);
    }
}

const std::string DefaultAnnotationAlias = "default";

std::string ModelIndex::Annotation::getValidIdentifierFromAlias(std::string const& alias) {
    auto isAllowed = [](auto ch) { return (std::isalnum(ch) && !std::isupper(ch)) || ch == '_' || ch == '-'; };
    if (alias.empty()) {
        return DefaultAnnotationAlias;  // empty identifier is not allowed, so we return a default name
    }

    std::stringstream identifier;
    for (auto ch : alias) {
        if (isAllowed(ch)) {
            identifier << ch;
        } else if (ch == ' ') {  // replace spaces with underscores
            identifier << '_';
        } else {  // replace other characters with their hex representation
            identifier << "0x" << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(ch);
        }
    }
    return identifier.str();
}

bool ModelIndex::Annotation::appliesToStates() const {
    return std::find(appliesTo.begin(), appliesTo.end(), AppliesTo::States) != appliesTo.end();
}

bool ModelIndex::Annotation::appliesToChoices() const {
    return std::find(appliesTo.begin(), appliesTo.end(), AppliesTo::Choices) != appliesTo.end();
}

bool ModelIndex::Annotation::appliesToBranches() const {
    return std::find(appliesTo.begin(), appliesTo.end(), AppliesTo::Branches) != appliesTo.end();
}

bool ModelIndex::Annotation::appliesToObservations() const {
    return std::find(appliesTo.begin(), appliesTo.end(), AppliesTo::Observations) != appliesTo.end();
}

bool ModelIndex::Annotation::appliesToPlayers() const {
    return std::find(appliesTo.begin(), appliesTo.end(), AppliesTo::Players) != appliesTo.end();
}

storm::OptionalRef<ModelIndex::AnnotationMap> ModelIndex::aps(bool createIfMissing) {
    return annotation("aps", createIfMissing);
}

storm::OptionalRef<ModelIndex::AnnotationMap const> ModelIndex::aps() const {
    return annotation("aps");
}

storm::OptionalRef<ModelIndex::AnnotationMap> ModelIndex::rewards(bool createIfMissing) {
    return annotation("rewards", createIfMissing);
}

storm::OptionalRef<ModelIndex::AnnotationMap const> ModelIndex::rewards() const {
    return annotation("rewards");
}

storm::OptionalRef<ModelIndex::AnnotationMap> ModelIndex::annotation(std::string const& annotationsType, bool createIfMissing) {
    if (createIfMissing) {
        if (!annotations.has_value()) {
            annotations.emplace();
        }
        return (*annotations)[annotationsType];
    }
    if (annotations.has_value()) {
        if (auto it = annotations->find(annotationsType); it != annotations->end()) {
            return it->second;
        }
    }
    return {};
}

storm::OptionalRef<ModelIndex::AnnotationMap const> ModelIndex::annotation(std::string const& annotationsType) const {
    if (annotations.has_value()) {
        if (auto it = annotations->find(annotationsType); it != annotations->end()) {
            return it->second;
        }
    }
    return {};
}

std::optional<std::string> ModelIndex::findAPName(std::string const& id) const {
    return findAnnotationName("aps", id);
}

std::optional<std::string> ModelIndex::findRewardName(std::string const& id) const {
    return findAnnotationName("rewards", id);
}

std::optional<std::string> ModelIndex::findAnnotationName(std::string const& annotationsType, std::string const& id) const {
    if (id.empty()) {  // the empty id indicates that we want the default annotation
        return findAnnotationName(annotationsType, DefaultAnnotationAlias);
    }

    auto map = annotation(annotationsType);
    if (!map.has_value()) {
        return {};
    }

    // First try to find a suitable alias matching id
    for (auto const& [name, annotation] : map.value()) {
        if (annotation.alias == id) {
            return name;
        }
    }

    // If no alias matches, check if there is an annotation with the right identifier
    if (map->contains(id)) {
        return id;
    }
    return {};
}

}  // namespace storm::umb