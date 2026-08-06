#pragma once

#include <boost/container/flat_set.hpp>
#include "storm/logic/Formula.h"
#include "storm/models/sparse/Model.h"
#include "storm/storage/expressions/BinaryRelationExpression.h"
#include "storm/storage/expressions/VariableExpression.h"
#include "storm/utility/Stopwatch.h"

#include "storm-pars/modelchecker/region/monotonicity/AssumptionMaker.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityChecker.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/Order.h"
#include "storm-pars/storage/ParameterRegion.h"

namespace storm {
namespace analysis {
template<typename ValueType, typename ConstantType>
class OrderExtender {
   public:
    typedef typename utility::parametric::CoefficientType<ValueType>::type CoefficientType;
    typedef typename utility::parametric::VariableType<ValueType>::type VariableType;
    typedef typename MonotonicityResult<VariableType>::Monotonicity Monotonicity;

    /*!
     * Constructs a new OrderExtender.
     *
     * @param model The model for which the order should be extended.
     * @param formula The considered formula.
     * @param region The Region of the model's parameters.
     */
    OrderExtender(std::shared_ptr<models::sparse::Model<ValueType>> model, std::shared_ptr<logic::Formula const> formula);

    /*!
     * Constructs a new OrderExtender.
     *
     * @param topStates The top states of the order.
     * @param bottomStates The bottom states of the order.
     * @param matrix The matrix of the considered model.
     */
    OrderExtender(storm::storage::BitVector const& topStates, storm::storage::BitVector const& bottomStates, storm::storage::SparseMatrix<ValueType> matrix);

    /*!
     * Creates an order based on the given formula.
     *
     * @param monRes The monotonicity result so far.
     * @return A triple with a pointer to the order and two states of which the current place in the order
     *         is unknown but needed. When the states have as number the number of states, no states are
     *         unplaced but needed.
     */
    std::tuple<std::shared_ptr<Order>, uint_fast64_t, uint_fast64_t> toOrder(storage::ParameterRegion<ValueType> region,
                                                                             std::shared_ptr<MonotonicityResult<VariableType>> monRes = nullptr);

    /*!
     * Extends the order for the given region.
     *
     * @param order pointer to the order.
     * @param region The region on which the order needs to be extended.
     * @return Two states of which the current place in the order
     *         is unknown but needed. When the states have as number the number of states, no states are
     *         unplaced or needed.
     */
    std::tuple<std::shared_ptr<Order>, uint_fast64_t, uint_fast64_t> extendOrder(std::shared_ptr<Order> order,
                                                                                 storm::storage::ParameterRegion<ValueType> region,
                                                                                 std::shared_ptr<MonotonicityResult<VariableType>> monRes = nullptr,
                                                                                 std::shared_ptr<expressions::BinaryRelationExpression> assumption = nullptr);

    void setMinMaxValues(std::shared_ptr<Order> order, std::vector<ConstantType>&& minValues, std::vector<ConstantType>&& maxValues);
    void setMinValues(std::shared_ptr<Order> order, std::vector<ConstantType>&& minValues);
    void setMaxValues(std::shared_ptr<Order> order, std::vector<ConstantType>&& maxValues);
    void setMinValuesInit(std::vector<ConstantType>&& minValues);
    void setMaxValuesInit(std::vector<ConstantType>&& minValues);

    void setUnknownStates(std::shared_ptr<Order> order, uint_fast64_t state1, uint_fast64_t state2);

    std::pair<uint_fast64_t, uint_fast64_t> getUnknownStates(std::shared_ptr<Order> order) const;
    void setUnknownStates(std::shared_ptr<Order> orderOriginal, std::shared_ptr<Order> orderCopy);
    void copyMinMax(std::shared_ptr<Order> orderOriginal, std::shared_ptr<Order> orderCopy);
    void initializeMinMaxValues(storage::ParameterRegion<ValueType> region);
    void checkParOnStateMonRes(uint_fast64_t s, std::shared_ptr<Order> order, typename OrderExtender<ValueType, ConstantType>::VariableType param,
                               std::shared_ptr<MonotonicityResult<VariableType>> monResult);

    bool isHope(std::shared_ptr<Order> order);

    MonotonicityChecker<ValueType>& getMonotoncityChecker();
    std::vector<std::set<VariableType>> const& getVariablesOccuringAtState();

   private:
    Order::NodeComparison addStatesBasedOnMinMax(std::shared_ptr<Order> order, uint_fast64_t state1, uint_fast64_t state2) const;
    std::tuple<std::shared_ptr<Order>, uint_fast64_t, uint_fast64_t> extendOrder(std::shared_ptr<Order> order,
                                                                                 std::shared_ptr<MonotonicityResult<VariableType>> monRes,
                                                                                 std::shared_ptr<expressions::BinaryRelationExpression> assumption = nullptr);
    std::pair<uint_fast64_t, uint_fast64_t> extendNormal(std::shared_ptr<Order> order, uint_fast64_t currentState, std::vector<uint_fast64_t> const& successors,
                                                         bool allowMerge);
    std::pair<uint_fast64_t, uint_fast64_t> extendByBackwardReasoning(std::shared_ptr<Order> order, uint_fast64_t currentState,
                                                                      std::vector<uint_fast64_t> const& successors, bool allowMerge);
    std::pair<uint_fast64_t, uint_fast64_t> extendByForwardReasoning(std::shared_ptr<Order> order, uint_fast64_t currentState,
                                                                     std::vector<uint_fast64_t> const& successors, bool allowMerge);
    bool extendByAssumption(std::shared_ptr<Order> order, uint_fast64_t state1, uint_fast64_t state2);

    void handleOneSuccessor(std::shared_ptr<Order> order, uint_fast64_t currentState, uint_fast64_t successor);
    void handleAssumption(std::shared_ptr<Order> order, std::shared_ptr<expressions::BinaryRelationExpression> assumption) const;

    std::pair<uint_fast64_t, bool> getNextState(std::shared_ptr<Order> order, uint_fast64_t stateNumber, bool done);
    std::shared_ptr<Order> getBottomTopOrder();

    std::shared_ptr<Order> bottomTopOrder = nullptr;

    std::map<std::shared_ptr<Order>, std::vector<ConstantType>> minValues;
    boost::optional<std::vector<ConstantType>> minValuesInit;
    boost::optional<std::vector<ConstantType>> maxValuesInit;
    std::map<std::shared_ptr<Order>, std::vector<ConstantType>> maxValues;

    storage::SparseMatrix<ValueType> matrix;
    std::shared_ptr<models::sparse::Model<ValueType>> model;

    std::map<uint_fast64_t, std::vector<uint_fast64_t>> stateMap;
    std::map<std::shared_ptr<Order>, std::pair<uint_fast64_t, uint_fast64_t>> unknownStatesMap;

    std::map<std::shared_ptr<Order>, bool> usePLA;
    std::map<std::shared_ptr<Order>, bool> continueExtending;
    bool cyclic;

    std::shared_ptr<logic::Formula const> formula;

    storage::ParameterRegion<ValueType> region;

    uint_fast64_t numberOfStates;

    analysis::AssumptionMaker<ValueType, ConstantType>* assumptionMaker;

    boost::container::flat_set<uint_fast64_t> nonParametricStates;

    std::map<VariableType, std::vector<uint_fast64_t>> occuringStatesAtVariable;
    std::vector<std::set<VariableType>> occuringVariablesAtState;
    MonotonicityChecker<ValueType> monotonicityChecker;
};
}  // namespace analysis
}  // namespace storm
