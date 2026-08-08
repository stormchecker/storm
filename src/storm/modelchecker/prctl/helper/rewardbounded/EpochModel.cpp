#include "storm/modelchecker/prctl/helper/rewardbounded/EpochModel.h"
#include "storm/modelchecker/prctl/helper/rewardbounded/MultiDimensionalRewardUnfolding.h"

#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/utility/graph.h"

#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace rewardbounded {

template<typename ValueType>
std::vector<ValueType> analyzeTrivialDtmcEpochModel(EpochModel<ValueType, true> &epochModel) {
    std::vector<ValueType> epochResult;
    epochResult.reserve(epochModel.epochInStates.getNumberOfSetBits());
    auto stepSolutionIt = epochModel.stepSolutions.begin();
    auto stepChoiceIt = epochModel.stepChoices.begin();
    for (auto state : epochModel.epochInStates) {
        while (*stepChoiceIt < state) {
            ++stepChoiceIt;
            ++stepSolutionIt;
        }
        if (epochModel.objectiveRewardFilter.front().get(state)) {
            if (*stepChoiceIt == state) {
                epochResult.push_back(epochModel.objectiveRewards.front()[state] + *stepSolutionIt);
            } else {
                epochResult.push_back(epochModel.objectiveRewards.front()[state]);
            }
        } else {
            if (*stepChoiceIt == state) {
                epochResult.push_back(*stepSolutionIt);
            } else {
                epochResult.push_back(storm::utility::zero<ValueType>());
            }
        }
    }
    return epochResult;
}

template<typename ValueType>
std::vector<ValueType> analyzeNonTrivialDtmcEpochModel(Environment const &env, EpochModel<ValueType, true> &epochModel, std::vector<ValueType> &x,
                                                       std::vector<ValueType> &b, std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>> &linEqSolver,
                                                       boost::optional<ValueType> const &lowerBound, boost::optional<ValueType> const &upperBound) {
    // Update some data for the case that the Matrix has changed
    if (epochModel.epochMatrixChanged) {
        x.assign(epochModel.epochMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
        storm::solver::GeneralLinearEquationSolverFactory<ValueType> linearEquationSolverFactory;
        // We only check for acyclic models if the equation problem has the fixedPointSystem format.
        // We could also do this for other formats, however, this requires either matrix conversions or a different 'hasCycle' implementation.
        // Also, we would have to match the equationProblemFormat of the acyclic solver.
        bool epochMatrixAcyclic = epochModel.equationSolverProblemFormat.get() == storm::solver::LinearEquationSolverProblemFormat::FixedPointSystem &&
                                  !storm::utility::graph::hasCycle(epochModel.epochMatrix);
        Environment acyclicEnv;
        if (epochMatrixAcyclic) {
            acyclicEnv = env;
            acyclicEnv.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Acyclic);
            linEqSolver = linearEquationSolverFactory.create(acyclicEnv, epochModel.epochMatrix);
        } else {
            linEqSolver = linearEquationSolverFactory.create(env, epochModel.epochMatrix);
        }
        linEqSolver->setCachingEnabled(true);
        auto req = linEqSolver->getRequirements(epochMatrixAcyclic ? acyclicEnv : env);
        if (lowerBound) {
            linEqSolver->setLowerBound(lowerBound.get());
            req.clearLowerBounds();
        }
        if (upperBound) {
            linEqSolver->setUpperBound(upperBound.get());
            req.clearUpperBounds();
        }
        if (epochMatrixAcyclic) {
            req.clearAcyclic();
        }
        STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                        "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
        STORM_LOG_THROW(linEqSolver->getEquationProblemFormat(epochMatrixAcyclic ? acyclicEnv : env) == epochModel.equationSolverProblemFormat.get(),
                        storm::exceptions::UnexpectedException,
                        "The constructed solver uses a different equation problem format then the one that has been specified initially.");
    }

    // Prepare the right hand side of the equation system
    b.assign(epochModel.epochMatrix.getRowCount(), storm::utility::zero<ValueType>());
    std::vector<ValueType> const &objectiveValues = epochModel.objectiveRewards.front();
    for (auto choice : epochModel.objectiveRewardFilter.front()) {
        b[choice] = objectiveValues[choice];
    }
    auto stepSolutionIt = epochModel.stepSolutions.begin();
    for (auto choice : epochModel.stepChoices) {
        b[choice] += *stepSolutionIt;
        ++stepSolutionIt;
    }
    STORM_LOG_ASSERT(stepSolutionIt == epochModel.stepSolutions.end(), "Step solution iterator not at end.");

    // Solve the minMax equation system
    linEqSolver->solveEquations(env, x, b);

    return storm::utility::vector::filterVector(x, epochModel.epochInStates);
}

template<typename ValueType>
std::vector<ValueType> analyzeTrivialMdpEpochModel(OptimizationDirection dir, EpochModel<ValueType, true> &epochModel) {
    // Assert that the epoch model is indeed trivial
    STORM_LOG_ASSERT(epochModel.epochMatrix.getEntryCount() == 0, "Epoch matrix should be empty for trivial MDP.");

    std::vector<ValueType> epochResult;
    epochResult.reserve(epochModel.epochInStates.getNumberOfSetBits());

    auto stepSolutionIt = epochModel.stepSolutions.begin();
    auto stepChoiceIt = epochModel.stepChoices.begin();
    for (auto state : epochModel.epochInStates) {
        // Obtain the best choice for this state
        ValueType bestValue;
        uint64_t lastChoice = epochModel.epochMatrix.getRowGroupIndices()[state + 1];
        bool isFirstChoice = true;
        for (uint64_t choice = epochModel.epochMatrix.getRowGroupIndices()[state]; choice < lastChoice; ++choice) {
            while (*stepChoiceIt < choice) {
                ++stepChoiceIt;
                ++stepSolutionIt;
            }

            ValueType choiceValue = storm::utility::zero<ValueType>();
            if (epochModel.objectiveRewardFilter.front().get(choice)) {
                choiceValue += epochModel.objectiveRewards.front()[choice];
            }
            if (*stepChoiceIt == choice) {
                choiceValue += *stepSolutionIt;
            }

            if (isFirstChoice) {
                bestValue = std::move(choiceValue);
                isFirstChoice = false;
            } else {
                if (storm::solver::minimize(dir)) {
                    if (choiceValue < bestValue) {
                        bestValue = std::move(choiceValue);
                    }
                } else {
                    if (choiceValue > bestValue) {
                        bestValue = std::move(choiceValue);
                    }
                }
            }
        }
        // Insert the solution w.r.t. this choice
        epochResult.push_back(std::move(bestValue));
    }
    return epochResult;
}

template<typename ValueType>
std::vector<ValueType> analyzeNonTrivialMdpEpochModel(Environment const &env, OptimizationDirection dir, EpochModel<ValueType, true> &epochModel,
                                                      std::vector<ValueType> &x, std::vector<ValueType> &b,
                                                      std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>> &minMaxSolver,
                                                      boost::optional<ValueType> const &lowerBound, boost::optional<ValueType> const &upperBound) {
    // Update some data for the case that the Matrix has changed
    if (epochModel.epochMatrixChanged) {
        x.assign(epochModel.epochMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
        storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType> minMaxLinearEquationSolverFactory;
        bool epochMatrixAcyclic = !storm::utility::graph::hasCycle(epochModel.epochMatrix);
        Environment acyclicEnv;
        if (epochMatrixAcyclic) {
            acyclicEnv = env;
            acyclicEnv.solver().minMax().setMethod(storm::solver::MinMaxMethod::Acyclic);
            minMaxSolver = minMaxLinearEquationSolverFactory.create(acyclicEnv, epochModel.epochMatrix);
        } else {
            minMaxSolver = minMaxLinearEquationSolverFactory.create(env, epochModel.epochMatrix);
        }
        minMaxSolver->setHasUniqueSolution();
        minMaxSolver->setHasNoEndComponents();
        minMaxSolver->setOptimizationDirection(dir);
        minMaxSolver->setCachingEnabled(true);
        minMaxSolver->setTrackScheduler(!epochMatrixAcyclic);  // only track the scheduler if there are cycles
        auto req = minMaxSolver->getRequirements(epochMatrixAcyclic ? acyclicEnv : env, dir, false);
        if (lowerBound) {
            minMaxSolver->setLowerBound(lowerBound.get());
            req.clearLowerBounds();
        }
        if (upperBound) {
            minMaxSolver->setUpperBound(upperBound.get());
            req.clearUpperBounds();
        }
        if (epochMatrixAcyclic) {
            req.clearAcyclic();
        }
        STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                        "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
        minMaxSolver->setRequirementsChecked();
    } else {
        if (minMaxSolver && minMaxSolver->isTrackSchedulerSet()) {
            auto choicesTmp = minMaxSolver->getSchedulerChoices();
            minMaxSolver->setInitialScheduler(std::move(choicesTmp));
        }
    }

    // Prepare the right hand side of the equation system
    b.assign(epochModel.epochMatrix.getRowCount(), storm::utility::zero<ValueType>());
    std::vector<ValueType> const &objectiveValues = epochModel.objectiveRewards.front();
    for (auto choice : epochModel.objectiveRewardFilter.front()) {
        b[choice] = objectiveValues[choice];
    }
    auto stepSolutionIt = epochModel.stepSolutions.begin();
    for (auto choice : epochModel.stepChoices) {
        b[choice] += *stepSolutionIt;
        ++stepSolutionIt;
    }
    STORM_LOG_ASSERT(stepSolutionIt == epochModel.stepSolutions.end(), "Step solution iterator not at end.");
    minMaxSolver->solveEquations(env, x, b);

    return storm::utility::vector::filterVector(x, epochModel.epochInStates);
}

template<>
std::vector<double> EpochModel<double, true>::analyzeSingleObjective(const storm::Environment &env, std::vector<double> &x, std::vector<double> &b,
                                                                     std::unique_ptr<storm::solver::LinearEquationSolver<double>> &linEqSolver,
                                                                     const boost::optional<double> &lowerBound, const boost::optional<double> &upperBound) {
    STORM_LOG_ASSERT(epochMatrix.hasTrivialRowGrouping(), "This operation is only allowed if no nondeterminism is present.");
    STORM_LOG_ASSERT(equationSolverProblemFormat.is_initialized(), "Unknown equation problem format.");
    // If the epoch matrix is empty we do not need to solve a linear equation system
    bool convertToEquationSystem = (equationSolverProblemFormat == storm::solver::LinearEquationSolverProblemFormat::EquationSystem);
    if ((convertToEquationSystem && epochMatrix.isIdentityMatrix()) || (!convertToEquationSystem && epochMatrix.getEntryCount() == 0)) {
        return analyzeTrivialDtmcEpochModel<double>(*this);
    } else {
        return analyzeNonTrivialDtmcEpochModel<double>(env, *this, x, b, linEqSolver, lowerBound, upperBound);
    }
}

template<>
std::vector<double> EpochModel<double, true>::analyzeSingleObjective(const storm::Environment &env, storm::OptimizationDirection dir, std::vector<double> &x,
                                                                     std::vector<double> &b,
                                                                     std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<double>> &minMaxSolver,
                                                                     const boost::optional<double> &lowerBound, const boost::optional<double> &upperBound) {
    // If the epoch matrix is empty we do not need to solve a linear equation system
    if (epochMatrix.getEntryCount() == 0) {
        return analyzeTrivialMdpEpochModel<double>(dir, *this);
    } else {
        return analyzeNonTrivialMdpEpochModel<double>(env, dir, *this, x, b, minMaxSolver, lowerBound, upperBound);
    }
}

template<>
std::vector<storm::RationalNumber> EpochModel<storm::RationalNumber, true>::analyzeSingleObjective(
    const storm::Environment &env, std::vector<storm::RationalNumber> &x, std::vector<storm::RationalNumber> &b,
    std::unique_ptr<storm::solver::LinearEquationSolver<storm::RationalNumber>> &linEqSolver, const boost::optional<storm::RationalNumber> &lowerBound,
    const boost::optional<storm::RationalNumber> &upperBound) {
    STORM_LOG_ASSERT(epochMatrix.hasTrivialRowGrouping(), "This operation is only allowed if no nondeterminism is present.");
    STORM_LOG_ASSERT(equationSolverProblemFormat.is_initialized(), "Unknown equation problem format.");
    // If the epoch matrix is empty we do not need to solve a linear equation system
    bool convertToEquationSystem = (equationSolverProblemFormat == storm::solver::LinearEquationSolverProblemFormat::EquationSystem);
    if ((convertToEquationSystem && epochMatrix.isIdentityMatrix()) || (!convertToEquationSystem && epochMatrix.getEntryCount() == 0)) {
        return analyzeTrivialDtmcEpochModel<storm::RationalNumber>(*this);
    } else {
        return analyzeNonTrivialDtmcEpochModel<storm::RationalNumber>(env, *this, x, b, linEqSolver, lowerBound, upperBound);
    }
}

template<>
std::vector<storm::RationalNumber> EpochModel<storm::RationalNumber, true>::analyzeSingleObjective(
    const storm::Environment &env, storm::OptimizationDirection dir, std::vector<storm::RationalNumber> &x, std::vector<storm::RationalNumber> &b,
    std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<storm::RationalNumber>> &minMaxSolver, const boost::optional<storm::RationalNumber> &lowerBound,
    const boost::optional<storm::RationalNumber> &upperBound) {
    // If the epoch matrix is empty we do not need to solve a linear equation system
    if (epochMatrix.getEntryCount() == 0) {
        return analyzeTrivialMdpEpochModel<storm::RationalNumber>(dir, *this);
    } else {
        return analyzeNonTrivialMdpEpochModel<storm::RationalNumber>(env, dir, *this, x, b, minMaxSolver, lowerBound, upperBound);
    }
}

template struct EpochModel<double, true>;
template struct EpochModel<double, false>;
template struct EpochModel<storm::RationalNumber, true>;
template struct EpochModel<storm::RationalNumber, false>;
}  // namespace rewardbounded
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
