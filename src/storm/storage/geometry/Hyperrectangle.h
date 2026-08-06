#pragma once

#include <iomanip>
#include <iostream>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/storage/geometry/Halfspace.h"
#include "storm/storage/geometry/Polytope.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {
namespace geometry {

/*
 * This class represents a hyperrectangle, i.e., the intersection of finitely many intervals
 */

template<typename ValueType>
class Hyperrectangle {
   public:
    Hyperrectangle(std::vector<ValueType> const& lowerBounds, std::vector<ValueType> const& upperBounds)
        : mLowerBounds(lowerBounds), mUpperBounds(upperBounds) {
        STORM_LOG_THROW(lowerBounds.size() == upperBounds.size(), storm::exceptions::InvalidArgumentException,
                        "Tried to construct a hyperrectangle but the number of given lower bounds does not equal the number of given upper bounds.");
    }

    Hyperrectangle(std::vector<ValueType>&& lowerBounds, std::vector<ValueType>&& upperBounds) : mLowerBounds(lowerBounds), mUpperBounds(upperBounds) {
        STORM_LOG_THROW(lowerBounds.size() == upperBounds.size(), storm::exceptions::InvalidArgumentException,
                        "Tried to construct a hyperrectangle but the number of given lower bounds does not equal the number of given upper bounds.");
    }

    std::vector<ValueType> const& lowerBounds() const {
        return mLowerBounds;
    }

    std::vector<ValueType>& lowerBounds() {
        return mLowerBounds;
    }

    std::vector<ValueType> const& upperBounds() const {
        return mUpperBounds;
    }

    std::vector<ValueType>& upperBounds() {
        return mUpperBounds;
    }

    /*
     * Enlarges this hyperrectangle such that it contains the given point
     */
    void enlarge(std::vector<ValueType> const& point) {
        STORM_LOG_THROW(point.size() == lowerBounds().size() && point.size() == upperBounds().size(), storm::exceptions::InvalidArgumentException,
                        "Tried to enlarge a hyperrectangle but the dimension of the given point does not match.");
        for (uint_fast64_t i = 0; i < lowerBounds().size(); ++i) {
            lowerBounds()[i] = std::min(lowerBounds()[i], point[i]);
            upperBounds()[i] = std::max(upperBounds()[i], point[i]);
        }
    }

    std::shared_ptr<Polytope<ValueType>> asPolytope() const {
        STORM_LOG_THROW(lowerBounds().size() == upperBounds().size(), storm::exceptions::InvalidArgumentException,
                        "Tried to construct a polytope form a hyperrectangle but the numbers of given lower and upper bounds do not match.");
        std::vector<Halfspace<ValueType>> halfspaces;
        halfspaces.reserve(2 * lowerBounds().size());
        for (uint_fast64_t i = 0; i < lowerBounds().size(); ++i) {
            std::vector<ValueType> direction(lowerBounds().size(), storm::utility::zero<ValueType>());
            direction[i] = -storm::utility::one<ValueType>();
            ValueType offset = -lowerBounds()[i];
            halfspaces.emplace_back(std::move(direction), std::move(offset));

            direction = std::vector<ValueType>(lowerBounds().size(), storm::utility::zero<ValueType>());
            direction[i] = storm::utility::one<ValueType>();
            offset = upperBounds()[i];
            halfspaces.emplace_back(std::move(direction), std::move(offset));
        }
        return Polytope<ValueType>::create(halfspaces);
    }

   private:
    std::vector<ValueType> mLowerBounds;
    std::vector<ValueType> mUpperBounds;
};
}  // namespace geometry
}  // namespace storage
}  // namespace storm
