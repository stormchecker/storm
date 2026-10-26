#include "storm-pars/scp/ScpLpEncoder.h"

#include <algorithm>
#include <optional>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/expressions/Expression.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"

namespace storm::pars::scp {

ScpLpEncoder::ScpLpEncoder(AffinePMdpDecomposition const& decomposition, std::vector<uint64_t> rowGroupIndices, storm::storage::BitVector zeroStates,
                           storm::storage::BitVector oneStates, storm::storage::BitVector infiniteStates, storm::storage::BitVector excludedRows,
                           uint64_t initialState, std::map<storm::RationalFunctionVariable, std::pair<double, double>> parameterBounds, double threshold,
                           ThresholdDirection direction, PropertyKind propertyKind, ScpOptions options)
    : decomposition(decomposition),
      rowGroupIndices(std::move(rowGroupIndices)),
      zeroStates(std::move(zeroStates)),
      oneStates(std::move(oneStates)),
      infiniteStates(std::move(infiniteStates)),
      excludedRows(std::move(excludedRows)),
      initialState(initialState),
      parameterBounds(std::move(parameterBounds)),
      threshold(threshold),
      direction(direction),
      propertyKind(propertyKind),
      options(options) {
    STORM_LOG_ASSERT(!this->zeroStates.get(initialState) && !this->oneStates.get(initialState) && !this->infiniteStates.get(initialState),
                     "The initial state's value must not be a priori known; the caller should short-circuit before building an LP in that case.");
    STORM_LOG_ASSERT(propertyKind != PropertyKind::Reward || this->oneStates.getNumberOfSetBits() == 0,
                     "A reward property has no 'known value 1' states; oneStates must be empty.");
}

EncodedLp ScpLpEncoder::encode(storm::Environment const& env, std::map<storm::RationalFunctionVariable, double> const& valuation,
                               std::vector<double> const& stateValues, double trustRegionRadius) const {
    STORM_LOG_ASSERT(trustRegionRadius >= 1.0, "Trust region radius must be >= 1.");
    uint64_t const numberOfStates = rowGroupIndices.size() - 1;

    std::unique_ptr<storm::solver::LpSolver<double>> solver = storm::utility::solver::getLpSolver<double>(env, "scp");

    EncodedLp result;
    result.stateVariables.resize(numberOfStates);

    // Parameter variables, trust-region-bounded within the outer search region.
    for (auto const& [var, bounds] : parameterBounds) {
        double vHat = valuation.at(var);
        double lb = std::max(bounds.first, vHat / trustRegionRadius);
        double ub = std::min(bounds.second, vHat * trustRegionRadius);
        result.parameterVariables.emplace(var, solver->addBoundedContinuousVariable("v_" + var.name(), lb, ub));
    }

    // State variables exist only for states without a known value. Probability variables are capped
    // by 1; the trust region bounds both kinds.
    for (uint64_t s = 0; s < numberOfStates; ++s) {
        if (zeroStates.get(s) || oneStates.get(s) || infiniteStates.get(s)) {
            continue;
        }
        double pHat = stateValues.at(s);
        double lb = std::max(0.0, pHat / trustRegionRadius);
        double ub = pHat * trustRegionRadius;
        if (propertyKind == PropertyKind::Probability) {
            ub = std::min(1.0, ub);
        }
        double objectiveCoefficient = 0.0;
        if (s == initialState) {
            objectiveCoefficient = (direction == ThresholdDirection::AtLeast) ? -options.initialStateBias : options.initialStateBias;
        }
        result.stateVariables[s] = solver->addBoundedContinuousVariable("p_" + std::to_string(s), lb, ub, objectiveCoefficient);
    }

    // Per-state slack (violation) variables, and (if configured) the shared max-violation variable.
    std::vector<storm::expressions::Variable> tau(numberOfStates);
    for (uint64_t s = 0; s < numberOfStates; ++s) {
        if (zeroStates.get(s) || oneStates.get(s) || infiniteStates.get(s)) {
            continue;
        }
        double objectiveCoefficient = options.useMaxViolationObjective ? 0.0 : 1.0;
        tau[s] = solver->addLowerBoundedContinuousVariable("tau_" + std::to_string(s), 0.0, objectiveCoefficient);
    }
    storm::expressions::Variable maxViolation;
    if (options.useMaxViolationObjective) {
        maxViolation = solver->addLowerBoundedContinuousVariable("max_violation", 0.0, 1.0);
    }

    solver->update();

    // The linearized contribution of one transition to a Bellman right-hand side: zero successors
    // contribute nothing, one successors their exact affine value, and other successors the
    // first-order expansion of P(v) * p_succ around (valuation, stateValues). Rows with infinite
    // successors are excluded before this is called.
    auto successorContribution = [&](UnpackedTransition const& ut) -> std::optional<storm::expressions::Expression> {
        if (zeroStates.get(ut.successor)) {
            return std::nullopt;
        }
        if (oneStates.get(ut.successor)) {
            storm::expressions::Expression expr = solver->getConstant(ut.constant);
            for (auto const& [var, coeff] : ut.parameterCoefficients) {
                expr = expr + result.parameterVariables.at(var).getExpression() * solver->getConstant(coeff);
            }
            return expr;
        }
        STORM_LOG_ASSERT(!infiniteStates.get(ut.successor), "A row with an infinite-value successor was not excluded before building its Bellman constraint.");
        double pHatSucc = stateValues.at(ut.successor);
        storm::expressions::Expression pSucc = result.stateVariables.at(ut.successor).getExpression();
        storm::expressions::Expression expr = pSucc * solver->getConstant(ut.constant);
        for (auto const& [var, coeff] : ut.parameterCoefficients) {
            double vHat = valuation.at(var);
            storm::expressions::Expression v = result.parameterVariables.at(var).getExpression();
            // First-order Taylor expansion of v * p_succ around (vHat, pHatSucc):
            //   vHat*pHatSucc + pHatSucc*(v - vHat) + vHat*(p_succ - pHatSucc) = pHatSucc*v + vHat*p_succ - vHat*pHatSucc
            storm::expressions::Expression bilinear =
                v * solver->getConstant(pHatSucc) + pSucc * solver->getConstant(vHat) - solver->getConstant(vHat * pHatSucc);
            expr = expr + bilinear * solver->getConstant(coeff);
        }
        return expr;
    };

    for (uint64_t s = 0; s < numberOfStates; ++s) {
        if (zeroStates.get(s) || oneStates.get(s) || infiniteStates.get(s)) {
            continue;
        }
        for (uint64_t row = rowGroupIndices[s]; row < rowGroupIndices[s + 1]; ++row) {
            if (excludedRows.get(row)) {
                continue;
            }
            std::vector<storm::expressions::Expression> summands;
            for (auto const& transition : decomposition.getRow(row)) {
                if (auto contribution = successorContribution(transition)) {
                    summands.push_back(*contribution);
                }
            }
            if (propertyKind == PropertyKind::Reward) {
                UnpackedValue const& reward = decomposition.getReward(row);
                storm::expressions::Expression rewardExpr = solver->getConstant(reward.constant);
                for (auto const& [var, coeff] : reward.parameterCoefficients) {
                    rewardExpr = rewardExpr + result.parameterVariables.at(var).getExpression() * solver->getConstant(coeff);
                }
                summands.push_back(rewardExpr);
            }
            storm::expressions::Expression rhs = summands.empty() ? solver->getConstant(0.0) : storm::expressions::sum(summands);
            // AtMost: "p_s >= rhs - tau_s" for every action, minimized -> the least supersolution
            // (Pmax; the max scheduler's best action survives the minimization).
            // AtLeast: "p_s <= rhs + tau_s" for every action, maximized -> the greatest subsolution
            // (Pmin; the min scheduler's worst action survives the maximization).
            if (direction == ThresholdDirection::AtLeast) {
                solver->addConstraint("", result.stateVariables[s].getExpression() <= rhs + tau[s].getExpression());
            } else {
                solver->addConstraint("", result.stateVariables[s].getExpression() >= rhs - tau[s].getExpression());
            }
            if (options.useMaxViolationObjective) {
                solver->addConstraint("", tau[s].getExpression() <= maxViolation.getExpression());
            }
        }
    }

    // Threshold constraint on the initial state, sharing its Bellman constraint's slack variable.
    storm::expressions::Expression initialStateValue = result.stateVariables[initialState].getExpression();
    if (direction == ThresholdDirection::AtLeast) {
        solver->addConstraint("", initialStateValue + tau[initialState].getExpression() >= solver->getConstant(threshold));
    } else {
        solver->addConstraint("", initialStateValue <= solver->getConstant(threshold) + tau[initialState].getExpression());
    }

    result.solver = std::move(solver);
    return result;
}

}  // namespace storm::pars::scp
