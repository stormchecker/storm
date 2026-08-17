#include "storm/logic/FormulaInformation.h"

namespace storm {
namespace logic {
FormulaInformation::FormulaInformation()
    : mContainsRewardOperator(false),
      mContainsNextFormula(false),
      mContainsBoundedUntilFormula(false),
      mContainsCumulativeRewardFormula(false),
      mContainsRewardBoundedFormula(false),
      mContainsLongRunFormula(false),
      mContainsComplexPathFormula(false),
      mContainsDiscountFormula(false) {
    // Intentionally left empty
}

bool FormulaInformation::containsRewardOperator() const {
    return this->mContainsRewardOperator;
}

bool FormulaInformation::containsNextFormula() const {
    return this->mContainsNextFormula;
}

bool FormulaInformation::containsBoundedUntilFormula() const {
    return this->mContainsBoundedUntilFormula;
}

bool FormulaInformation::containsCumulativeRewardFormula() const {
    return this->mContainsCumulativeRewardFormula;
}

bool FormulaInformation::containsRewardBoundedFormula() const {
    return this->mContainsRewardBoundedFormula;
}

bool FormulaInformation::containsLongRunFormula() const {
    return this->mContainsLongRunFormula;
}

bool FormulaInformation::containsComplexPathFormula() const {
    return this->mContainsComplexPathFormula;
}

bool FormulaInformation::containsDiscountFormula() const {
    return this->mContainsDiscountFormula;
}

FormulaInformation FormulaInformation::join(FormulaInformation const& other) {
    FormulaInformation result;
    result.mContainsRewardOperator = this->containsRewardOperator() || other.containsRewardOperator();
    result.mContainsNextFormula = this->containsNextFormula() || other.containsNextFormula();
    result.mContainsBoundedUntilFormula = this->containsBoundedUntilFormula() || other.containsBoundedUntilFormula();
    result.mContainsCumulativeRewardFormula = this->containsCumulativeRewardFormula() || other.containsCumulativeRewardFormula();
    result.mContainsRewardBoundedFormula = this->containsRewardBoundedFormula() || other.containsRewardBoundedFormula();
    result.mContainsLongRunFormula = this->containsLongRunFormula() || other.containsLongRunFormula();
    result.mContainsComplexPathFormula = this->containsComplexPathFormula() || other.containsComplexPathFormula();
    result.mContainsDiscountFormula = this->containsDiscountFormula() || other.containsDiscountFormula();
    return result;
}

FormulaInformation& FormulaInformation::setContainsRewardOperator(bool newValue) {
    this->mContainsRewardOperator = newValue;
    return *this;
}

FormulaInformation& FormulaInformation::setContainsNextFormula(bool newValue) {
    this->mContainsNextFormula = newValue;
    return *this;
}

FormulaInformation& FormulaInformation::setContainsBoundedUntilFormula(bool newValue) {
    this->mContainsBoundedUntilFormula = newValue;
    return *this;
}

FormulaInformation& FormulaInformation::setContainsCumulativeRewardFormula(bool newValue) {
    this->mContainsCumulativeRewardFormula = newValue;
    return *this;
}

FormulaInformation& FormulaInformation::setContainsRewardBoundedFormula(bool newValue) {
    this->mContainsRewardBoundedFormula = newValue;
    return *this;
}

FormulaInformation& FormulaInformation::setContainsLongRunFormula(bool newValue) {
    this->mContainsLongRunFormula = newValue;
    return *this;
}

FormulaInformation& FormulaInformation::setContainsComplexPathFormula(bool newValue) {
    this->mContainsComplexPathFormula = newValue;
    return *this;
}

FormulaInformation& FormulaInformation::setContainsDiscountFormula(bool newValue) {
    this->mContainsDiscountFormula = newValue;
    return *this;
}

}  // namespace logic
}  // namespace storm
