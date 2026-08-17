#pragma once

#include <boost/optional.hpp>

#include "storm/logic/BinaryPathFormula.h"

#include "storm/logic/TimeBound.h"
#include "storm/logic/TimeBoundType.h"

namespace storm {
namespace logic {
class BoundedUntilFormula : public PathFormula {
   public:
    BoundedUntilFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula,
                        boost::optional<TimeBound> const& lowerBound, boost::optional<TimeBound> const& upperBound,
                        TimeBoundReference const& timeBoundReference);
    BoundedUntilFormula(std::shared_ptr<Formula const> const& leftSubformula, std::shared_ptr<Formula const> const& rightSubformula,
                        std::vector<boost::optional<TimeBound>> const& lowerBounds, std::vector<boost::optional<TimeBound>> const& upperBounds,
                        std::vector<TimeBoundReference> const& timeBoundReferences);
    BoundedUntilFormula(std::vector<std::shared_ptr<Formula const>> const& leftSubformulas, std::vector<std::shared_ptr<Formula const>> const& rightSubformulas,
                        std::vector<boost::optional<TimeBound>> const& lowerBounds, std::vector<boost::optional<TimeBound>> const& upperBounds,
                        std::vector<TimeBoundReference> const& timeBoundReferences);

    virtual bool isBoundedUntilFormula() const override;

    virtual bool isProbabilityPathFormula() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual void gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const override;
    virtual void gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const override;
    virtual void gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const override;
    virtual void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

    virtual bool hasQualitativeResult() const override;
    virtual bool hasQuantitativeResult() const override;

    bool isMultiDimensional() const;
    bool hasMultiDimensionalSubformulas() const;
    unsigned getDimension() const;

    Formula const& getLeftSubformula() const;
    Formula const& getLeftSubformula(unsigned i) const;
    Formula const& getRightSubformula() const;
    Formula const& getRightSubformula(unsigned i) const;

    TimeBoundReference const& getTimeBoundReference(unsigned i = 0) const;

    bool isLowerBoundStrict(unsigned i = 0) const;
    bool hasLowerBound() const;
    bool hasLowerBound(unsigned i) const;
    bool hasIntegerLowerBound(unsigned i = 0) const;

    bool isUpperBoundStrict(unsigned i = 0) const;
    bool hasUpperBound() const;
    bool hasUpperBound(unsigned i) const;
    bool hasIntegerUpperBound(unsigned i = 0) const;

    storm::expressions::Expression const& getLowerBound(unsigned i = 0) const;
    storm::expressions::Expression const& getUpperBound(unsigned i = 0) const;

    std::optional<TimeBound> getLowerBoundAsOptionalTimeBound(unsigned i = 0) const;
    std::optional<TimeBound> getUpperBoundAsOptionalTimeBound(unsigned i = 0) const;

    template<typename ValueType>
    ValueType getLowerBound(unsigned i = 0) const;

    template<typename ValueType>
    ValueType getUpperBound(unsigned i = 0) const;

    template<typename ValueType>
    ValueType getNonStrictUpperBound(unsigned i = 0) const;

    template<typename ValueType>
    ValueType getNonStrictLowerBound(unsigned i = 0) const;

    std::shared_ptr<BoundedUntilFormula const> restrictToDimension(unsigned i) const;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

   private:
    static void checkNoVariablesInBound(storm::expressions::Expression const& bound);

    std::vector<std::shared_ptr<Formula const>> leftSubformula;
    std::vector<std::shared_ptr<Formula const>> rightSubformula;
    std::vector<TimeBoundReference> timeBoundReference;
    std::vector<boost::optional<TimeBound>> lowerBound;
    std::vector<boost::optional<TimeBound>> upperBound;
};
}  // namespace logic
}  // namespace storm
