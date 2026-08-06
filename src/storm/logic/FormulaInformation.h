#pragma once

namespace storm {
namespace logic {

class FormulaInformation {
   public:
    FormulaInformation();
    FormulaInformation(FormulaInformation const& other) = default;
    FormulaInformation(FormulaInformation&& other) = default;
    FormulaInformation& operator=(FormulaInformation const& other) = default;
    FormulaInformation& operator=(FormulaInformation&& other) = default;

    bool containsRewardOperator() const;
    bool containsNextFormula() const;
    bool containsBoundedUntilFormula() const;
    bool containsCumulativeRewardFormula() const;
    bool containsRewardBoundedFormula() const;
    bool containsLongRunFormula() const;
    bool containsDiscountFormula() const;

    /*!
     * @return true iff the formula contains nested temporal operators and/or boolean combinations of path formulas (e.g. '"safe" & F "goal"')
     */
    bool containsComplexPathFormula() const;

    FormulaInformation join(FormulaInformation const& other);

    FormulaInformation& setContainsRewardOperator(bool newValue = true);
    FormulaInformation& setContainsNextFormula(bool newValue = true);
    FormulaInformation& setContainsBoundedUntilFormula(bool newValue = true);
    FormulaInformation& setContainsCumulativeRewardFormula(bool newValue = true);
    FormulaInformation& setContainsRewardBoundedFormula(bool newValue = true);
    FormulaInformation& setContainsLongRunFormula(bool newValue = true);
    FormulaInformation& setContainsComplexPathFormula(bool newValue = true);
    FormulaInformation& setContainsDiscountFormula(bool newValue = true);

   private:
    bool mContainsRewardOperator;
    bool mContainsNextFormula;
    bool mContainsBoundedUntilFormula;
    bool mContainsCumulativeRewardFormula;
    bool mContainsRewardBoundedFormula;
    bool mContainsLongRunFormula;
    bool mContainsComplexPathFormula;
    bool mContainsDiscountFormula;
};

}  // namespace logic
}  // namespace storm
