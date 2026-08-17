#include "FailureBoundFinder.h"

namespace storm::dft {
namespace utility {

uint64_t FailureBoundFinder::correctLowerBound(std::shared_ptr<storm::dft::modelchecker::DFTASFChecker> smtchecker, uint64_t bound, uint_fast64_t timeout) {
    STORM_LOG_DEBUG("Lower bound correction - try to correct bound " << std::to_string(bound));
    uint64_t boundCandidate = bound;
    uint64_t nrDepEvents = 0;
    uint64_t nrNonMarkovian = 0;
    auto dft = smtchecker->getDFT();

    // Count dependent events
    for (size_t i = 0; i < dft.nrElements(); ++i) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<double> const> element = dft.getElement(i);
        if (element->isBasicElement()) {
            auto be = std::static_pointer_cast<storm::dft::storage::elements::DFTBE<double> const>(element);
            if (be->hasIngoingDependencies()) {
                ++nrDepEvents;
            }
        }
    }

    // Only need to check as long as bound candidate + nr of non-Markovians to check is smaller than number of dependent events
    // Check boundCandidate == 0 is later performed in the switch case
    while (nrNonMarkovian <= nrDepEvents) {
        if (nrNonMarkovian == 0 and boundCandidate == 0) {
            nrNonMarkovian = 1;
        }
        STORM_LOG_TRACE("Lower bound correction - check possible bound " << std::to_string(boundCandidate) << " with " << std::to_string(nrNonMarkovian)
                                                                         << " non-Markovian states");
        // The uniqueness transformation for constantly failed BEs guarantees that a DFT never fails
        // in step 0 without intermediate non-Markovians, thus forcibly set nrNonMarkovian
        smtchecker->setSolverTimeout(timeout * 1000);
        storm::solver::SmtSolver::CheckResult tmp_res = smtchecker->checkFailsLeqWithEqNonMarkovianState(boundCandidate + nrNonMarkovian, nrNonMarkovian);
        smtchecker->unsetSolverTimeout();
        switch (tmp_res) {
            case storm::solver::SmtSolver::CheckResult::Sat:
                /* If SAT, there is a sequence where only boundCandidate-many BEs fail directly and rest is nonMarkovian.
                 * Bound candidate is vaild, therefore check the next one */
                STORM_LOG_TRACE("Lower bound correction - SAT");
                // Prevent integer underflow
                if (boundCandidate == 0) {
                    STORM_LOG_DEBUG("Lower bound correction - corrected bound to 0");
                    return 0;
                }
                --boundCandidate;
                break;
            case storm::solver::SmtSolver::CheckResult::Unknown:
                // If any query returns unknown, we cannot be sure about the bound and fall back to the naive one
                STORM_LOG_DEBUG("Lower bound correction - Solver returned 'Unknown', corrected to 1");
                return 1;
            default:
                // if query is UNSAT, increase number of non-Markovian states and try again
                STORM_LOG_TRACE("Lower bound correction - UNSAT");
                ++nrNonMarkovian;
                break;
        }
    }
    // if for one candidate all queries are UNSAT, it is not valid. Return last valid candidate
    STORM_LOG_DEBUG("Lower bound correction - corrected bound to " << std::to_string(boundCandidate + 1));
    return boundCandidate + 1;
}

uint64_t FailureBoundFinder::correctUpperBound(std::shared_ptr<storm::dft::modelchecker::DFTASFChecker> smtchecker, uint64_t bound, uint_fast64_t timeout) {
    STORM_LOG_DEBUG("Upper bound correction - try to correct bound " << std::to_string(bound));
    uint64_t boundCandidate = bound;
    uint64_t nrNonMarkovian = 0;
    uint64_t currentTimepoint = 0;
    auto dft = smtchecker->getDFT();

    while (boundCandidate > 0) {
        currentTimepoint = bound + 1;
        while (currentTimepoint - boundCandidate > 0) {
            --currentTimepoint;
            nrNonMarkovian = currentTimepoint - boundCandidate;
            STORM_LOG_TRACE("Upper bound correction - candidate " << std::to_string(boundCandidate) << " check split " << std::to_string(currentTimepoint)
                                                                  << "|" << std::to_string(nrNonMarkovian));
            smtchecker->setSolverTimeout(timeout * 1000);
            storm::solver::SmtSolver::CheckResult tmp_res = smtchecker->checkFailsAtTimepointWithEqNonMarkovianState(currentTimepoint, nrNonMarkovian);
            smtchecker->unsetSolverTimeout();
            switch (tmp_res) {
                case storm::solver::SmtSolver::CheckResult::Sat:
                    STORM_LOG_TRACE("Upper bound correction - SAT");
                    STORM_LOG_DEBUG("Upper bound correction - corrected to bound " << boundCandidate << " (TLE can fail at sequence point "
                                                                                   << std::to_string(currentTimepoint) << " with "
                                                                                   << std::to_string(nrNonMarkovian) << " non-Markovian states)");
                    return boundCandidate;
                case storm::solver::SmtSolver::CheckResult::Unknown:
                    // If any query returns unknown, we cannot be sure about the bound and fall back to the naive one
                    STORM_LOG_DEBUG("Upper bound correction - Solver returned 'Unknown', corrected to bound " << bound);
                    return bound;
                default:
                    // if query is UNSAT, increase number of non-Markovian states and try again
                    STORM_LOG_TRACE("Lower bound correction - UNSAT");
                    break;
            }
        }
        --boundCandidate;
    }

    // if for one candidate all queries are UNSAT, it is not valid. Return last valid candidate
    STORM_LOG_DEBUG("Upper bound correction - corrected bound to " << std::to_string(boundCandidate));
    return boundCandidate;
}

uint64_t FailureBoundFinder::getLeastFailureBound(storm::dft::storage::DFT<double> const &dft, bool useSMT, uint_fast64_t timeout) {
    if (useSMT) {
        STORM_LOG_TRACE("Compute lower bound for number of BE failures necessary for the DFT to fail");

        storm::dft::modelchecker::DFTASFChecker smtchecker(dft);
        smtchecker.toSolver();

        uint64_t bound = 0;
        while (bound < dft.nrBasicElements() + 1) {
            smtchecker.setSolverTimeout(timeout * 1000);
            storm::solver::SmtSolver::CheckResult tmp_res = smtchecker.checkTleFailsWithLeq(bound);
            smtchecker.unsetSolverTimeout();
            switch (tmp_res) {
                case storm::solver::SmtSolver::CheckResult::Sat:
                    if (!dft.getDependencies().empty()) {
                        return correctLowerBound(std::make_shared<storm::dft::modelchecker::DFTASFChecker>(smtchecker), bound, timeout);
                    } else {
                        return bound;
                    }
                case storm::solver::SmtSolver::CheckResult::Unknown:
                    STORM_LOG_DEBUG("Lower bound: Solver returned 'Unknown'");
                    return bound;
                default:
                    ++bound;
                    break;
            }
        }

        return bound;
    } else {
        // naive bound
        return 1;
    }
}

uint64_t FailureBoundFinder::getLeastFailureBound(storm::dft::storage::DFT<RationalFunction> const &dft, bool useSMT, uint_fast64_t timeout) {
    if (useSMT) {
        STORM_LOG_WARN("SMT encoding does not support rational functions");
    }
    return 1;
}

uint64_t FailureBoundFinder::getAlwaysFailedBound(storm::dft::storage::DFT<double> const &dft, bool useSMT, uint_fast64_t timeout) {
    STORM_LOG_TRACE("Compute bound for number of BE failures such that the DFT always fails");
    if (useSMT) {
        storm::dft::modelchecker::DFTASFChecker smtchecker(dft);
        smtchecker.toSolver();

        if (smtchecker.checkTleNeverFailed() == storm::solver::SmtSolver::CheckResult::Sat) {
            return dft.nrBasicElements() + 1;
        }
        uint64_t bound = dft.nrBasicElements();
        while (bound > 0) {
            smtchecker.setSolverTimeout(timeout * 1000);
            storm::solver::SmtSolver::CheckResult tmp_res = smtchecker.checkTleFailsWithEq(bound);
            smtchecker.unsetSolverTimeout();
            switch (tmp_res) {
                case storm::solver::SmtSolver::CheckResult::Sat:
                    if (!dft.getDependencies().empty()) {
                        return correctUpperBound(std::make_shared<storm::dft::modelchecker::DFTASFChecker>(smtchecker), bound, timeout);
                    } else {
                        return bound;
                    }
                case storm::solver::SmtSolver::CheckResult::Unknown:
                    STORM_LOG_DEBUG("Upper bound: Solver returned 'Unknown'");
                    return bound;
                default:
                    --bound;
                    break;
            }
        }
        return bound;
    } else {
        // naive bound
        return dft.nrBasicElements() + 1;
    }
}

uint64_t FailureBoundFinder::getAlwaysFailedBound(storm::dft::storage::DFT<RationalFunction> const &dft, bool useSMT, uint_fast64_t timeout) {
    if (useSMT) {
        STORM_LOG_WARN("SMT encoding does not support rational functions");
    }
    return dft.nrBasicElements() + 1;
}

class FailureBoundFinder;

}  // namespace utility
}  // namespace storm::dft
