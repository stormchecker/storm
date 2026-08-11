#pragma once

#include <optional>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/macros.h"

namespace storm::analysis {
template<typename ValueType>
class ConstraintCollector;
}

namespace storm::pars {
namespace api {
template<typename ValueType>
void exportParametricResultToFile(std::optional<ValueType>, storm::OptionalRef<storm::analysis::ConstraintCollector<ValueType> const> const&,
                                  std::string const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Cannot export non-parametric result.");
}

template<>
void exportParametricResultToFile(std::optional<storm::RationalFunction> result,
                                  storm::OptionalRef<storm::analysis::ConstraintCollector<storm::RationalFunction> const> const& constraintCollector,
                                  std::string const& path);

}  // namespace api
}  // namespace storm::pars
