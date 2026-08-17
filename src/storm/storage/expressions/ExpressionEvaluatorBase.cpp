#include "storm/storage/expressions/ExpressionEvaluatorBase.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace expressions {
template<typename RationalType>
ExpressionEvaluatorBase<RationalType>::ExpressionEvaluatorBase(storm::expressions::ExpressionManager const& manager) : manager(manager.getSharedPointer()) {
    // Intentionally left empty.
}

template<typename RationalType>
storm::expressions::ExpressionManager const& ExpressionEvaluatorBase<RationalType>::getManager() const {
    return *manager;
}

template class ExpressionEvaluatorBase<double>;

template class ExpressionEvaluatorBase<storm::RationalNumber>;
template class ExpressionEvaluatorBase<storm::RationalFunction>;
}  // namespace expressions
}  // namespace storm
