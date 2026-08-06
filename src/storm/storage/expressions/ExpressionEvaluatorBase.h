#pragma once

#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace expressions {
template<typename RationalReturnType>
class ExpressionEvaluatorBase {
   public:
    ExpressionEvaluatorBase(storm::expressions::ExpressionManager const& manager);
    virtual ~ExpressionEvaluatorBase() = default;

    virtual bool asBool(Expression const& expression) const = 0;
    virtual int_fast64_t asInt(Expression const& expression) const = 0;
    virtual RationalReturnType asRational(Expression const& expression) const = 0;

    virtual void setBooleanValue(storm::expressions::Variable const& variable, bool value) = 0;
    virtual void setIntegerValue(storm::expressions::Variable const& variable, int_fast64_t value) = 0;
    virtual void setRationalValue(storm::expressions::Variable const& variable, double value) = 0;

   protected:
    /*!
     * Retrieves the manager associated with this evaluator.
     *
     * @return The manager associated with this evaluator.
     */
    storm::expressions::ExpressionManager const& getManager() const;

   private:
    // The expression manager that is used by this evaluator.
    std::shared_ptr<storm::expressions::ExpressionManager const> manager;
};
}  // namespace expressions
}  // namespace storm
