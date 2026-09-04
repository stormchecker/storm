#pragma once

#include "storm-gspn/storage/gspn/Transition.h"
#include "storm/numbers/constants.h"

namespace storm {
namespace gspn {
template<typename WeightType>
class ImmediateTransition : public storm::gspn::Transition {
   public:
    /*!
     * Sets the weight of this transition to the given value.
     *
     * @param weight The new weight for this transition.
     */
    void setWeight(WeightType const& weight) {
        this->weight = weight;
    }

    /*!
     * Retrieves the weight of this transition.
     *
     * @return The weight of this transition.
     */
    WeightType getWeight() const {
        return this->weight;
    }

    /**
     * True iff no weight is attached.
     */
    bool noWeightAttached() const {
        return storm::numbers::isZero(weight);
    }

   private:
    // the weight of the transition. Must be positive, if zero, then no weight is attached.
    WeightType weight;
};
}  // namespace gspn
}  // namespace storm
