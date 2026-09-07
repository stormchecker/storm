#include "ArrayType.h"

#include <string>

namespace storm {
namespace jani {
ArrayType::ArrayType(JaniType const& baseType) : ArrayType(baseType.clone()) {
    // Intentionally left empty
}

ArrayType::ArrayType(std::unique_ptr<JaniType>&& baseType) : baseType(std::move(baseType)) {
    // Intentionally left empty
}

bool ArrayType::isArrayType() const {
    return true;
}

JaniType& ArrayType::getBaseType() {
    return *baseType;
}

JaniType const& ArrayType::getBaseType() const {
    return *baseType;
}

JaniType const& ArrayType::getBaseTypeRecursive() const {
    if (getBaseType().isArrayType()) {
        return getBaseType().asArrayType().getBaseTypeRecursive();
    } else {
        return getBaseType();
    }
}

uint64_t ArrayType::getNestingDegree() const {
    if (getBaseType().isArrayType()) {
        return getBaseType().asArrayType().getNestingDegree() + 1;
    } else {
        return 1;
    }
}

std::string ArrayType::getStringRepresentation() const {
    return "array[" + getBaseType().getStringRepresentation() + "]";
}

void ArrayType::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                           bool const substituteTranscendentalNumbers) {
    JaniType::substitute(substitution, substituteTranscendentalNumbers);
    baseType->substitute(substitution, substituteTranscendentalNumbers);
}

std::unique_ptr<JaniType> ArrayType::clone() const {
    return std::make_unique<ArrayType>(baseType->clone());
}

}  // namespace jani
}  // namespace storm