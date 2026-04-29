
#include "storm/utility/RationalApproximation.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::utility {

storm::RationalNumber findRational(storm::RationalNumber const& lowerBound, bool lowerInclusive, storm::RationalNumber const& upperBound, bool upperInclusive) {
    using Integer = typename storm::NumberTraits<storm::RationalNumber>::IntegerType;
    STORM_LOG_ASSERT(lowerBound < upperBound || (lowerBound == upperBound && lowerInclusive && upperInclusive), "Invalid interval for rational approximation.");

    // Handle negative numbers
    if (auto const zero = storm::utility::zero<storm::RationalNumber>(); lowerBound < zero) {
        // check if zero is in the interval
        if (upperBound > zero || (upperBound == zero && upperInclusive)) {
            return storm::utility::zero<storm::RationalNumber>();
        } else {
            // all numbers in the interval are negative. We translate that to a positive problem and negate the result
            return -findRational(-upperBound, upperInclusive, -lowerBound, lowerInclusive);
        }
    }
    // At this point, the solution is known to be non-negative

    // We compute a path in the Stern-Brocot tree from the root to the node representing the simplest rational in the closed interval [lowerBound, upperBound]
    // If the input interval is open on one or both sides, we traverse the tree further down until a suitable rational number is found
    // @see https://en.wikipedia.org/wiki/Stern–Brocot_tree#A_tree_of_continued_fractions
    // @see https://mathoverflow.net/a/424509
    // The path is encoded using a simple continued fraction representation.
    // We take path[0] times the right child, path[1] times the left child, path[2] times the right child, etc, using path.back()-1 steps in the last direction.
    std::vector<Integer> path;  // in simple continued fraction representation
    auto l = lowerBound;
    auto u = upperBound;
    while (true) {
        auto l_den = storm::utility::denominator(l);
        auto u_den = storm::utility::denominator(u);
        auto const [l_i, l_rem] = storm::utility::divide(storm::utility::numerator(l), l_den);
        auto const [u_i, u_rem] = storm::utility::divide(storm::utility::numerator(u), u_den);

        path.push_back(std::min(l_i, u_i));  // insert tree traversal information
        if (l_i == u_i && !storm::utility::isZero(l_rem) && !storm::utility::isZero(u_rem)) {
            // continue traversing the tree
            l = storm::utility::convertNumber<storm::RationalNumber>(l_den) / l_rem;
            u = storm::utility::convertNumber<storm::RationalNumber>(u_den) / u_rem;
            continue;
        }
        // Reaching this point means that we have found a node in the Stern-Brocot tree where the paths for lower and upper bound diverge.
        // If there still is a remainder, we need to add one to the last entry of the path so that it correctly encodes the node we are referring to.
        if (l_i != u_i && !storm::utility::isZero(l_i < u_i ? l_rem : u_rem)) {
            path.back() += Integer(1);
        }

        // Find out if we hit an interval boundary and whether we need to adapt this due to open intervals
        bool const needAdjustLower = !lowerInclusive && path.back() == l_i && storm::utility::isZero(l_rem);
        bool const needAdjustUpper = !upperInclusive && path.back() == u_i && storm::utility::isZero(u_rem);
        if (needAdjustLower || needAdjustUpper) {
            // handle for values of the "other" bound that does not need adjustment
            auto const& o_i = needAdjustLower ? u_i : l_i;
            auto const& o_rem = needAdjustLower ? u_rem : l_rem;
            auto const& o_den = needAdjustLower ? u_den : l_den;
            auto const& o_inclusive = needAdjustLower ? upperInclusive : lowerInclusive;

            // When adjusting lower bounds, we need to explore the right subtree to obtain a larger value than the current lower bound
            // When adjusting upper bounds, we need to explore the left subtree to obtain a smaller value than the current upper bound
            // Whether we currently look at left or right subtrees is determined by the parity of the index in the path:
            // Path entries at even indices correspond to right moves (increasing value) and entries at odd indices correspond to left moves (decreasing value)
            bool const currentDirectionIsIncreasing = (path.size() - 1) % 2 == 0;
            bool const adjustInCurrentDirection = (needAdjustLower && currentDirectionIsIncreasing) || (needAdjustUpper && !currentDirectionIsIncreasing);
            // Below, we navigate through the Stern-Brocot tree by adapting the path
            // path.back() += 1; extends the path to a child in the "current direction"
            // path.back() -= 1; path.emplace_back(2); extends the path to a child in the "counter direction"
            if (adjustInCurrentDirection) {
                STORM_LOG_ASSERT(path.back() <= o_i, "Unexpected case when navigating the Stern-Brocot tree.");
                if (path.back() + Integer(1) < o_i || (path.back() + Integer(1) == o_i && !storm::utility::isZero(o_rem))) {
                    // In this case, the next child (in the current direction) is inside the interval, so we can just take that
                    path.back() += Integer(1);
                } else if (path.back() + Integer(1) == o_i && storm::utility::isZero(o_rem)) {
                    // In this case, the next child coincides with the other boundary
                    if (o_inclusive) {
                        path.back() += Integer(1);  // add next child
                    } else {
                        // We first take one child in the current direction and then one child in the counter direction.
                        // path.back() += 1; path.back() -= 1; // cancels out
                        path.emplace_back(2);
                    }
                } else {
                    // The following assertion holds because path.back() > o_i is not possible due to the way we constructed the path above
                    // and if there would be no remainder, the other boundary would be hit as well (i.e. we would have an empty interval (a,a).
                    STORM_LOG_ASSERT(path.back() == o_i && !storm::utility::isZero(o_rem), "Unexpected case when navigating the Stern-Brocot tree.");
                    // In this case, we need to append one child in the current direction and multiple children in the counter direction based on the continued
                    // fraction representation of the other boundary
                    auto const [o_i2, o_rem2] = storm::utility::divide(o_den, o_rem);
                    // path.back() += 1; path.back() -= 1; // cancels out
                    path.push_back(o_i2);
                    if (!storm::utility::isZero(o_rem2)) {
                        // If there still is a remainder, we add one to the last entry of the path so that it correctly encodes the node we are referring to.
                        path.back() += Integer(1);
                    } else if (!o_inclusive) {
                        // If there is no remainder, we are exactly on the other boundary. If that boundary is also excluded, we need to add one more step.
                        path.back() += Integer(1);
                    }
                }
            } else {
                // Adjusting a bound in the counter direction can only happen if the other bound still has a remainder
                // Otherwise, we would have also hit that bound
                STORM_LOG_ASSERT(o_i == path.back() - Integer(1), "Unexpected case when navigating the Stern-Brocot tree.");
                STORM_LOG_ASSERT(!storm::utility::isZero(o_rem), "Unexpected case when navigating the Stern-Brocot tree.");
                auto const [o_i2, o_rem2] = storm::utility::divide(o_den, o_rem);
                path.back() -= Integer(1);  // necessary in all cases
                if (o_i2 > Integer(2) || (o_i2 == Integer(2) && !storm::utility::isZero(o_rem2))) {
                    // In this case, the next child (in the counter direction) is inside the interval, so we can just take that
                    path.emplace_back(2);
                } else if (o_i2 == Integer(2) && storm::utility::isZero(o_rem2)) {
                    // In this case, the next child in counter direction coincides with the other boundary
                    if (o_inclusive) {
                        path.emplace_back(2);
                    } else {
                        // We first take one child in the counter direction and then one child in the current direction.
                        path.emplace_back(1);
                        path.emplace_back(2);
                    }
                } else {
                    STORM_LOG_ASSERT(o_i2 == Integer(1) && !storm::utility::isZero(o_rem2), "Unexpected case when navigating the Stern-Brocot tree.");
                    // In this case, we need to append one child in the counter direction and multiple children in the current direction based on the continued
                    // fraction representation of the other boundary
                    auto const [o_i3, o_rem3] = storm::utility::divide(o_rem, o_rem2);
                    path.emplace_back(1);
                    path.push_back(o_i3);
                    if (!storm::utility::isZero(o_rem3)) {
                        // If there still is a remainder, we add one to the last entry of the path so that it correctly encodes the node we are referring to.
                        path.back() += Integer(1);
                    } else if (!o_inclusive) {
                        // If there is no remainder, we are exactly on the other boundary. If that boundary is also excluded, we need to add one more step.
                        path.back() += Integer(1);
                    }
                }
            }
        }
        break;
    }

    // Now, construct the rational number from the path
    auto it = path.rbegin();
    auto result = storm::utility::convertNumber<storm::RationalNumber>(*it);
    for (++it; it != path.rend(); ++it) {
        result = storm::utility::convertNumber<storm::RationalNumber>(*it) + storm::utility::one<storm::RationalNumber>() / result;
    }
    return result;

    STORM_LOG_ASSERT(result > lowerBound || (lowerInclusive && result == lowerBound), "Result is below lower bound.");
    STORM_LOG_ASSERT(result < upperBound || (upperInclusive && result == upperBound), "Result is above upper bound.");
    return result;
}

}  // namespace storm::utility