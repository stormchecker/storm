#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/modelchecker/cvar/helper/SspParetoFront.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/transformer/zeroWeight/ZeroWeightAnalysis.h"
#include "storm/transformer/zeroWeight/ZeroWeightComponentLp.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::modelchecker::cvar {

/*! Answers supporting-direction queries for one zero-weight component entry. */
template<typename ValueType, SspParetoFrontKind FrontKind = SspParetoFrontKind::CostUpperTail>
class SspZeroWeightComponentLpSolver {
   public:
    using ParetoFront = SspParetoFront<ValueType, FrontKind>;
    using Point = typename ParetoFront::Point;
    using FrontierLayer = std::vector<ParetoFront>;

    struct Result {
        Point point;
        std::vector<ValueType> boundaryProbabilities;
    };

    /*! Builds a reusable LP for a validated component in the split model.
     * @param environment LP solver settings.
     * @param transitionMatrix Split transition matrix.
     * @param actionWeights One weight per matrix row.
     * @param component Zero-weight component and its boundary.
     */
    SspZeroWeightComponentLpSolver(storm::Environment const& environment, storm::storage::SparseMatrix<ValueType> const& transitionMatrix,
                                   std::vector<ValueType> const& actionWeights, storm::transformer::ZeroWeightComponent const& component)
        : componentLp(environment, transitionMatrix, actionWeights, component) {}

    /*! Returns one point minimizing the requested frontier support score.
     * @param entryState Entry state of this component.
     * @param frontierLayer Current-threshold fronts indexed by split state.
     * @param slope Nonnegative coefficient of probability in the support score.
     * @return One supported point and its boundary exit distribution.
     */
    Result support(uint64_t entryState, FrontierLayer const& frontierLayer, ValueType const& slope) {
        STORM_LOG_THROW(storm::utility::isNonNegative(slope), storm::exceptions::InvalidArgumentException, "Expected a nonnegative support slope.");

        auto const& boundaryStates = componentLp.getBoundaryStates();
        STORM_LOG_THROW(frontierLayer.size() > boundaryStates.back(), storm::exceptions::InvalidArgumentException,
                        "Expected a current-threshold frontier for every boundary state.");

        std::vector<Point> selectedPoints;
        std::vector<ValueType> directions;
        selectedPoints.reserve(boundaryStates.size());
        directions.reserve(boundaryStates.size());
        for (uint64_t boundaryState : boundaryStates) {
            auto const& front = frontierLayer[boundaryState];
            STORM_LOG_THROW(!front.empty(), storm::exceptions::InvalidArgumentException, "Expected a nonempty boundary frontier.");

            Point selected = *front.begin();
            auto score = supportScore(selected, slope);
            for (auto const& point : front) {
                auto const candidate = supportScore(point, slope);
                if (candidate < score) {
                    selected = point;
                    score = candidate;
                }
            }
            selectedPoints.push_back(selected);
            directions.push_back(-score);
        }

        auto lpResult = componentLp.maximize(entryState, directions);
        Point point{storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>()};
        for (uint64_t index = 0; index < boundaryStates.size(); ++index) {
            point.probability += lpResult.boundaryProbabilities[index] * selectedPoints[index].probability;
            point.expectedCost += lpResult.boundaryProbabilities[index] * selectedPoints[index].expectedCost;
        }
        return {point, std::move(lpResult.boundaryProbabilities)};
    }

    /*! Recovers the entry frontier by biobjective dichotomic search (Aneja and Nair, doi:10.1287/mnsc.25.1.73).
     * Floating-point edge checks are approximate.
     * @param entryState Entry state of this component.
     * @param frontierLayer Current-threshold fronts indexed by split state.
     * @return The supported entry frontier.
     */
    ParetoFront frontier(uint64_t entryState, FrontierLayer const& frontierLayer) {
        auto const zero = storm::utility::zero<ValueType>();
        auto const one = storm::utility::one<ValueType>();
        Point left;
        Point right;
        if constexpr (FrontKind == SspParetoFrontKind::CostUpperTail) {
            left = supportLexicographic(entryState, frontierLayer, zero, one, -one, zero);
            right = supportLexicographic(entryState, frontierLayer, -one, zero, zero, one);
        } else {
            left = supportLexicographic(entryState, frontierLayer, one, zero, zero, one);
            right = supportLexicographic(entryState, frontierLayer, zero, one, one, zero);
        }

        typename ParetoFront::container_type points{left};
        if (left.probability == right.probability) {
            return ParetoFront(std::move(points));
        }
        STORM_LOG_THROW(left.probability < right.probability, storm::exceptions::UnexpectedException, "Invalid zero-weight entry frontier endpoints.");
        points.push_back(right);
        std::vector<std::pair<Point, Point>> pending{{left, right}};
        while (!pending.empty()) {
            auto const [start, end] = pending.back();
            pending.pop_back();
            ValueType slope;
            if constexpr (FrontKind == SspParetoFrontKind::CostUpperTail) {
                slope = (end.expectedCost - start.expectedCost) / (end.probability - start.probability);
            } else {
                slope = (start.expectedCost - end.expectedCost) / (end.probability - start.probability);
            }
            STORM_LOG_THROW(storm::utility::isNonNegative(slope), storm::exceptions::UnexpectedException, "Invalid zero-weight entry frontier slope.");
            auto const candidate = support(entryState, frontierLayer, slope).point;
            ValueType const improvement = supportScore(start, slope) - supportScore(candidate, slope);
            if constexpr (std::is_floating_point_v<ValueType>) {
                if (storm::utility::isAlmostZero(improvement)) {
                    continue;
                }
            }
            STORM_LOG_THROW(storm::utility::isNonNegative(improvement), storm::exceptions::UnexpectedException,
                            "A zero-weight entry support query exceeded its known frontier edge.");
            if (storm::utility::isPositive(improvement)) {
                STORM_LOG_THROW(start.probability < candidate.probability && candidate.probability < end.probability, storm::exceptions::UnexpectedException,
                                "Invalid zero-weight entry frontier support point.");
                points.push_back(candidate);
                pending.emplace_back(candidate, end);
                pending.emplace_back(start, candidate);
            }
        }
        return ParetoFront(std::move(points));
    }

   private:
    Point supportLexicographic(uint64_t entryState, FrontierLayer const& frontierLayer, ValueType const& primaryProbability,
                               ValueType const& primaryExpectedCost, ValueType const& secondaryProbability, ValueType const& secondaryExpectedCost) {
        auto const& boundaryStates = componentLp.getBoundaryStates();
        STORM_LOG_THROW(frontierLayer.size() > boundaryStates.back(), storm::exceptions::InvalidArgumentException,
                        "Expected a current-threshold frontier for every boundary state.");

        std::vector<Point> selectedPoints;
        std::vector<ValueType> primaryDirections;
        std::vector<ValueType> secondaryDirections;
        selectedPoints.reserve(boundaryStates.size());
        primaryDirections.reserve(boundaryStates.size());
        secondaryDirections.reserve(boundaryStates.size());
        for (uint64_t boundaryState : boundaryStates) {
            auto const& front = frontierLayer[boundaryState];
            STORM_LOG_THROW(!front.empty(), storm::exceptions::InvalidArgumentException, "Expected a nonempty boundary frontier.");

            Point selected = *front.begin();
            ValueType primary = primaryProbability * selected.probability + primaryExpectedCost * selected.expectedCost;
            ValueType secondary = secondaryProbability * selected.probability + secondaryExpectedCost * selected.expectedCost;
            for (auto const& point : front) {
                auto const candidatePrimary = primaryProbability * point.probability + primaryExpectedCost * point.expectedCost;
                auto const candidateSecondary = secondaryProbability * point.probability + secondaryExpectedCost * point.expectedCost;
                if (candidatePrimary < primary || (candidatePrimary == primary && candidateSecondary < secondary)) {
                    selected = point;
                    primary = candidatePrimary;
                    secondary = candidateSecondary;
                }
            }
            selectedPoints.push_back(selected);
            primaryDirections.push_back(-primary);
            secondaryDirections.push_back(-secondary);
        }

        auto const primaryResult = componentLp.maximize(entryState, primaryDirections);
        auto const result = componentLp.maximizeWithFixedScore(entryState, secondaryDirections, primaryDirections, primaryResult.objectiveValue);
        Point point{storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>()};
        for (uint64_t index = 0; index < boundaryStates.size(); ++index) {
            point.probability += result.boundaryProbabilities[index] * selectedPoints[index].probability;
            point.expectedCost += result.boundaryProbabilities[index] * selectedPoints[index].expectedCost;
        }
        return point;
    }

    static ValueType supportScore(Point const& point, ValueType const& slope) {
        if constexpr (FrontKind == SspParetoFrontKind::CostUpperTail) {
            return point.expectedCost - slope * point.probability;
        } else {
            return point.expectedCost + slope * point.probability;
        }
    }

    storm::transformer::ZeroWeightComponentLp<ValueType> componentLp;
};

}  // namespace storm::modelchecker::cvar
