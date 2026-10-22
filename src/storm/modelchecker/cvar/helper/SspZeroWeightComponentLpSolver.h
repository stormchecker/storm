#pragma once

#include <cstdint>
#include <utility>
#include <vector>

#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidArgumentException.h"
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

   private:
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
