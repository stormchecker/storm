#include "storm-pars/modelchecker/region/monotonicity/MonotonicityAnnotation.h"

#include "storm-pars/modelchecker/region/monotonicity/LocalMonotonicityResult.h"
#include "storm-pars/modelchecker/region/monotonicity/Order.h"
#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm::modelchecker {

template<typename ParametricType>
storm::OptionalRef<typename MonotonicityAnnotation<ParametricType>::DefaultMonotonicityAnnotation>
MonotonicityAnnotation<ParametricType>::getDefaultMonotonicityAnnotation() {
    if (auto* mono = std::get_if<DefaultMonotonicityAnnotation>(&data)) {
        return *mono;
    }
    return storm::NullRef;
}

template<typename ParametricType>
storm::OptionalRef<typename MonotonicityAnnotation<ParametricType>::OrderBasedMonotonicityAnnotation>
MonotonicityAnnotation<ParametricType>::getOrderBasedMonotonicityAnnotation() {
    if (auto* mono = std::get_if<OrderBasedMonotonicityAnnotation>(&data)) {
        return *mono;
    }
    return storm::NullRef;
}

template<typename ParametricType>
storm::OptionalRef<typename MonotonicityAnnotation<ParametricType>::DefaultMonotonicityAnnotation const>
MonotonicityAnnotation<ParametricType>::getDefaultMonotonicityAnnotation() const {
    if (auto const* mono = std::get_if<DefaultMonotonicityAnnotation>(&data)) {
        return *mono;
    }
    return storm::NullRef;
}

template<typename ParametricType>
storm::OptionalRef<typename MonotonicityAnnotation<ParametricType>::OrderBasedMonotonicityAnnotation const>
MonotonicityAnnotation<ParametricType>::getOrderBasedMonotonicityAnnotation() const {
    if (auto const* mono = std::get_if<OrderBasedMonotonicityAnnotation>(&data)) {
        return *mono;
    }
    return storm::NullRef;
}

template<typename ParametricType>
storm::OptionalRef<storm::analysis::MonotonicityResult<typename MonotonicityAnnotation<ParametricType>::VariableType> const>
MonotonicityAnnotation<ParametricType>::getGlobalMonotonicityResult() const {
    storm::OptionalRef<storm::analysis::MonotonicityResult<VariableType> const> result;
    if (auto defaultMono = getDefaultMonotonicityAnnotation(); defaultMono.has_value() && defaultMono->globalMonotonicity) {
        result.reset(*defaultMono->globalMonotonicity);
    } else if (auto orderMono = getOrderBasedMonotonicityAnnotation(); orderMono.has_value() && orderMono->localMonotonicityResult) {
        if (auto globalRes = orderMono->localMonotonicityResult->getGlobalMonotonicityResult()) {
            result.reset(*globalRes);
        }
    }
    return result;
}

template struct MonotonicityAnnotation<storm::RationalFunction>;
}  // namespace storm::modelchecker