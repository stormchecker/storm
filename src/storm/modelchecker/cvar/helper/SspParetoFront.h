#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <sstream>
#include <vector>

#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

/*!
 * Represents the lower-right boundary of one SSP CVaR Pareto set from the paper.
 *
 * For a fixed state and cost bound n, each point (p, E) represents an achievable tradeoff where:
 *  - p is the probability of reaching the goal within the current cost bound, and
 *  - E is the expected continuation cost beyond that bound.
 *
 * The class only stores extremal boundary points. The upward/leftward closed polygon itself is induced by
 * these points together with convex closure.
 */
template<typename ValueType>
class SspParetoFront {
   public:
    struct Point {
        //! Probability of reaching the goal within the current cost bound.
        ValueType probability;
        //! Expected continuation cost beyond the current cost bound.
        ValueType expectedCost;

        enum class DominanceResult { Incomparable, Dominates, Dominated, Equal };

        DominanceResult getDominance(Point const& other) const {
            if (probability == other.probability && expectedCost == other.expectedCost) {
                return DominanceResult::Equal;
            }
            if (probability >= other.probability && expectedCost <= other.expectedCost) {
                return DominanceResult::Dominates;
            }
            if (probability <= other.probability && expectedCost >= other.expectedCost) {
                return DominanceResult::Dominated;
            }
            return DominanceResult::Incomparable;
        }
    };

    using container_type = std::vector<Point>;
    using const_iterator = typename container_type::const_iterator;

    SspParetoFront() = default;

    explicit SspParetoFront(container_type points) : points(std::move(points)) {
        canonicalize();
    }

    static SspParetoFront singleton(ValueType const& probability, ValueType const& expectedCost) {
        return SspParetoFront(container_type{{probability, expectedCost}});
    }

    bool empty() const {
        return points.empty();
    }

    std::size_t size() const {
        return points.size();
    }

    container_type const& getPoints() const {
        return points;
    }

    const_iterator begin() const {
        return points.begin();
    }

    const_iterator end() const {
        return points.end();
    }

    void clear() {
        points.clear();
    }

    void addPoint(Point const& point) {
        addPoint(Point{point});
    }

    void addPoint(Point&& point) {
        auto it = points.begin();
        while (it != points.end()) {
            switch (point.getDominance(*it)) {
                case Point::DominanceResult::Equal:
                case Point::DominanceResult::Dominated:
                    return;
                case Point::DominanceResult::Dominates:
                    it = points.erase(it);
                    break;
                case Point::DominanceResult::Incomparable:
                    ++it;
                    break;
            }
        }
        points.push_back(std::move(point));
        canonicalize();
    }

    void addPoints(container_type const& additionalPoints) {
        points.insert(points.end(), additionalPoints.begin(), additionalPoints.end());
        canonicalize();
    }

    SspParetoFront scaled(ValueType const& factor) const {
        if (empty()) {
            return SspParetoFront();
        }
        container_type scaledPoints;
        scaledPoints.reserve(points.size());
        for (auto const& point : points) {
            scaledPoints.push_back(Point{factor * point.probability, factor * point.expectedCost});
        }
        return SspParetoFront(std::move(scaledPoints));
    }

    SspParetoFront minkowskiSum(SspParetoFront const& other) const {
        if (empty() || other.empty()) {
            return SspParetoFront();
        }
        container_type sumPoints;
        sumPoints.reserve(points.size() * other.points.size());
        for (auto const& left : points) {
            for (auto const& right : other.points) {
                sumPoints.push_back(Point{left.probability + right.probability, left.expectedCost + right.expectedCost});
            }
        }
        return SspParetoFront(std::move(sumPoints));
    }

    static SspParetoFront convexUnion(std::vector<SspParetoFront> const& fronts) {
        container_type unionPoints;
        std::size_t totalPointCount = 0;
        for (auto const& front : fronts) {
            totalPointCount += front.size();
        }
        unionPoints.reserve(totalPointCount);
        for (auto const& front : fronts) {
            unionPoints.insert(unionPoints.end(), front.begin(), front.end());
        }
        return SspParetoFront(std::move(unionPoints));
    }

    /*!
     * Returns the minimal continuation cost E on the lower boundary at the given probability p.
     *
     * This is the paper-specific query used to evaluate a frontier for a fixed bound n:
     * it yields the value min{E | (p, E) is contained in the convex Pareto set}.
     */
    std::optional<ValueType> getMinimalContinuationCostAtProbability(ValueType const& probability) const {
        if (empty()) {
            return std::nullopt;
        }
        if (probability > points.back().probability) {
            return std::nullopt;
        }
        if (probability <= points.front().probability) {
            return points.front().expectedCost;
        }

        for (std::size_t index = 1; index < points.size(); ++index) {
            Point const& left = points[index - 1];
            Point const& right = points[index];
            if (probability <= right.probability) {
                ValueType const probabilityDelta = right.probability - left.probability;
                STORM_LOG_ASSERT(probabilityDelta > 0, "Expected SSP Pareto front points to be strictly sorted by probability.");
                ValueType const interpolationFactor = (probability - left.probability) / probabilityDelta;
                return left.expectedCost + interpolationFactor * (right.expectedCost - left.expectedCost);
            }
        }

        return points.back().expectedCost;
    }

    std::string toString() const {
        std::stringstream stream;
        stream << "{";
        bool first = true;
        for (auto const& point : points) {
            if (!first) {
                stream << ", ";
            }
            first = false;
            stream << "(" << point.probability << ", " << point.expectedCost << ")";
        }
        stream << "}";
        return stream.str();
    }

   private:
    void canonicalize() {
        if (points.empty()) {
            return;
        }
        sortPoints();
        removeDuplicateProbabilityPoints();
        removeDominatedPoints();
        removeNonExtremeConvexPoints();
    }

    void sortPoints() {
        std::sort(points.begin(), points.end(), [](Point const& left, Point const& right) {
            if (left.probability == right.probability) {
                return left.expectedCost < right.expectedCost;
            }
            return left.probability < right.probability;
        });
    }

    void removeDuplicateProbabilityPoints() {
        container_type uniquePoints;
        uniquePoints.reserve(points.size());
        for (auto const& point : points) {
            if (!uniquePoints.empty() && uniquePoints.back().probability == point.probability) {
                continue;
            }
            uniquePoints.push_back(point);
        }
        points = std::move(uniquePoints);
    }

    void removeDominatedPoints() {
        if (points.size() < 2) {
            return;
        }
        container_type nonDominatedPoints;
        nonDominatedPoints.reserve(points.size());
        ValueType bestExpectedCostSeenFromRight = points.back().expectedCost;
        nonDominatedPoints.push_back(points.back());
        for (std::size_t index = points.size() - 1; index > 0; --index) {
            Point const& point = points[index - 1];
            if (point.expectedCost < bestExpectedCostSeenFromRight) {
                nonDominatedPoints.push_back(point);
                bestExpectedCostSeenFromRight = point.expectedCost;
            }
        }
        std::reverse(nonDominatedPoints.begin(), nonDominatedPoints.end());
        points = std::move(nonDominatedPoints);
    }

    void removeNonExtremeConvexPoints() {
        if (points.size() < 3) {
            return;
        }
        container_type hullPoints;
        hullPoints.reserve(points.size());
        for (auto const& point : points) {
            hullPoints.push_back(point);
            while (hullPoints.size() >= 3 &&
                   liesOnOrAboveSegment(hullPoints[hullPoints.size() - 3], hullPoints[hullPoints.size() - 2], hullPoints[hullPoints.size() - 1])) {
                hullPoints.erase(hullPoints.end() - 2);
            }
        }
        points = std::move(hullPoints);
        STORM_LOG_ASSERT(std::adjacent_find(points.begin(), points.end(),
                                            [](Point const& left, Point const& right) {
                                                return left.probability >= right.probability || left.expectedCost >= right.expectedCost;
                                            }) == points.end(),
                         "Expected SSP Pareto front points to be strictly ordered by increasing probability and increasing expected cost.");
    }

    static bool liesOnOrAboveSegment(Point const& left, Point const& middle, Point const& right) {
        ValueType const leftToRightProbabilityDelta = right.probability - left.probability;
        ValueType const leftToMiddleProbabilityDelta = middle.probability - left.probability;
        STORM_LOG_ASSERT(leftToRightProbabilityDelta > 0 && leftToMiddleProbabilityDelta > 0,
                         "Expected SSP Pareto front probabilities to be strictly increasing.");
        return (middle.expectedCost - left.expectedCost) * leftToRightProbabilityDelta >=
               (right.expectedCost - left.expectedCost) * leftToMiddleProbabilityDelta;
    }

    container_type points;
};

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
