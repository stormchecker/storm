#include "storm-pars/modelchecker/region/monotonicity/AssumptionMaker.h"

#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm {
namespace analysis {
template<typename ValueType, typename ConstantType>
AssumptionMaker<ValueType, ConstantType>::AssumptionMaker(storage::SparseMatrix<ValueType> matrix) : assumptionChecker(matrix) {
    // Intentionally left empty.
}

template<typename ValueType, typename ConstantType>
std::vector<std::pair<Assumption, AssumptionStatus>> AssumptionMaker<ValueType, ConstantType>::createAndCheckAssumptions(
    uint_fast64_t val1, uint_fast64_t val2, std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region) const {
    auto vec1 = std::vector<ConstantType>();
    auto vec2 = std::vector<ConstantType>();
    return createAndCheckAssumptions(val1, val2, order, region, vec1, vec2);
}

template<typename ValueType, typename ConstantType>
std::vector<std::pair<Assumption, AssumptionStatus>> AssumptionMaker<ValueType, ConstantType>::createAndCheckAssumptions(
    uint_fast64_t val1, uint_fast64_t val2, std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region, std::vector<ConstantType> const minValues,
    std::vector<ConstantType> const maxValues) const {
    std::vector<std::pair<Assumption, AssumptionStatus>> result;
    STORM_LOG_INFO("Creating assumptions for " << val1 << " and " << val2);
    STORM_LOG_ASSERT(order->compare(val1, val2) == Order::UNKNOWN, "Expected the given pair to indeed be unordered.");
    auto assumption = createAndCheckAssumption(val1, val2, expressions::RelationType::Greater, order, region, minValues, maxValues);
    if (assumption.second != AssumptionStatus::INVALID) {
        result.push_back(assumption);
        if (assumption.second == AssumptionStatus::VALID) {
            STORM_LOG_ASSERT(createAndCheckAssumption(val2, val1, expressions::RelationType::Greater, order, region, minValues, maxValues).second !=
                                     AssumptionStatus::VALID &&
                                 createAndCheckAssumption(val1, val2, expressions::RelationType::Equal, order, region, minValues, maxValues).second !=
                                     AssumptionStatus::VALID,
                             "At most one of the three candidate assumptions may be valid.");
            STORM_LOG_INFO("Assumption " << assumption.first << "is valid\n");
            return result;
        }
    }
    STORM_LOG_ASSERT(order->compare(val1, val2) == Order::UNKNOWN, "Expected the given pair to indeed be unordered.");
    assumption = createAndCheckAssumption(val2, val1, expressions::RelationType::Greater, order, region, minValues, maxValues);
    if (assumption.second != AssumptionStatus::INVALID) {
        if (assumption.second == AssumptionStatus::VALID) {
            result.clear();
            result.push_back(assumption);
            STORM_LOG_ASSERT(
                createAndCheckAssumption(val1, val2, expressions::RelationType::Equal, order, region, minValues, maxValues).second != AssumptionStatus::VALID,
                "At most one of the three candidate assumptions may be valid.");
            STORM_LOG_INFO("Assumption " << assumption.first << "is valid\n");
            return result;
        }
        result.push_back(assumption);
    }
    STORM_LOG_ASSERT(order->compare(val1, val2) == Order::UNKNOWN, "Expected the given pair to indeed be unordered.");
    assumption = createAndCheckAssumption(val1, val2, expressions::RelationType::Equal, order, region, minValues, maxValues);
    if (assumption.second != AssumptionStatus::INVALID) {
        if (assumption.second == AssumptionStatus::VALID) {
            result.clear();
            result.push_back(assumption);
            STORM_LOG_INFO("Assumption " << assumption.first << "is valid\n");
            return result;
        }
        result.push_back(assumption);
    }
    STORM_LOG_ASSERT(order->compare(val1, val2) == Order::UNKNOWN, "Expected the given pair to indeed be unordered.");
    STORM_LOG_INFO("None of the assumptions is valid, number of possible assumptions:  " << result.size() << '\n');
    return result;
}

template<typename ValueType, typename ConstantType>
void AssumptionMaker<ValueType, ConstantType>::initializeCheckingOnSamples(std::shared_ptr<logic::Formula const> formula,
                                                                           std::shared_ptr<models::sparse::Dtmc<ValueType>> model,
                                                                           storage::ParameterRegion<ValueType> region, uint_fast64_t numberOfSamples) {
    assumptionChecker.initializeCheckingOnSamples(formula, model, region, numberOfSamples);
}

template<typename ValueType, typename ConstantType>
void AssumptionMaker<ValueType, ConstantType>::setSampleValues(std::vector<std::vector<ConstantType>> const& samples) {
    assumptionChecker.setSampleValues(samples);
}

template<typename ValueType, typename ConstantType>
std::pair<Assumption, AssumptionStatus> AssumptionMaker<ValueType, ConstantType>::createAndCheckAssumption(
    uint_fast64_t val1, uint_fast64_t val2, expressions::RelationType relationType, std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region,
    std::vector<ConstantType> const minValues, std::vector<ConstantType> const maxValues) const {
    STORM_LOG_ASSERT(val1 != val2, "An assumption must relate two distinct states.");
    Assumption assumption{val1, val2, relationType};
    AssumptionStatus validationResult = assumptionChecker.validateAssumption(assumption, order, region, minValues, maxValues);
    return {assumption, validationResult};
}

template class AssumptionMaker<RationalFunction, double>;
template class AssumptionMaker<RationalFunction, RationalNumber>;
}  // namespace analysis
}  // namespace storm
