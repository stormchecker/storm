#pragma once

#include <vector>

#include "storm-pomdp/beliefs/storage/BeliefBuilder.h"
#include "storm-pomdp/beliefs/utility/BeliefNumerics.h"
#include "storm-pomdp/beliefs/utility/types.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::pomdp::beliefs {
/** Selects a fixed grid resolution or a suitable coarser resolution per belief. */
enum class FreudenthalTriangulationMode { Static, Dynamic };

/*!
 * Abstracts a belief by triangulating it using the Freudenthal triangulation.
 * Intuitively, the Freudenthal triangulation considers a grid (or: foundation) of beliefs that assign probabilities from the set {0/N, 1/N, 2/N, ..., N/N} for
 * some resolution N. The freudenthal belief abstraction then represents a given belief by a convex combination over nearby grid-beliefs.
 * In the static mode, the resolution is fixed.
 * In the dynamic mode, a lower resolution is chosen, if it fits the belief significantly better. For example, if the original belief is {state1: 1/3, state2:
 * 2/3 } and the given resolution is N=4, it will be triangulated using N'=3 instead.
 *
 * @see 10.1007/978-3-030-59152-6_16
 *
 * @tparam BeliefType Sparse belief representation to abstract.
 */
template<typename BeliefType>
class FreudenthalTriangulationBeliefAbstraction {
   public:
    using BeliefValueType = typename BeliefType::ValueType;

    /** Creates an abstraction with the default grid resolution and resolution-selection mode. */
    FreudenthalTriangulationBeliefAbstraction(BeliefValueType const& initialResolution, FreudenthalTriangulationMode mode);

    /**
     * Emits the grid beliefs and weights whose convex combination represents @p belief.
     *
     * The emitted weights include @p probabilityFactor.
     */
    template<typename AbstractCallback>
    void abstract(BeliefType&& belief, BeliefValueType&& probabilityFactor, AbstractCallback const& callback) const {
        // Quickly triangulate Dirac beliefs
        if (belief.size() == 1u) {
            callback(std::move(belief), std::move(probabilityFactor));
        } else {
            auto observationResolutionFindRes = observationResolutions.find(belief.observation());
            auto observationResolution =
                observationResolutionFindRes != observationResolutions.end() ? observationResolutionFindRes->second : defaultResolution;
            switch (mode) {
                case FreudenthalTriangulationMode::Static:
                    abstractStatic(probabilityFactor, belief, callback, observationResolution);
                    break;
                case FreudenthalTriangulationMode::Dynamic:
                    abstractDynamic(probabilityFactor, belief, callback, observationResolution);
                    break;
                default:
                    STORM_LOG_ASSERT(false, "Invalid triangulation mode.");
            }
        }
    }

   private:
    template<typename AbstractCallback>
    void abstractStatic(BeliefValueType const& probabilityFactor, BeliefType const& belief, AbstractCallback& callback,
                        BeliefValueType const& staticResolution) const {
        struct FreudenthalDiff {
            BeliefValueType diff;       // d[i]
            BeliefStateType dimension;  // i
            bool operator>(FreudenthalDiff const& other) const {
                if (diff != other.diff) {
                    return diff > other.diff;
                } else {
                    return dimension < other.dimension;
                }
            }
        };

        STORM_LOG_ASSERT(storm::utility::isInteger(staticResolution), "Expected an integer resolution");
        STORM_LOG_ASSERT(staticResolution > 0, "Expected a positive resolution");
        BeliefStateType numEntries = belief.size();
        // This is the Freudenthal Triangulation as described in Lovejoy (a whole lotta math)
        // Probabilities will be triangulated to values in 0/N, 1/N, 2/N, ..., N/N
        // Variable names are mostly based on the paper
        // However, we speed this up a little by exploiting that belief states usually have sparse support (i.e. numEntries is much smaller than
        // pomdp.getNumberOfStates()). Initialize diffs and the first row of the 'qs' matrix (aka v)
        std::set<FreudenthalDiff, std::greater<>> sorted_diffs;  // d (and p?) in the paper
        std::vector<BeliefValueType> qsRow;                      // Row of the 'qs' matrix from the paper (initially corresponds to v
        qsRow.reserve(numEntries);
        std::vector<BeliefStateType> toOriginalIndicesMap;  // Maps 'local' indices to the original pomdp state indices
        toOriginalIndicesMap.reserve(numEntries);
        BeliefValueType x = staticResolution;
        belief.forEach([&qsRow, &sorted_diffs, &toOriginalIndicesMap, &x, &staticResolution](auto const& state, auto const& value) {
            qsRow.push_back(storm::utility::floor(x));                                              // v
            sorted_diffs.insert(FreudenthalDiff({x - qsRow.back(), toOriginalIndicesMap.size()}));  // x-v
            toOriginalIndicesMap.push_back(state);
            x -= value * staticResolution;
        });
        // Insert a dummy 0 column in the qs matrix so the loops below are a bit simpler
        qsRow.push_back(storm::utility::zero<BeliefValueType>());

        auto currentSortedDiff = sorted_diffs.begin();
        auto previousSortedDiff = sorted_diffs.end();
        --previousSortedDiff;
        for (BeliefStateType i = 0; i < numEntries; ++i) {
            // Compute the weight for the grid points
            BeliefValueType weight = previousSortedDiff->diff - currentSortedDiff->diff;
            if (i == 0) {
                // The first weight is a bit different
                weight += storm::utility::one<BeliefValueType>();
            } else {
                // 'compute' the next row of the qs matrix
                qsRow[previousSortedDiff->dimension] += storm::utility::one<BeliefValueType>();
            }
            if (!BeliefNumerics<BeliefValueType>::isZero(weight)) {
                // build the grid point
                BeliefBuilder<BeliefType> builder;
                builder.reserve(numEntries);
                builder.setObservation(belief.observation());
                for (BeliefStateType j = 0; j < numEntries; ++j) {
                    BeliefValueType gridPointEntry = qsRow[j] - qsRow[j + 1];
                    if (!BeliefNumerics<BeliefValueType>::isZero(gridPointEntry)) {
                        builder.addValue(toOriginalIndicesMap[j], gridPointEntry / staticResolution);
                    }
                }
                callback(builder.build(), static_cast<BeliefValueType>(weight * probabilityFactor));
            }
            previousSortedDiff = currentSortedDiff++;
        }
    }

    template<typename AbstractCallback>
    void abstractDynamic(BeliefValueType const& probabilityFactor, BeliefType const& belief, AbstractCallback& callback,
                         BeliefValueType const& maxResolution) const {
        // Find the best resolution for this belief, i.e., N such that the largest distance between one of the belief values to a value in {i/N | 0 ≤ i ≤ N} is
        // minimal
        STORM_LOG_ASSERT(storm::utility::isInteger(maxResolution), "Expected an integer resolution");
        BeliefValueType const halfResolution = maxResolution / storm::utility::convertNumber<BeliefValueType, uint64_t>(2);
        BeliefValueType const initialDist = storm::utility::one<BeliefValueType>() / maxResolution;
        // We take 1/maxResolution as initial distance. This means that coarser maxResolution are only chosen if they are clearly better suited for the given
        // belief.

        BeliefValueType resolution = maxResolution;
        BeliefValueType resolutionDist = initialDist;
        // We don't need to check resolutions that are smaller than the maximal resolution divided by 2 as we already checked multiples of these
        for (BeliefValueType currResolution = maxResolution; currResolution > halfResolution; --currResolution) {
            BeliefValueType currDist = storm::utility::zero<BeliefValueType>();
            bool const newBest = belief.allOf([&currDist, &currResolution, &resolutionDist](BeliefStateType const&, BeliefValueType const& val) {
                currDist += storm::utility::abs<BeliefValueType>(val - storm::utility::round<BeliefValueType>(val * currResolution) / currResolution);
                return currDist <= resolutionDist;  // continue as long as the current dist is still smaller than the smallest dist
            });
            if (newBest) {
                STORM_LOG_ASSERT(currDist <= resolutionDist, "Expected a smaller resolution");
                resolution = currResolution;
                resolutionDist = currDist;
                if (BeliefNumerics<BeliefValueType>::isZero(resolutionDist)) {
                    break;
                }
            }
        }
        abstractStatic(probabilityFactor, belief, callback, resolution);
    }

   private:
    BeliefValueType const defaultResolution;
    FreudenthalTriangulationMode const mode;
    std::map<BeliefObservationType, BeliefValueType> observationResolutions;
};
}  // namespace storm::pomdp::beliefs
