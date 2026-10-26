#pragma once

#include <cstdint>

namespace storm::pars::scp {

/*!
 * Configuration for the SCP feasibility search (SparseScpInstantiationSearcher).
 *
 * The trust region radius is a multiplicative factor >= 1: a variable may move to at most
 * radius * v_hat and at least v_hat / radius, where v_hat is its value at the current linearization
 * point. The radius shrinks towards 1 on rejected steps, so the search stops once radius - 1
 * falls below trustRegionMinExcess. A variable whose linearization value is exactly 0 has bounds
 * [0, 0] and cannot move in later iterations.
 */
struct ScpOptions {
    /// Initial trust region radius (delta_0).
    double trustRegionInitial = 3.0;

    /// Multiplicative factor (gamma) by which the trust region grows after an accepted step and
    /// shrinks after a rejected one.
    double trustRegionFactor = 1.5;

    /// Upper cap on the trust region radius.
    double trustRegionMax = 10.0;

    /// Termination threshold on (radius - 1).
    double trustRegionMinExcess = 1e-4;

    /// Maximum number of SCP iterations before giving up.
    uint64_t maxIterations = 1000;

    /// Weight of the initial state's value in the LP objective. It is dominated by the violation objective and only breaks ties towards the threshold side.
    double initialStateBias = 1e-4;
    /// If true, the LP objective minimizes the maximum per-state constraint violation; if false, the sum of violations.
    bool useMaxViolationObjective = true;
};

}  // namespace storm::pars::scp
