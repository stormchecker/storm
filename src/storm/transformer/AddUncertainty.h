#pragma once

#include <type_traits>

#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/models/sparse/Model.h"
#include "storm/utility/constants.h"

namespace storm::transformer {

/**
 * This class is a convenience transformer to add uncertainty.
 * We currently support only one type of self-defined uncertainty, although additional types of uncertainty are imaginable.
 * The transformer does maintain reward models, state labels, state valuations, choice labels and choice origins.
 *
 * When ValueType is storm::RationalNumber the output model uses storm::RationalInterval (exact arithmetic).
 * For all other ValueTypes (e.g. double) the output uses storm::Interval (double-precision).
 *
 * @tparam ValueType  The value type of the input model.
 */
template<typename ValueType>
class AddUncertainty {
   public:
    using IntervalType = std::conditional_t<std::is_same_v<ValueType, storm::RationalNumber>, storm::RationalInterval, storm::Interval>;
    static_assert(std::is_same_v<ValueType, storm::IntervalBaseType<IntervalType>>, "Expected ValueType to match the interval base type.");

    AddUncertainty(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& originalModel);
    std::shared_ptr<storm::models::sparse::Model<IntervalType>> transform(ValueType additiveUncertainty,
                                                                          ValueType minimalValue = storm::utility::convertNumber<ValueType>(0.0001),
                                                                          std::optional<uint64_t> maxSuccessors = {});

   private:
    IntervalType addUncertainty(ValueType const& vt, ValueType additiveUncertainty, ValueType minimalValue);
    std::shared_ptr<storm::models::sparse::Model<ValueType>> origModel;
};

}  // namespace storm::transformer