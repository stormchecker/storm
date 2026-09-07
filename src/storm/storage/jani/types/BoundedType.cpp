#include "BoundedType.h"

#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/jani/visitor/JaniExpressionSubstitutionVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {
BoundedType::BoundedType(const BaseType& type, storm::expressions::Expression const& lowerBound, storm::expressions::Expression const& upperBound)
    : type(type), lowerBound(lowerBound), upperBound(upperBound) {
    // Intentionally left empty
}

bool BoundedType::isBoundedType() const {
    return true;
}

BoundedType::BaseType const& BoundedType::getBaseType() const {
    return type;
}

bool BoundedType::isIntegerType() const {
    return type == BaseType::Int;
}

bool BoundedType::isRealType() const {
    return type == BaseType::Real;
}

bool BoundedType::isNumericalType() const {
    return true;
}

void BoundedType::setLowerBound(storm::expressions::Expression const& expression) {
    this->lowerBound = expression;
}

void BoundedType::setUpperBound(storm::expressions::Expression const& expression) {
    this->upperBound = expression;
}

bool BoundedType::hasLowerBound() const {
    return this->lowerBound.isInitialized();
}

bool BoundedType::hasUpperBound() const {
    return this->upperBound.isInitialized();
}

storm::expressions::Expression& BoundedType::getLowerBound() {
    return this->lowerBound;
}

storm::expressions::Expression& BoundedType::getUpperBound() {
    return this->upperBound;
}

storm::expressions::Expression const& BoundedType::getLowerBound() const {
    return this->lowerBound;
}

storm::expressions::Expression const& BoundedType::getUpperBound() const {
    return this->upperBound;
}

void BoundedType::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                             bool const substituteTranscendentalNumbers) {
    JaniType::substitute(substitution, substituteTranscendentalNumbers);
    if (this->hasLowerBound()) {
        this->setLowerBound(substituteJaniExpression(this->getLowerBound(), substitution, substituteTranscendentalNumbers));
    }
    if (this->hasUpperBound()) {
        this->setUpperBound(substituteJaniExpression(this->getUpperBound(), substitution, substituteTranscendentalNumbers));
    }
}

std::string BoundedType::getStringRepresentation() const {
    switch (type) {
        case BaseType::Real:
            return "bounded real";
        case BaseType::Int:
            return "bounded int";
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unhandled bounded type.");
}

std::unique_ptr<JaniType> BoundedType::clone() const {
    return std::make_unique<BoundedType>(*this);
}

}  // namespace jani
}  // namespace storm
