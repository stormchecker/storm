#pragma once

#include <boost/container/flat_map.hpp>
#include <map>

#include "storm-pars/modelchecker/region/monotonicity/LocalMonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/Order.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm/environment/Environment.h"
#include "storm/expressions/BinaryRelationExpression.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/expressions/RationalFunctionToExpression.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"
#include "storm/utility/solver.h"

namespace storm {
namespace analysis {

template<typename ValueType>
class MonotonicityChecker {
   public:
    typedef typename utility::parametric::VariableType<ValueType>::type VariableType;
    typedef typename utility::parametric::CoefficientType<ValueType>::type CoefficientType;
    typedef typename MonotonicityResult<VariableType>::Monotonicity Monotonicity;
    typedef typename storage::ParameterRegion<ValueType> Region;

    /*!
     * Constructs a new MonotonicityChecker object.
     *
     * @param matrix The Matrix of the model.
     */
    MonotonicityChecker(storage::SparseMatrix<ValueType> const& matrix);

    /*!
     * Checks if a derivative >=0 or/and <=0.
     *
     * @param env The environment (selecting the SMT solver).
     * @param derivative The derivative you want to check.
     * @param reg The region of the parameters.
     * @return Pair of bools, >= 0 and <= 0.
     */
    static std::pair<bool, bool> checkDerivative(Environment const& env, ValueType const& derivative, storage::ParameterRegion<ValueType> const& reg);

    /*!
     * Checks for local monotonicity at the given state.
     *
     * @param env The environment (selecting the SMT solver).
     * @param order The order on which the monotonicity should be checked.
     * @param state The considerd state.
     * @param var The variable in which we check for monotonicity.
     * @param region The region on which we check the monotonicity.
     * @return Incr, Decr, Constant, Unknown or Not
     */
    Monotonicity checkLocalMonotonicity(Environment const& env, std::shared_ptr<Order> const& order, uint_fast64_t state, VariableType const& var,
                                        storage::ParameterRegion<ValueType> const& region);

   private:
    Monotonicity checkTransitionMonRes(Environment const& env, ValueType function, VariableType param, Region region);

    ValueType& getDerivative(ValueType function, VariableType var);

    storage::SparseMatrix<ValueType> matrix;

    boost::container::flat_map<ValueType, boost::container::flat_map<VariableType, ValueType>> derivatives;
};
}  // namespace analysis
}  // namespace storm
