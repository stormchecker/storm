#include "ArrayVariableReplacementInformation.h"

#include "storm/exceptions/OutOfRangeException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace generator {
ArrayVariableReplacementInformation::ArrayVariableReplacementInformation(std::vector<ArrayVariableReplacementInformation>&& children)
    : data(std::move(children)) {
    // Intentionally left empty
}

ArrayVariableReplacementInformation::ArrayVariableReplacementInformation(uint64_t informationIndex) : data(std::move(informationIndex)) {
    // Intentionally left empty
}

bool ArrayVariableReplacementInformation::isInformationIndex() const {
    return data.which() == 0;
}

uint64_t ArrayVariableReplacementInformation::getInformationIndex() const {
    STORM_LOG_ASSERT(isInformationIndex(), "Invalid operation.");
    return boost::get<uint64_t>(data);
}

uint64_t ArrayVariableReplacementInformation::getNumberOfChildren() const {
    STORM_LOG_ASSERT(!isInformationIndex(), "Invalid operation.");
    return boost::get<std::vector<ArrayVariableReplacementInformation>>(data).size();
}

ArrayVariableReplacementInformation const& ArrayVariableReplacementInformation::getChild(uint64_t arrayAccessIndex) const {
    STORM_LOG_ASSERT(!isInformationIndex(), "Invalid operation.");
    return boost::get<std::vector<ArrayVariableReplacementInformation>>(data).at(arrayAccessIndex);
}

uint64_t ArrayVariableReplacementInformation::getVariableInformationIndex(std::vector<uint64_t> arrayIndexVector) const {
    auto const* replInfo = this;
    for (auto const& i : arrayIndexVector) {
        STORM_LOG_ASSERT(!replInfo->isInformationIndex(), "Unexpected array replacement information.");
        STORM_LOG_THROW(
            i < replInfo->getNumberOfChildren(), storm::exceptions::OutOfRangeException,
            "Array access evaluates to array index " << i << " which is out of bounds as the array size is " << replInfo->getNumberOfChildren() << ".");
        replInfo = &replInfo->getChild(i);
    }
    STORM_LOG_ASSERT(replInfo->isInformationIndex(), "Unexpected array replacement information.");
    return replInfo->getInformationIndex();
}
}  // namespace generator
}  // namespace storm
