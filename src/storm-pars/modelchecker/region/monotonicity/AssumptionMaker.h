#pragma once

#include "storm/storage/SparseMatrix.h"
#include "storm/storage/expressions/BinaryRelationType.h"

#include "storm-pars/modelchecker/region/monotonicity/Assumption.h"
#include "storm-pars/modelchecker/region/monotonicity/AssumptionChecker.h"
#include "storm-pars/modelchecker/region/monotonicity/Order.h"

namespace storm {
namespace analysis {

template<typename ValueType, typename ConstantType>
class AssumptionMaker {
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
     * @return At most three (assumption, status) candidates, in the order they were tried
     *         (val1 > val2, val2 > val1, val1 == val2).
     */
    std::vector<std::pair<Assumption, AssumptionStatus>> createAndCheckAssumptions(uint_fast64_t val1, uint_fast64_t val2, std::shared_ptr<Order> order,
                                                                                   storage::ParameterRegion<ValueType> region) const;
    std::vector<std::pair<Assumption, AssumptionStatus>> createAndCheckAssumptions(uint_fast64_t val1, uint_fast64_t val2, std::shared_ptr<Order> order,
                                                                                   storage::ParameterRegion<ValueType> region,
                                                                                   std::vector<ConstantType> const minValues,
                                                                                   std::vector<ConstantType> const maxValue) const;

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
    std::pair<Assumption, AssumptionStatus> createAndCheckAssumption(uint_fast64_t val1, uint_fast64_t val2, expressions::RelationType relationType,
                                                                     std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region,
                                                                     std::vector<ConstantType> const minValues, std::vector<ConstantType> const maxValue) const;

    AssumptionChecker<ValueType, ConstantType> assumptionChecker;
};
}  // namespace analysis
}  // namespace storm
