#pragma once

#include "Order.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm/environment/Environment.h"
#include "storm/logic/Formula.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/expressions/BinaryRelationExpression.h"

namespace storm {
namespace analysis {
/*!
 * Constants for status of assumption
 */
enum AssumptionStatus {
    VALID,
    INVALID,
    UNKNOWN,
};

template<typename ValueType, typename ConstantType>
class AssumptionChecker {
   public:
    typedef typename utility::parametric::VariableType<ValueType>::type VariableType;
    typedef typename utility::parametric::CoefficientType<ValueType>::type CoefficientType;

    /*!
     * Constructs an AssumptionChecker.
     *
     * @param matrix The matrix of the considered model.
     */
    AssumptionChecker(storage::SparseMatrix<ValueType> matrix);

    /*!
     * Constructs an AssumptionChecker based on the number of samples, for the given formula and model.
     *
     * @param formula The formula to check.
     * @param model The mdp model to check the formula on.
     * @param numberOfSamples Number of sample points.
     */
    AssumptionChecker(std::shared_ptr<logic::Formula const> formula, std::shared_ptr<models::sparse::Mdp<ValueType>> model,
                      uint_fast64_t const numberOfSamples);

    /*!
     * Initializes the given number of sample points for a given model, formula and region.
     *
     * @param formula The formula to compute the samples for.
     * @param model The considered model.
     * @param region The region of the model's parameters.
     * @param numberOfSamples Number of sample points.
     */
    void initializeCheckingOnSamples(std::shared_ptr<logic::Formula const> formula, std::shared_ptr<models::sparse::Dtmc<ValueType>> model,
                                     storage::ParameterRegion<ValueType> region, uint_fast64_t numberOfSamples);

    /*!
     * Sets the sample values to the given vector and useSamples to true.
     *
     * @param samples The new value for samples.
     */
    void setSampleValues(std::vector<std::vector<ConstantType>> samples);

    /*!
     * Tries to validate an assumption based on the order and underlying transition matrix.
     *
     * @param assumption The assumption to validate.
     * @param order The order.
     * @param region The region of the considered model.
     * @return AssumptionStatus::VALID, or AssumptionStatus::UNKNOWN, or AssumptionStatus::INVALID
     */
    AssumptionStatus validateAssumption(uint_fast64_t val1, uint_fast64_t val2, std::shared_ptr<expressions::BinaryRelationExpression> assumption,
                                        std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region, std::vector<ConstantType> const minValues,
                                        std::vector<ConstantType> const maxValue) const;
    AssumptionStatus validateAssumption(std::shared_ptr<expressions::BinaryRelationExpression> assumption, std::shared_ptr<Order> order,
                                        storage::ParameterRegion<ValueType> region) const;

   private:
    bool useSamples;

    std::vector<std::vector<ConstantType>> samples;

    storage::SparseMatrix<ValueType> matrix;

    AssumptionStatus validateAssumptionSMTSolver(uint_fast64_t val1, uint_fast64_t val2, std::shared_ptr<expressions::BinaryRelationExpression> assumption,
                                                 std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region,
                                                 std::vector<ConstantType> const minValues, std::vector<ConstantType> const maxValue) const;

    AssumptionStatus checkOnSamples(std::shared_ptr<expressions::BinaryRelationExpression> assumption) const;
};
}  // namespace analysis
}  // namespace storm
