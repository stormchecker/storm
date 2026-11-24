#include "storm/storage/sparse/ChoiceOrigins.h"

#include "storm/adapters/JsonAdapter.h"

#include "storm/storage/sparse/JaniChoiceOrigins.h"
#include "storm/storage/sparse/PrismChoiceOrigins.h"
#include "storm/utility/vector.h"

#include "storm/utility/macros.h"

namespace storm {
namespace storage {
namespace sparse {

ChoiceOrigins::ChoiceOrigins(std::vector<uint_fast64_t> const& indexToIdentifierMapping) : indexToIdentifier(indexToIdentifierMapping) {
    // Intentionally left empty
}

ChoiceOrigins::ChoiceOrigins(std::vector<uint_fast64_t>&& indexToIdentifierMapping) : indexToIdentifier(std::move(indexToIdentifierMapping)) {
    // Intentionally left empty
}

bool ChoiceOrigins::isPrismChoiceOrigins() const {
    return false;
}

bool ChoiceOrigins::isJaniChoiceOrigins() const {
    return false;
}

PrismChoiceOrigins& ChoiceOrigins::asPrismChoiceOrigins() {
    return dynamic_cast<PrismChoiceOrigins&>(*this);
}

PrismChoiceOrigins const& ChoiceOrigins::asPrismChoiceOrigins() const {
    return dynamic_cast<PrismChoiceOrigins const&>(*this);
}

JaniChoiceOrigins& ChoiceOrigins::asJaniChoiceOrigins() {
    return dynamic_cast<JaniChoiceOrigins&>(*this);
}

JaniChoiceOrigins const& ChoiceOrigins::asJaniChoiceOrigins() const {
    return dynamic_cast<JaniChoiceOrigins const&>(*this);
}

uint_fast64_t ChoiceOrigins::getIdentifier(uint_fast64_t choiceIndex) const {
    STORM_LOG_ASSERT(choiceIndex < indexToIdentifier.size(), "Invalid choice index: " << choiceIndex);
    return indexToIdentifier[choiceIndex];
}

std::vector<uint_fast64_t> const& ChoiceOrigins::getIdentifiers() const {
    return indexToIdentifier;
}

uint_fast64_t ChoiceOrigins::getNumberOfChoices() const {
    return indexToIdentifier.size();
}

uint_fast64_t ChoiceOrigins::getIdentifierForChoicesWithNoOrigin() {
    return 0;
}

std::string const& ChoiceOrigins::getIdentifierInfo(uint_fast64_t identifier) const {
    STORM_LOG_ASSERT(identifier < this->getNumberOfIdentifiers(), "Invalid choice origin identifier: " << identifier);
    if (identifierToInfo.empty()) {
        computeIdentifierInfos();
    }
    return identifierToInfo[identifier];
}

std::string const& ChoiceOrigins::getChoiceInfo(uint_fast64_t choiceIndex) const {
    return getIdentifierInfo(getIdentifier(choiceIndex));
}

typename ChoiceOrigins::Json const& ChoiceOrigins::getIdentifierAsJson(uint_fast64_t identifier) const {
    STORM_LOG_ASSERT(identifier < this->getNumberOfIdentifiers(), "Invalid choice origin identifier: " << identifier);
    if (identifierToJson.empty()) {
        computeIdentifierJson();
    }
    return identifierToJson[identifier];
}

typename ChoiceOrigins::Json const& ChoiceOrigins::getChoiceAsJson(uint_fast64_t choiceIndex) const {
    return getIdentifierAsJson(getIdentifier(choiceIndex));
}

std::shared_ptr<ChoiceOrigins> ChoiceOrigins::selectChoices(storm::storage::BitVector const& selectedChoices) const {
    std::vector<uint_fast64_t> indexToIdentifierMapping(selectedChoices.getNumberOfSetBits());
    storm::utility::vector::selectVectorValues(indexToIdentifierMapping, selectedChoices, indexToIdentifier);
    return cloneWithNewIndexToIdentifierMapping(std::move(indexToIdentifierMapping));
}

void ChoiceOrigins::clearOriginOfChoice(uint_fast64_t choiceIndex) {
    indexToIdentifier[choiceIndex] = getIdentifierForChoicesWithNoOrigin();
}

std::shared_ptr<ChoiceOrigins> ChoiceOrigins::selectChoices(std::vector<uint_fast64_t> const& selectedChoices) const {
    std::vector<uint_fast64_t> indexToIdentifierMapping;
    indexToIdentifierMapping.reserve(selectedChoices.size());
    for (auto const& selectedChoice : selectedChoices) {
        if (selectedChoice < this->indexToIdentifier.size()) {
            indexToIdentifierMapping.push_back(indexToIdentifier[selectedChoice]);
        } else {
            indexToIdentifierMapping.push_back(getIdentifierForChoicesWithNoOrigin());
        }
    }
    return cloneWithNewIndexToIdentifierMapping(std::move(indexToIdentifierMapping));
}

storm::models::sparse::ChoiceLabeling ChoiceOrigins::toChoiceLabeling() const {
    storm::models::sparse::ChoiceLabeling result(indexToIdentifier.size());
    for (uint_fast64_t identifier = 0; identifier < this->getNumberOfIdentifiers(); ++identifier) {
        storm::storage::BitVector choicesWithIdentifier =
            storm::utility::vector::filter<uint_fast64_t>(indexToIdentifier, [&identifier](uint_fast64_t i) -> bool { return i == identifier; });
        if (!choicesWithIdentifier.empty()) {
            result.addLabel(getIdentifierInfo(identifier), std::move(choicesWithIdentifier));
        }
    }
    return result;
}
}  // namespace sparse
}  // namespace storage
}  // namespace storm