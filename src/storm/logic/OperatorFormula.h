#pragma once

#include <boost/optional.hpp>

#include "storm/logic/Bound.h"
#include "storm/logic/UnaryStateFormula.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/expressions/Expression.h"

#include "storm/utility/constants.h"

namespace storm {
namespace logic {

struct OperatorInformation {
    OperatorInformation(boost::optional<storm::solver::OptimizationDirection> const& optimizationDirection = boost::none,
                        boost::optional<Bound> const& bound = boost::none);

    boost::optional<storm::solver::OptimizationDirection> optimalityType;
    boost::optional<Bound> bound;
};

class OperatorFormula : public UnaryStateFormula {
   public:
    OperatorFormula(std::shared_ptr<Formula const> const& subformula, OperatorInformation const& operatorInformation = OperatorInformation());

    virtual ~OperatorFormula() {
        // Intentionally left empty.
    }

    // Bound-related accessors.
    bool hasBound() const;
    Bound const& getBound() const;
    void setBound(Bound const& newBound);
    void removeBound();

    ComparisonType getComparisonType() const;
    void setComparisonType(ComparisonType newComparisonType);
    storm::expressions::Expression const& getThreshold() const;
    template<typename ValueType>
    ValueType getThresholdAs() const;
    void setThreshold(storm::expressions::Expression const& newThreshold);

    // Optimality-type-related accessors.
    bool hasOptimalityType() const;
    storm::solver::OptimizationDirection const& getOptimalityType() const;
    void setOptimalityType(storm::solver::OptimizationDirection newOptimalityType);
    void removeOptimalityType();
    virtual bool isOperatorFormula() const override;

    OperatorInformation const& getOperatorInformation() const;

    virtual bool hasQualitativeResult() const override;
    virtual bool hasQuantitativeResult() const override;

    virtual void gatherUsedVariables(std::set<storm::expressions::Variable>& usedVariables) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

   protected:
    OperatorInformation operatorInformation;
};
}  // namespace logic
}  // namespace storm
