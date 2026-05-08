#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <sstream>
#include <utility>
#include <vector>

#include "storm/utility/constants.h"
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

   private:
    struct AlreadyCanonicalTag {};
    struct AlreadySortedTag {};

    explicit SspParetoFront(container_type points, AlreadyCanonicalTag) : points(std::move(points)) {
        // Intentionally left empty.
    }

    explicit SspParetoFront(container_type points, AlreadySortedTag) : points(std::move(points)) {
        canonicalizeSorted();
    }

   public:
    SspParetoFront() = default;

    explicit SspParetoFront(container_type points) : points(std::move(points)) {
        canonicalize();
    }

    static SspParetoFront singleton(ValueType const& probability, ValueType const& expectedCost) {
        return SspParetoFront(container_type{{probability, expectedCost}}, AlreadyCanonicalTag{});
    }

    bool empty() const {
        return points.empty();
    }

    std::size_t size() const {
        return points.size();
    }

    bool isSingleton() const {
        return points.size() == 1;
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
        if (storm::utility::isOne(factor)) {
            return *this;
        }
        if (storm::utility::isZero(factor)) {
            return singleton(storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>());
        }
        if (isSingleton()) {
            auto const& point = points.front();
            return singleton(factor * point.probability, factor * point.expectedCost);
        }
        container_type scaledPoints;
        scaledPoints.reserve(points.size());
        for (auto const& point : points) {
            scaledPoints.push_back(Point{factor * point.probability, factor * point.expectedCost});
        }
        if (storm::utility::isPositive(factor)) {
            return SspParetoFront(std::move(scaledPoints), AlreadyCanonicalTag{});
        }
        return SspParetoFront(std::move(scaledPoints));
    }

    SspParetoFront minkowskiSum(SspParetoFront const& other) const {
        if (empty() || other.empty()) {
            return SspParetoFront();
        }
        if (isSingleton()) {
            return other.translated(points.front());
        }
        if (other.isSingleton()) {
            return translated(other.points.front());
        }
        return minkowskiSumConvexChain(other, storm::utility::one<ValueType>());
    }

    SspParetoFront minkowskiSumScaled(SspParetoFront const& other, ValueType const& factor) const {
        if (empty() || other.empty()) {
            return SspParetoFront();
        }
        if (storm::utility::isZero(factor)) {
            return *this;
        }
        if (storm::utility::isOne(factor)) {
            return minkowskiSum(other);
        }
        if (isSingleton()) {
            return other.scaledTranslated(factor, points.front());
        }
        if (other.isSingleton()) {
            Point const& point = other.points.front();
            return translated(Point{factor * point.probability, factor * point.expectedCost});
        }
        if (storm::utility::isPositive(factor)) {
            return minkowskiSumConvexChain(other, factor);
        }

        container_type sumPoints;
        sumPoints.reserve(points.size() * other.points.size());
        for (auto const& left : points) {
            for (auto const& right : other.points) {
                sumPoints.push_back(Point{left.probability + factor * right.probability, left.expectedCost + factor * right.expectedCost});
            }
        }
        return SspParetoFront(std::move(sumPoints));
    }

    static SspParetoFront convexUnion(std::vector<SspParetoFront> const& fronts) {
        SspParetoFront const* onlyNonEmptyFront = nullptr;
        std::size_t totalPointCount = 0;
        for (auto const& front : fronts) {
            if (front.empty()) {
                continue;
            }
            onlyNonEmptyFront = onlyNonEmptyFront == nullptr ? &front : onlyNonEmptyFront;
            totalPointCount += front.size();
        }
        if (totalPointCount == 0) {
            return SspParetoFront();
        }
        if (onlyNonEmptyFront != nullptr && onlyNonEmptyFront->size() == totalPointCount) {
            return *onlyNonEmptyFront;
        }
        return SspParetoFront(mergeSortedFrontPoints(fronts, totalPointCount), AlreadySortedTag{});
    }

    static SspParetoFront convexUnion(std::vector<SspParetoFront>&& fronts) {
        return convexUnionDestructive(fronts);
    }

    static SspParetoFront convexUnionDestructive(std::vector<SspParetoFront>& fronts) {
        SspParetoFront* onlyNonEmptyFront = nullptr;
        std::size_t totalPointCount = 0;
        for (auto& front : fronts) {
            if (front.empty()) {
                continue;
            }
            onlyNonEmptyFront = onlyNonEmptyFront == nullptr ? &front : onlyNonEmptyFront;
            totalPointCount += front.size();
        }
        if (totalPointCount == 0) {
            return SspParetoFront();
        }
        if (onlyNonEmptyFront != nullptr && onlyNonEmptyFront->size() == totalPointCount) {
            return std::move(*onlyNonEmptyFront);
        }
        return SspParetoFront(mergeSortedFrontPoints(fronts, totalPointCount), AlreadySortedTag{});
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

        auto rightIt =
            std::lower_bound(points.begin(), points.end(), probability, [](Point const& point, ValueType const& value) { return point.probability < value; });
        STORM_LOG_ASSERT(rightIt != points.begin() && rightIt != points.end(), "Expected probability to lie inside the SSP Pareto-front range.");

        Point const& left = *(rightIt - 1);
        Point const& right = *rightIt;
        ValueType const probabilityDelta = right.probability - left.probability;
        STORM_LOG_ASSERT(probabilityDelta > 0, "Expected SSP Pareto front points to be strictly sorted by probability.");
        ValueType const interpolationFactor = (probability - left.probability) / probabilityDelta;
        return left.expectedCost + interpolationFactor * (right.expectedCost - left.expectedCost);
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
    SspParetoFront translated(Point const& offset) const {
        if (empty()) {
            return SspParetoFront();
        }
        if (storm::utility::isZero(offset.probability) && storm::utility::isZero(offset.expectedCost)) {
            return *this;
        }
        if (isSingleton()) {
            Point const& point = points.front();
            return singleton(point.probability + offset.probability, point.expectedCost + offset.expectedCost);
        }

        container_type translatedPoints;
        translatedPoints.reserve(points.size());
        for (auto const& point : points) {
            translatedPoints.push_back(Point{point.probability + offset.probability, point.expectedCost + offset.expectedCost});
        }
        return SspParetoFront(std::move(translatedPoints), AlreadyCanonicalTag{});
    }

    SspParetoFront scaledTranslated(ValueType const& factor, Point const& offset) const {
        if (empty()) {
            return SspParetoFront();
        }
        if (storm::utility::isZero(factor)) {
            return singleton(offset.probability, offset.expectedCost);
        }
        if (isSingleton()) {
            Point const& point = points.front();
            return singleton(offset.probability + factor * point.probability, offset.expectedCost + factor * point.expectedCost);
        }

        container_type scaledTranslatedPoints;
        scaledTranslatedPoints.reserve(points.size());
        for (auto const& point : points) {
            scaledTranslatedPoints.push_back(Point{offset.probability + factor * point.probability, offset.expectedCost + factor * point.expectedCost});
        }
        if (storm::utility::isPositive(factor)) {
            return SspParetoFront(std::move(scaledTranslatedPoints), AlreadyCanonicalTag{});
        }
        return SspParetoFront(std::move(scaledTranslatedPoints));
    }

    SspParetoFront minkowskiSumConvexChain(SspParetoFront const& other, ValueType const& factor) const {
        STORM_LOG_ASSERT(storm::utility::isPositive(factor), "Expected a positive scaling factor for convex-chain Minkowski sum.");

        container_type sumPoints;
        sumPoints.reserve(points.size() + other.points.size() - 1);

        Point current{points.front().probability + factor * other.points.front().probability,
                      points.front().expectedCost + factor * other.points.front().expectedCost};
        sumPoints.push_back(current);

        std::size_t leftEdge = 0;
        std::size_t rightEdge = 0;
        while (leftEdge + 1 < points.size() || rightEdge + 1 < other.points.size()) {
            if (rightEdge + 1 == other.points.size()) {
                addEdge(current, points[leftEdge], points[leftEdge + 1]);
                ++leftEdge;
            } else if (leftEdge + 1 == points.size()) {
                addScaledEdge(current, other.points[rightEdge], other.points[rightEdge + 1], factor);
                ++rightEdge;
            } else {
                int_fast8_t const slopeComparison =
                    compareEdgeSlopes(points[leftEdge], points[leftEdge + 1], other.points[rightEdge], other.points[rightEdge + 1]);
                if (slopeComparison < 0) {
                    addEdge(current, points[leftEdge], points[leftEdge + 1]);
                    ++leftEdge;
                } else if (slopeComparison > 0) {
                    addScaledEdge(current, other.points[rightEdge], other.points[rightEdge + 1], factor);
                    ++rightEdge;
                } else {
                    addEdge(current, points[leftEdge], points[leftEdge + 1]);
                    addScaledEdge(current, other.points[rightEdge], other.points[rightEdge + 1], factor);
                    ++leftEdge;
                    ++rightEdge;
                }
            }
            sumPoints.push_back(current);
        }

        return SspParetoFront(std::move(sumPoints), AlreadySortedTag{});
    }

    void canonicalize() {
        if (points.empty()) {
            return;
        }
        sortPoints();
        canonicalizeSorted();
    }

    void canonicalizeSorted() {
        if (points.empty()) {
            return;
        }
        removeDuplicateAndDominatedPoints();
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

    static bool pointLess(Point const& left, Point const& right) {
        if (left.probability == right.probability) {
            return left.expectedCost < right.expectedCost;
        }
        return left.probability < right.probability;
    }

    static void addEdge(Point& point, Point const& edgeStart, Point const& edgeEnd) {
        point.probability += edgeEnd.probability - edgeStart.probability;
        point.expectedCost += edgeEnd.expectedCost - edgeStart.expectedCost;
    }

    static void addScaledEdge(Point& point, Point const& edgeStart, Point const& edgeEnd, ValueType const& factor) {
        point.probability += factor * (edgeEnd.probability - edgeStart.probability);
        point.expectedCost += factor * (edgeEnd.expectedCost - edgeStart.expectedCost);
    }

    static int_fast8_t compareEdgeSlopes(Point const& leftStart, Point const& leftEnd, Point const& rightStart, Point const& rightEnd) {
        ValueType const leftProbabilityDelta = leftEnd.probability - leftStart.probability;
        ValueType const rightProbabilityDelta = rightEnd.probability - rightStart.probability;
        STORM_LOG_ASSERT(leftProbabilityDelta > 0 && rightProbabilityDelta > 0, "Expected strictly increasing probabilities in SSP Pareto front edges.");

        ValueType const leftScaledCostDelta = (leftEnd.expectedCost - leftStart.expectedCost) * rightProbabilityDelta;
        ValueType const rightScaledCostDelta = (rightEnd.expectedCost - rightStart.expectedCost) * leftProbabilityDelta;
        if (leftScaledCostDelta < rightScaledCostDelta) {
            return -1;
        }
        if (rightScaledCostDelta < leftScaledCostDelta) {
            return 1;
        }
        return 0;
    }

    static container_type mergeSortedFrontPoints(std::vector<SspParetoFront> const& fronts, std::size_t totalPointCount) {
        struct MergeCursor {
            const_iterator iterator;
            const_iterator end;
        };
        auto cursorGreater = [](MergeCursor const& left, MergeCursor const& right) { return pointLess(*right.iterator, *left.iterator); };
        std::vector<MergeCursor> initialCursors;
        initialCursors.reserve(fronts.size());
        for (auto const& front : fronts) {
            if (!front.empty()) {
                initialCursors.push_back(MergeCursor{front.begin(), front.end()});
            }
        }
        std::priority_queue<MergeCursor, std::vector<MergeCursor>, decltype(cursorGreater)> cursors(cursorGreater, std::move(initialCursors));

        container_type mergedPoints;
        mergedPoints.reserve(totalPointCount);
        while (!cursors.empty()) {
            MergeCursor cursor = cursors.top();
            cursors.pop();
            mergedPoints.push_back(*cursor.iterator);
            ++cursor.iterator;
            if (cursor.iterator != cursor.end) {
                cursors.push(cursor);
            }
        }
        STORM_LOG_ASSERT(mergedPoints.size() == totalPointCount, "Unexpected number of points produced by sorted SSP Pareto-front merge.");
        return mergedPoints;
    }

    void removeDuplicateAndDominatedPoints() {
        if (points.size() < 2) {
            return;
        }

        std::size_t writeIndex = points.size();
        std::size_t index = points.size();
        bool hasBestExpectedCostSeenFromRight = false;
        ValueType bestExpectedCostSeenFromRight{};
        while (index > 0) {
            std::size_t const groupEnd = index;
            ValueType const probability = points[groupEnd - 1].probability;
            while (index > 0 && points[index - 1].probability == probability) {
                --index;
            }

            Point const& bestPointForProbability = points[index];
            if (!hasBestExpectedCostSeenFromRight || bestPointForProbability.expectedCost < bestExpectedCostSeenFromRight) {
                --writeIndex;
                points[writeIndex] = bestPointForProbability;
                bestExpectedCostSeenFromRight = bestPointForProbability.expectedCost;
                hasBestExpectedCostSeenFromRight = true;
            }
        }
        points.erase(points.begin(), points.begin() + static_cast<typename container_type::difference_type>(writeIndex));
    }

    void removeNonExtremeConvexPoints() {
        if (points.size() < 3) {
            return;
        }
        std::size_t hullSize = 0;
        for (std::size_t index = 0, endIndex = points.size(); index < endIndex; ++index) {
            points[hullSize++] = points[index];
            while (hullSize >= 3 && liesOnOrAboveSegment(points[hullSize - 3], points[hullSize - 2], points[hullSize - 1])) {
                points[hullSize - 2] = points[hullSize - 1];
                --hullSize;
            }
        }
        points.resize(hullSize);
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
