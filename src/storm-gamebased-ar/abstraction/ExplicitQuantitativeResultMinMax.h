#pragma once

#include "storm-gamebased-ar/abstraction/ExplicitQuantitativeResult.h"
#include "storm/solver/OptimizationDirection.h"

namespace storm::gbar {
namespace abstraction {

template<typename ValueType>
class ExplicitQuantitativeResultMinMax {
   public:
    ExplicitQuantitativeResultMinMax() = default;
    ExplicitQuantitativeResultMinMax(uint64_t numberOfStates);

    ExplicitQuantitativeResult<ValueType> const& getMin() const;
    ExplicitQuantitativeResult<ValueType>& getMin();
    ExplicitQuantitativeResult<ValueType> const& getMax() const;
    ExplicitQuantitativeResult<ValueType>& getMax();
    ExplicitQuantitativeResult<ValueType> const& get(storm::OptimizationDirection const& dir) const;
    ExplicitQuantitativeResult<ValueType>& get(storm::OptimizationDirection const& dir);

    void setMin(ExplicitQuantitativeResult<ValueType>&& newMin);
    void setMax(ExplicitQuantitativeResult<ValueType>&& newMax);

   private:
    ExplicitQuantitativeResult<ValueType> min;
    ExplicitQuantitativeResult<ValueType> max;
};

}  // namespace abstraction
}  // namespace storm::gbar
