#include "storm-pars/modelchecker/region/monotonicity/OrderBasedMonotonicityBackend.h"

#include "storm-pars/modelchecker/region/AnnotatedRegion.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/macros.h"

namespace storm::modelchecker {

namespace detail {

template<typename ParametricType, typename ConstantType>
std::shared_ptr<storm::analysis::Order> extendOrder(storm::analysis::OrderExtender<ParametricType, ConstantType>& orderExtender,
                                                    std::shared_ptr<storm::analysis::Order> order, storm::storage::ParameterRegion<ParametricType> region) {
    auto [orderPtr, unknState1, unknState2] = orderExtender.extendOrder(order, region);
    order = orderPtr;
    if (unknState1 != order->getNumberOfStates()) {
        orderExtender.setUnknownStates(order, unknState1, unknState2);
    }
    return order;
}

template<typename ParametricType, typename ConstantType>
void extendLocalMonotonicityResult(
    storm::storage::ParameterRegion<ParametricType> const& region, std::shared_ptr<storm::analysis::Order> const& order,
    storm::analysis::LocalMonotonicityResult<typename storm::storage::ParameterRegion<ParametricType>::VariableType>& localMonotonicityResult,
    storm::analysis::MonotonicityChecker<ParametricType>& monotonicityChecker,
    storm::transformer::ParameterLifter<ParametricType, ConstantType> const& parameterLifter) {
    auto state = order->getNextDoneState(-1);
    auto const& variablesAtState = parameterLifter.getOccurringVariablesAtState();  // TODO: this might be possible via the OrderExtender as well
    while (state != order->getNumberOfStates()) {
        if (localMonotonicityResult.getMonotonicity(state) == nullptr) {
            auto variables = variablesAtState[state];
            if (variables.size() == 0 || order->isBottomState(state) || order->isTopState(state)) {
                localMonotonicityResult.setConstant(state);
            } else {
                for (auto const& var : variables) {
                    auto monotonicity = localMonotonicityResult.getMonotonicity(state, var);
                    if (!storm::analysis::isMonotone(monotonicity)) {
                        monotonicity = monotonicityChecker.checkLocalMonotonicity(order, state, var, region);
                        if (storm::analysis::isMonotone(monotonicity)) {
                            localMonotonicityResult.setMonotonicity(state, var, monotonicity);
                        } else {
                            // TODO: Skip for now?
                        }
                    }
                }
            }
        }
        state = order->getNextDoneState(state);
    }
    auto const& statesAtVariable = parameterLifter.getOccuringStatesAtVariable();
    bool allDone = true;
    for (auto const& entry : statesAtVariable) {
        auto states = entry.second;
        auto var = entry.first;
        bool done = true;
        for (auto const& state : states) {
            done &= order->contains(state) && localMonotonicityResult.getMonotonicity(state, var) != storm::analysis::MonotonicityKind::Unknown;
            if (!done) {
                break;
            }
        }

        allDone &= done;
        if (done) {
            localMonotonicityResult.getGlobalMonotonicityResult()->setDoneForVar(var);
        }
    }
    if (allDone) {
        localMonotonicityResult.setDone();
        while (order->existsNextState()) {
            // Simply add the states we couldn't add sofar between =) and =( as we could find local monotonicity for all parametric states
            order->add(order->getNextStateNumber().second);
        }
        STORM_LOG_ASSERT(order->getDoneBuilding(), "Order should be done building.");
    }
}
}  // namespace detail

template<typename ParametricType, typename ConstantType>
OrderBasedMonotonicityBackend<ParametricType, ConstantType>::OrderBasedMonotonicityBackend(bool useOnlyGlobal, bool useBounds)
    : useOnlyGlobal(useOnlyGlobal), useBounds(useBounds) {
    // Intentioanlly left empty
}

template<typename ParametricType, typename ConstantType>
bool OrderBasedMonotonicityBackend<ParametricType, ConstantType>::requiresInteractionWithRegionModelChecker() const {
    return true;
}

template<typename ParametricType, typename ConstantType>
bool OrderBasedMonotonicityBackend<ParametricType, ConstantType>::recommendModelSimplifications() const {
    return false;
}

template<typename ParametricType, typename ConstantType>
void OrderBasedMonotonicityBackend<ParametricType, ConstantType>::initializeMonotonicity(storm::Environment const& env,
                                                                                         AnnotatedRegion<ParametricType>& region) {
    if (useBounds) {
        STORM_LOG_ASSERT(plaBoundFunction, "PLA bound function not registered.");
        orderExtender->setMaxValuesInit(plaBoundFunction(env, region, storm::solver::OptimizationDirection::Maximize));
        orderExtender->setMaxValuesInit(plaBoundFunction(env, region, storm::solver::OptimizationDirection::Minimize));
    }
    typename MonotonicityAnnotation<ParametricType>::OrderBasedMonotonicityAnnotation annotation;
    annotation.stateOrder = detail::extendOrder(*this->orderExtender, nullptr, region.region);
    annotation.localMonotonicityResult = std::make_shared<storm::analysis::LocalMonotonicityResult<VariableType>>(annotation.stateOrder->getNumberOfStates());

    for (auto& [var, kind] : this->globallyKnownMonotonicityInformation) {
        if (kind == MonotonicityKind::Incr || kind == MonotonicityKind::Constant) {
            annotation.localMonotonicityResult->setMonotoneIncreasing(var);
        } else if (kind == MonotonicityKind::Decr) {
            annotation.localMonotonicityResult->setMonotoneDecreasing(var);
        }
    }

    detail::extendLocalMonotonicityResult(region.region, annotation.stateOrder, *annotation.localMonotonicityResult, *this->monotonicityChecker,
                                          *this->parameterLifterRef);
    region.monotonicityAnnotation.data = annotation;
}

template<typename ParametricType, typename ConstantType>
void OrderBasedMonotonicityBackend<ParametricType, ConstantType>::updateMonotonicity(storm::Environment const& env, AnnotatedRegion<ParametricType>& region) {
    auto annotation = region.monotonicityAnnotation.getOrderBasedMonotonicityAnnotation();
    STORM_LOG_ASSERT(annotation.has_value(), "Order-based monotonicity annotation must be present.");
    // Find out if we need to copy the order as it might be shared among subregions.
    // Copy order only if it will potentially change and if it is shared with another region
    bool const changeOrder = !annotation->stateOrder->getDoneBuilding() && orderExtender->isHope(annotation->stateOrder);
    if (changeOrder && annotation->stateOrder.use_count() > 1) {
        // TODO: orderExtender currently uses shared_ptr<Order> which likely interferes with the use_count() > 1 check above
        // TODO: Make sure that only annotated regions own the order
        auto newOrder = annotation->stateOrder->copy();
        orderExtender->setUnknownStates(annotation->stateOrder, newOrder);
        orderExtender->copyMinMax(annotation->stateOrder, newOrder);
        annotation->stateOrder = newOrder;
    }
    if (changeOrder) {
        detail::extendOrder(*this->orderExtender, annotation->stateOrder, region.region);
    }
    // Similarly handle local monotonicity result
    bool const changeLocalMonotonicity = changeOrder && !annotation->localMonotonicityResult->isDone();
    if (changeLocalMonotonicity && annotation->localMonotonicityResult.use_count() > 1) {
        // TODO: Make sure that only annotated regions own the localMonotonicityResult
        annotation->localMonotonicityResult = annotation->localMonotonicityResult->copy();
    }
    if (changeLocalMonotonicity) {
        detail::extendLocalMonotonicityResult(region.region, annotation->stateOrder, *annotation->localMonotonicityResult, *this->monotonicityChecker,
                                              *this->parameterLifterRef);
    }
}

template<typename ParametricType, typename ConstantType>
void OrderBasedMonotonicityBackend<ParametricType, ConstantType>::updateMonotonicityBeforeSplitting(storm::Environment const& env,
                                                                                                    AnnotatedRegion<ParametricType>& region) {
    auto annotation = region.monotonicityAnnotation.getOrderBasedMonotonicityAnnotation();
    STORM_LOG_ASSERT(annotation.has_value(), "Order-based monotonicity annotation must be present.");
    if (useBounds && !annotation->stateOrder->getDoneBuilding()) {
        STORM_LOG_ASSERT(plaBoundFunction, "PLA bound function not registered.");
        // TODO: Can re-use bounds from performed PLA call before splitting is triggered. Maybe allow some caching in the PLA checker?
        orderExtender->setMinMaxValues(annotation->stateOrder, plaBoundFunction(env, region, storm::solver::OptimizationDirection::Minimize),
                                       plaBoundFunction(env, region, storm::solver::OptimizationDirection::Maximize));
    }
}

template<typename ParametricType, typename ConstantType>
std::map<typename OrderBasedMonotonicityBackend<ParametricType, ConstantType>::VariableType,
         typename OrderBasedMonotonicityBackend<ParametricType, ConstantType>::MonotonicityKind>
OrderBasedMonotonicityBackend<ParametricType, ConstantType>::getOptimisticMonotonicityApproximation(AnnotatedRegion<ParametricType> const& region) {
    // TODO: Old implementation had checkForPossibleMonotonicity to determine this based on samples. Consider re-adding that.
    // https://github.com/stormchecker/storm/blob/5c89b2d1051b3abdbb8659101d60331c680b7050/src/storm-pars/modelchecker/region/SparseParameterLiftingModelChecker.cpp#L622
    // For now, we just do this based on known global monotonicity.
    return MonotonicityBackend<ParametricType>::getOptimisticMonotonicityApproximation(region);
}

template<typename ParametricType, typename ConstantType>
void OrderBasedMonotonicityBackend<ParametricType, ConstantType>::initializeMonotonicityChecker(
    storm::storage::SparseMatrix<ParametricType> const& parametricTransitionMatrix) {
    monotonicityChecker = storm::analysis::MonotonicityChecker<ParametricType>(parametricTransitionMatrix);
}

template<typename ParametricType, typename ConstantType>
void OrderBasedMonotonicityBackend<ParametricType, ConstantType>::initializeOrderExtender(
    storm::storage::BitVector const& topStates, storm::storage::BitVector const& bottomStates,
    storm::storage::SparseMatrix<ParametricType> const& parametricTransitionMatrix) {
    orderExtender = storm::analysis::OrderExtender<ParametricType, ConstantType>(topStates, bottomStates, parametricTransitionMatrix);
}

template<typename ParametricType, typename ConstantType>
void OrderBasedMonotonicityBackend<ParametricType, ConstantType>::registerParameterLifterReference(
    storm::transformer::ParameterLifter<ParametricType, ConstantType> const& parameterLifter) {
    this->parameterLifterRef.reset(parameterLifter);
}

template<typename ParametricType, typename ConstantType>
void OrderBasedMonotonicityBackend<ParametricType, ConstantType>::registerPLABoundFunction(
    std::function<std::vector<ConstantType>(storm::Environment const&, AnnotatedRegion<ParametricType>&, storm::OptimizationDirection)> fun) {
    this->plaBoundFunction = fun;
}

template<typename ParametricType, typename ConstantType>
storm::storage::BitVector OrderBasedMonotonicityBackend<ParametricType, ConstantType>::getChoicesToFixForPLASolver(
    AnnotatedRegion<ParametricType> const& region, storm::OptimizationDirection dir, std::vector<uint64_t>& schedulerChoices) {
    if (useOnlyGlobal) {
        return {};
    }
    STORM_LOG_ASSERT(parameterLifterRef.has_value(), "Parameter lifter reference not initialized.");

    auto monotonicityAnnotation = region.monotonicityAnnotation.getOrderBasedMonotonicityAnnotation();
    STORM_LOG_ASSERT(monotonicityAnnotation.has_value() && monotonicityAnnotation->localMonotonicityResult != nullptr,
                     "Order-based monotonicity annotation must be present.");
    auto const& localMonotonicityResult = *monotonicityAnnotation->localMonotonicityResult;

    storm::storage::BitVector result(schedulerChoices.size(), false);

    auto const& occurringVariables = parameterLifterRef->getOccurringVariablesAtState();
    for (uint64_t state = 0; state < parameterLifterRef->getRowGroupCount(); ++state) {
        auto oldStateNumber = parameterLifterRef->getOriginalStateNumber(state);
        auto const& variables = occurringVariables.at(oldStateNumber);
        // point at which we start with rows for this state

        STORM_LOG_THROW(variables.size() <= 1, storm::exceptions::NotImplementedException,
                        "Using localMonRes not yet implemented for states with 2 or more variables, please run without --use-monotonicity.");

        bool allMonotone = true;
        for (auto var : variables) {
            auto const monotonicity = localMonotonicityResult.getMonotonicity(oldStateNumber, var);

            bool const fixToLowerBound =
                monotonicity == MonotonicityKind::Constant || monotonicity == (storm::solver::minimize(dir) ? MonotonicityKind::Incr : MonotonicityKind::Decr);
            bool const fixToUpperBound =
                monotonicity == MonotonicityKind::Constant || monotonicity == (storm::solver::maximize(dir) ? MonotonicityKind::Incr : MonotonicityKind::Decr);
            if (fixToLowerBound || fixToUpperBound) {
                // TODO: Setting the lower/upper bounded choices like this is fragile and should be replaced by a more robust solution
                schedulerChoices[state] = fixToLowerBound ? 0 : 1;
            } else {
                allMonotone = false;
            }
        }
        if (allMonotone) {
            result.set(state);
        }
    }
    return result;
}

template class OrderBasedMonotonicityBackend<storm::RationalFunction, double>;
template class OrderBasedMonotonicityBackend<storm::RationalFunction, storm::RationalNumber>;

}  // namespace storm::modelchecker
