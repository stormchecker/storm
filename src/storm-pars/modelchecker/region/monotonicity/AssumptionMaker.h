#pragma once

#include "AssumptionChecker.h"
#include "Order.h"

#include "storm/storage/SparseMatrix.h"
#include "storm/storage/expressions/BinaryRelationExpression.h"
#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace analysis {

template<typename ValueType, typename ConstantType>
class AssumptionMaker {
    typedef std::shared_ptr<expressions::BinaryRelationExpression> AssumptionType;

   public:
    /*!
     * Constructs AssumptionMaker based on the matrix of the model.
     *
     * @param matrix The matrix of the model.
     */
    AssumptionMaker(storage::SparseMatrix<ValueType> matrix);

    /*!
     * Creates assumptions, and checks them, only VALID and UNKNOWN assumptions are returned.
     * If one assumption is VALID, this assumption will be returned as only assumption.
     * Possible results: AssumptionStatus::VALID, AssumptionStatus::UNKNOWN.
     *
     * @param val1 First state number.
     * @param val2 Second state number.
     * @param order The order on which the assumptions are checked.
     * @param region The region for the parameters.
     * @return Map with at most three assumptions, and the validation.
     */
    std::map<std::shared_ptr<expressions::BinaryRelationExpression>, AssumptionStatus> createAndCheckAssumptions(
        uint_fast64_t val1, uint_fast64_t val2, std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region) const;
    std::map<std::shared_ptr<expressions::BinaryRelationExpression>, AssumptionStatus> createAndCheckAssumptions(
        uint_fast64_t val1, uint_fast64_t val2, std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region,
        std::vector<ConstantType> const minValues, std::vector<ConstantType> const maxValue) const;

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
     * Sets the sample values to the given vector.
     *
     * @param samples The new value for samples.
     */
    void setSampleValues(std::vector<std::vector<ConstantType>> const& samples);

   private:
    std::pair<std::shared_ptr<expressions::BinaryRelationExpression>, AssumptionStatus> createAndCheckAssumption(
        uint_fast64_t val1, uint_fast64_t val2, expressions::RelationType relationType, std::shared_ptr<Order> order,
        storage::ParameterRegion<ValueType> region, std::vector<ConstantType> const minValues, std::vector<ConstantType> const maxValue) const;

    AssumptionChecker<ValueType, ConstantType> assumptionChecker;

    std::shared_ptr<expressions::ExpressionManager> expressionManager;

    uint_fast64_t numberOfStates;
};
}  // namespace analysis
}  // namespace storm
