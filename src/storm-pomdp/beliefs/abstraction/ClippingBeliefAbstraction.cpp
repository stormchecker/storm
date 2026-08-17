#include "storm-pomdp/beliefs/abstraction/ClippingBeliefAbstraction.h"

#include "storm-pomdp/beliefs/storage/Belief.h"

#include "storm-pomdp/beliefs/storage/BeliefBuilder.h"
#include "storm-pomdp/beliefs/utility/BeliefNumerics.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/solver/LpSolver.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/solver.h"

namespace storm::pomdp::beliefs {

template<typename BeliefType>
ClippingBeliefAbstraction<BeliefType>::ClippingBeliefAbstraction(storm::Environment const& env, std::vector<uint64_t>&& observationResolutions)
    : observationResolutions(std::forward<std::vector<uint64_t>>(observationResolutions)) {
    STORM_LOG_ASSERT(std::all_of(this->observationResolutions.begin(), this->observationResolutions.end(), [](auto o) { return o > 0; }),
                     "Expected that the resolutions are positive.");
    lpSolver = storm::utility::solver::getLpSolver<BeliefValueType>(env, "POMDP LP Solver");
    lpSolver->push();
}

template<typename BeliefType>
ClippingBeliefAbstraction<BeliefType>::ClippingBeliefAbstraction(storm::Environment const& env, std::vector<uint64_t>&& observationResolutions,
                                                                 std::vector<BeliefValueType>&& extremalRewardValues)
    : observationResolutions(std::forward<std::vector<uint64_t>>(observationResolutions)),
      extremalRewardValues(std::forward<std::vector<BeliefValueType>>(extremalRewardValues)) {
    STORM_LOG_ASSERT(std::all_of(this->observationResolutions.begin(), this->observationResolutions.end(), [](auto o) { return o > 0; }),
                     "Expected that the resolutions are positive.");
    lpSolver = storm::utility::solver::getLpSolver<BeliefValueType>(env, "POMDP LP Solver");
    lpSolver->push();
}

template<typename BeliefType>
typename ClippingBeliefAbstraction<BeliefType>::BeliefClipping ClippingBeliefAbstraction<BeliefType>::clipBeliefToGrid(const BeliefType& belief,
                                                                                                                       const uint64_t resolution) {
    lpSolver->pop();
    lpSolver->push();

    auto const resolutionConverted = storm::utility::convertNumber<BeliefValueType>(resolution);

    std::vector<BeliefValueType> helper(belief.size(), storm::utility::zero<BeliefValueType>());
    helper[0] = resolutionConverted;
    bool done = false;
    // Set-up Variables
    std::vector<storm::expressions::Expression> decisionVariables;
    // Add variable for the clipping value, it is to be minimized
    auto bigDelta = lpSolver->addBoundedContinuousVariable("D", storm::utility::zero<BeliefValueType>(), storm::utility::one<BeliefValueType>(),
                                                           storm::utility::one<BeliefValueType>());
    // State clipping values
    std::vector<storm::expressions::Expression> deltas;
    uint64_t i = 0;
    belief.forEach([this, &deltas, &i](BeliefStateType const& state, BeliefValueType const& beliefValue) {
        auto localDelta = lpSolver->addBoundedContinuousVariable("d_" + std::to_string(i), storm::utility::zero<BeliefValueType>(), beliefValue);
        deltas.push_back(storm::expressions::Expression(localDelta));
        ++i;
    });
    lpSolver->update();
    std::vector<BeliefType> gridCandidates;
    while (!done) {
        BeliefBuilder<BeliefType> candidateBuilder;
        candidateBuilder.setObservation(belief.observation());

        uint64_t j{0};
        uint64_t const jMax = belief.size() - 1;
        belief.forEach([&helper, &j, &resolutionConverted, &candidateBuilder, &jMax](BeliefStateType const& state, BeliefValueType const& beliefValue) {
            if (j < jMax) {
                if (!BeliefNumerics<BeliefValueType>::isZero(helper[j] - helper[j + 1])) {
                    candidateBuilder.addValue(state, (helper[j] - helper[j + 1]) / resolutionConverted);
                }
            } else {
                if (!BeliefNumerics<BeliefValueType>::isZero(helper[jMax])) {
                    candidateBuilder.addValue(state, helper[jMax] / resolutionConverted);
                }
            }
            ++j;
        });
        auto candidate = candidateBuilder.build();
        if (candidate == belief) {
            STORM_LOG_TRACE(belief.toString() << " on clipping grid.");
            // TODO Improve handling of successors which are already on the grid
            return BeliefClipping{false, std::move(candidate), storm::utility::zero<BeliefValueType>(), {}, true};
        } else {
            gridCandidates.push_back(candidate);

            // Add variables a_j
            auto decisionVar = lpSolver->addBinaryVariable("a_" + std::to_string(gridCandidates.size() - 1));
            decisionVariables.push_back(storm::expressions::Expression(decisionVar));
            lpSolver->update();

            i = 0;
            belief.forEachCombine(candidate, [&](BeliefStateType const& state, BeliefValueType const& beliefValue, BeliefValueType const& candidateValue) {
                // Add the constraint to describe the transformation between the state values in the beliefs
                // Add d_i >= b(s_i) - b_j(s_i) + D * b_j(s_i) - 1 + a_j
                lpSolver->addConstraint("state_eq_" + std::to_string(i) + "_" + std::to_string(gridCandidates.size() - 1),
                                        deltas.at(i) >= lpSolver->getConstant(beliefValue) - lpSolver->getConstant(candidateValue) +
                                                            storm::expressions::Expression(bigDelta) * lpSolver->getConstant(candidateValue) -
                                                            lpSolver->getConstant(storm::utility::one<BeliefValueType>()) +
                                                            storm::expressions::Expression(decisionVar));
                ++i;
                lpSolver->update();
            });
        }
        if (helper.back() == storm::utility::convertNumber<BeliefValueType>(resolution)) {
            // If the last entry of helper is the gridResolution, we have enumerated all necessary distributions
            done = true;
        } else {
            // Update helper by finding the index to increment
            auto helperIt = helper.end() - 1;
            while (*helperIt == *(helperIt - 1)) {
                --helperIt;
            }
            STORM_LOG_ASSERT(helperIt != helper.begin(), "Error in grid clipping - index wrong");
            // Increment the value at the index
            *helperIt += 1;
            // Reset all indices greater than the changed one to 0
            ++helperIt;
            while (helperIt != helper.end()) {
                *helperIt = 0;
                ++helperIt;
            }
        }
    }

    // Only one target belief should be chosen
    lpSolver->addConstraint("choice", storm::expressions::sum(decisionVariables) == lpSolver->getConstant(storm::utility::one<BeliefValueType>()));
    // Link D and d_i
    lpSolver->addConstraint("delta", storm::expressions::Expression(bigDelta) == storm::expressions::sum(deltas));
    // Exclude D = 0 (self-loop)
    lpSolver->addConstraint("not_zero", storm::expressions::Expression(bigDelta) > lpSolver->getConstant(storm::utility::zero<BeliefValueType>()));

    lpSolver->update();

    lpSolver->optimize();
    // Get the optimal belief for clipping
    // Not a belief but has the same type
    BeliefFlatMap<BeliefValueType> deltaValues;
    auto optDelta = storm::utility::zero<BeliefValueType>();
    auto deltaSum = storm::utility::zero<BeliefValueType>();
    if (lpSolver->isOptimal()) {
        uint64_t targetBeliefIndex = std::numeric_limits<uint64_t>::max();
        optDelta = lpSolver->getObjectiveValue();
        for (uint64_t dist = 0; dist < gridCandidates.size(); ++dist) {
            if (lpSolver->getBinaryValue(lpSolver->getManager().getVariable("a_" + std::to_string(dist)))) {
                targetBeliefIndex = dist;
                break;
            }
        }
        STORM_LOG_ASSERT(targetBeliefIndex < gridCandidates.size(), "LP optimal but no belief selected");
        auto targetBelief = gridCandidates.at(targetBeliefIndex);
        i = 0;
        belief.forEachStateInSupport([this, &i, &deltaValues, &deltaSum](BeliefStateType const& state) {
            auto val = lpSolver->getContinuousValue(lpSolver->getManager().getVariable("d_" + std::to_string(i)));
            if (!BeliefNumerics<BeliefValueType>::lessOrEqual(val, storm::utility::zero<BeliefValueType>())) {
                deltaValues.emplace(state, val);
                deltaSum += val;
            }
            ++i;
        });

        if (BeliefNumerics<BeliefValueType>::isZero(optDelta)) {
            // If we get an optimal value of 0, the LP solver considers two beliefs to be equal, possibly due to numerical instability
            // For a sound result, we consider the state to not be clippable
            STORM_LOG_WARN("LP solver returned an optimal value of 0. This should definitely not happen when using a grid");
            STORM_LOG_WARN("Origin" << belief.toString());
            STORM_LOG_WARN("Target [Bel " << targetBelief.toString());
            return BeliefClipping{false, std::move(targetBelief), storm::utility::zero<BeliefValueType>(), {}, false};
        }

        if (optDelta == storm::utility::one<BeliefValueType>()) {
            STORM_LOG_WARN("LP solver returned an optimal value of 1. Sum of state clipping values is " << deltaSum);
            // If we get an optimal value of 1, we cannot clip the belief as by definition this would correspond to a division by 0.
            STORM_LOG_DEBUG("Origin " << belief.toString());
            STORM_LOG_DEBUG("Target " << targetBelief.toString());

            if (deltaSum == storm::utility::one<BeliefValueType>()) {
                return BeliefClipping{false, std::move(targetBelief), storm::utility::zero<BeliefValueType>(), {}, false};
            }
            optDelta = deltaSum;
        }
        STORM_LOG_TRACE("Clip " << belief.toString() << " to " << targetBelief.toString() << " with value " << optDelta);
        return BeliefClipping{true, std::move(targetBelief), optDelta, deltaValues, false};
    }
    STORM_LOG_TRACE("Clipping " << belief.toString() << " not possible. LP not optimal.");
    return BeliefClipping{false, belief, optDelta, deltaValues, false};
}

template class ClippingBeliefAbstraction<Belief<double>>;
template class ClippingBeliefAbstraction<Belief<storm::RationalNumber>>;

}  // namespace storm::pomdp::beliefs
