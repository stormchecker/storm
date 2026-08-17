#include "storm-pars/modelchecker/region/monotonicity/AssumptionMaker.h"

#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm {
namespace analysis {
template<typename ValueType, typename ConstantType>
AssumptionMaker<ValueType, ConstantType>::AssumptionMaker(storage::SparseMatrix<ValueType> matrix) : assumptionChecker(matrix) {
    numberOfStates = matrix.getColumnCount();
    expressionManager = std::make_shared<expressions::ExpressionManager>(expressions::ExpressionManager());
    for (uint_fast64_t i = 0; i < this->numberOfStates; ++i) {
        expressionManager->declareRationalVariable(std::to_string(i));
    }
}

template<typename ValueType, typename ConstantType>
std::map<std::shared_ptr<expressions::BinaryRelationExpression>, AssumptionStatus> AssumptionMaker<ValueType, ConstantType>::createAndCheckAssumptions(
    uint_fast64_t val1, uint_fast64_t val2, std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region) const {
    auto vec1 = std::vector<ConstantType>();
    auto vec2 = std::vector<ConstantType>();
    return createAndCheckAssumptions(val1, val2, order, region, vec1, vec2);
}

template<typename ValueType, typename ConstantType>
std::map<std::shared_ptr<expressions::BinaryRelationExpression>, AssumptionStatus> AssumptionMaker<ValueType, ConstantType>::createAndCheckAssumptions(
    uint_fast64_t val1, uint_fast64_t val2, std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region, std::vector<ConstantType> const minValues,
    std::vector<ConstantType> const maxValues) const {
    std::map<std::shared_ptr<expressions::BinaryRelationExpression>, AssumptionStatus> result;
    STORM_LOG_INFO("Creating assumptions for " << val1 << " and " << val2);
    STORM_LOG_ASSERT(order->compare(val1, val2) == Order::UNKNOWN, "Order should be UNKNOWN at start.");
    auto assumption = createAndCheckAssumption(val1, val2, expressions::RelationType::Greater, order, region, minValues, maxValues);
    if (assumption.second != AssumptionStatus::INVALID) {
        result.insert(assumption);
        if (assumption.second == AssumptionStatus::VALID) {
            STORM_LOG_ASSERT(createAndCheckAssumption(val2, val1, expressions::RelationType::Greater, order, region, minValues, maxValues).second !=
                                     AssumptionStatus::VALID &&
                                 createAndCheckAssumption(val1, val2, expressions::RelationType::Equal, order, region, minValues, maxValues).second !=
                                     AssumptionStatus::VALID,
                             "Assumption " << val2 << " >= " << val1 << " should not be valid.");
            STORM_LOG_INFO("Assumption " << assumption.first << "is valid\n");
            return result;
        }
    }
    STORM_LOG_ASSERT(order->compare(val1, val2) == Order::UNKNOWN, "Order should be UNKNOWN.");
    assumption = createAndCheckAssumption(val2, val1, expressions::RelationType::Greater, order, region, minValues, maxValues);
    if (assumption.second != AssumptionStatus::INVALID) {
        if (assumption.second == AssumptionStatus::VALID) {
            result.clear();
            result.insert(assumption);
            STORM_LOG_ASSERT(
                createAndCheckAssumption(val1, val2, expressions::RelationType::Equal, order, region, minValues, maxValues).second != AssumptionStatus::VALID,
                "Assumption " << val1 << " == " << val2 << " should not be valid.");
            STORM_LOG_INFO("Assumption " << assumption.first << "is valid\n");
            return result;
        }
        result.insert(assumption);
    }
    STORM_LOG_ASSERT(order->compare(val1, val2) == Order::UNKNOWN, "Order should be UNKNOWN.");
    assumption = createAndCheckAssumption(val1, val2, expressions::RelationType::Equal, order, region, minValues, maxValues);
    if (assumption.second != AssumptionStatus::INVALID) {
        if (assumption.second == AssumptionStatus::VALID) {
            result.clear();
            result.insert(assumption);
            STORM_LOG_INFO("Assumption " << assumption.first << "is valid\n");
            return result;
        }
        result.insert(assumption);
    }
    STORM_LOG_ASSERT(order->compare(val1, val2) == Order::UNKNOWN, "Order should be UNKNOWN.");
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
std::pair<std::shared_ptr<expressions::BinaryRelationExpression>, AssumptionStatus> AssumptionMaker<ValueType, ConstantType>::createAndCheckAssumption(
    uint_fast64_t val1, uint_fast64_t val2, expressions::RelationType relationType, std::shared_ptr<Order> order, storage::ParameterRegion<ValueType> region,
    std::vector<ConstantType> const minValues, std::vector<ConstantType> const maxValues) const {
    STORM_LOG_ASSERT(val1 != val2, "Values should be different.");
    expressions::Variable var1 = expressionManager->getVariable(std::to_string(val1));
    expressions::Variable var2 = expressionManager->getVariable(std::to_string(val2));
    auto assumption = std::make_shared<expressions::BinaryRelationExpression>(
        expressions::BinaryRelationExpression(*expressionManager, expressionManager->getBooleanType(), var1.getExpression().getBaseExpressionPointer(),
                                              var2.getExpression().getBaseExpressionPointer(), relationType));
    AssumptionStatus validationResult = assumptionChecker.validateAssumption(val1, val2, assumption, order, region, minValues, maxValues);
    return std::pair<std::shared_ptr<expressions::BinaryRelationExpression>, AssumptionStatus>(assumption, validationResult);
}

template class AssumptionMaker<RationalFunction, double>;
template class AssumptionMaker<RationalFunction, RationalNumber>;
}  // namespace analysis
}  // namespace storm
