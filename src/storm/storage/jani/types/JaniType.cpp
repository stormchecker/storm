#include "JaniType.h"

#include <ostream>

#include "storm/storage/jani/types/AllJaniTypes.h"

namespace storm {
namespace jani {
JaniType::JaniType() {
    // Intentionally left empty
}

bool JaniType::isBasicType() const {
    return false;
}

bool JaniType::isBoundedType() const {
    return false;
}

bool JaniType::isArrayType() const {
    return false;
}

bool JaniType::isClockType() const {
    return false;
}

bool JaniType::isContinuousType() const {
    return false;
}

BasicType const& JaniType::asBasicType() const {
    return dynamic_cast<BasicType const&>(*this);
}

BasicType& JaniType::asBasicType() {
    return dynamic_cast<BasicType&>(*this);
}

BoundedType const& JaniType::asBoundedType() const {
    return dynamic_cast<BoundedType const&>(*this);
}

BoundedType& JaniType::asBoundedType() {
    return dynamic_cast<BoundedType&>(*this);
}

ArrayType const& JaniType::asArrayType() const {
    return dynamic_cast<ArrayType const&>(*this);
}

ArrayType& JaniType::asArrayType() {
    return dynamic_cast<ArrayType&>(*this);
}

ClockType const& JaniType::asClockType() const {
    return dynamic_cast<ClockType const&>(*this);
}

ClockType& JaniType::asClockType() {
    return dynamic_cast<ClockType&>(*this);
}

ContinuousType const& JaniType::asContinuousType() const {
    return dynamic_cast<ContinuousType const&>(*this);
}

ContinuousType& JaniType::asContinuousType() {
    return dynamic_cast<ContinuousType&>(*this);
}

void JaniType::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& /*substitution*/,
                          bool const /*substituteTranscendentalNumbers*/) {
    // intentionally left empty
}

std::ostream& operator<<(std::ostream& stream, JaniType const& type) {
    stream << type.getStringRepresentation();
    return stream;
}
}  // namespace jani
}  // namespace storm
