#pragma once

#include <memory>
#include <variant>

#include "storm-pars/utility/parametric.h"
#include "storm/utility/OptionalRef.h"

namespace storm {

// Forward Declarations
namespace analysis {
template<typename VariableType>
class MonotonicityResult;
class Order;
template<typename VariableType>
class LocalMonotonicityResult;
}  // namespace analysis

namespace modelchecker {

template<typename ParametricType>
struct MonotonicityAnnotation {
    using VariableType = storm::utility::parametric::VariableType_t<ParametricType>;

    struct DefaultMonotonicityAnnotation {
        std::shared_ptr<storm::analysis::MonotonicityResult<VariableType>> globalMonotonicity{nullptr};
    };
    struct OrderBasedMonotonicityAnnotation {
        std::shared_ptr<storm::analysis::Order> stateOrder{nullptr};
        std::shared_ptr<storm::analysis::LocalMonotonicityResult<VariableType>> localMonotonicityResult{nullptr};
    };

    storm::OptionalRef<DefaultMonotonicityAnnotation> getDefaultMonotonicityAnnotation();
    storm::OptionalRef<DefaultMonotonicityAnnotation const> getDefaultMonotonicityAnnotation() const;

    storm::OptionalRef<OrderBasedMonotonicityAnnotation> getOrderBasedMonotonicityAnnotation();
    storm::OptionalRef<OrderBasedMonotonicityAnnotation const> getOrderBasedMonotonicityAnnotation() const;

    storm::OptionalRef<storm::analysis::MonotonicityResult<VariableType> const> getGlobalMonotonicityResult() const;

    std::variant<DefaultMonotonicityAnnotation, OrderBasedMonotonicityAnnotation> data;
};

}  // namespace modelchecker
}  // namespace storm