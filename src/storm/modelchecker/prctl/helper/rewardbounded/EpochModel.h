#pragma once

#include <vector>

#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/LinearEquationSolverProblemFormat.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"

namespace storm {
class Environment;

namespace modelchecker {
namespace helper {
namespace rewardbounded {
template<typename ValueType, bool SingleObjectiveMode>
struct EpochModel {
    typedef typename std::conditional<SingleObjectiveMode, ValueType, std::vector<ValueType>>::type SolutionType;

    bool epochMatrixChanged;
    storm::storage::SparseMatrix<ValueType> epochMatrix;
    storm::storage::BitVector stepChoices;
    std::vector<SolutionType> stepSolutions;
    std::vector<std::vector<ValueType>> objectiveRewards;
    std::vector<storm::storage::BitVector> objectiveRewardFilter;
    storm::storage::BitVector epochInStates;
    /// In case of DTMCs we have different options for the equation problem format the epoch model will have.
    boost::optional<storm::solver::LinearEquationSolverProblemFormat> equationSolverProblemFormat;

    /*!
     * Analyzes the epoch model, i.e., solves the represented equation system. This method assumes a nondeterministic model.
     */
    std::vector<ValueType> analyzeSingleObjective(Environment const& env, OptimizationDirection dir, std::vector<ValueType>& x, std::vector<ValueType>& b,
                                                  std::unique_ptr<storm::solver::MinMaxLinearEquationSolver<ValueType>>& minMaxSolver,
                                                  boost::optional<ValueType> const& lowerBound, boost::optional<ValueType> const& upperBound);

    /*!
     * Analyzes the epoch model, i.e., solves the represented equation system. This method assumes a deterministic model.
     */
    std::vector<ValueType> analyzeSingleObjective(Environment const& env, std::vector<ValueType>& x, std::vector<ValueType>& b,
                                                  std::unique_ptr<storm::solver::LinearEquationSolver<ValueType>>& linEqSolver,
                                                  boost::optional<ValueType> const& lowerBound, boost::optional<ValueType> const& upperBound);
};

}  // namespace rewardbounded
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm