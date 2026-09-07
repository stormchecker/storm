#pragma once

#include "storm/modelchecker/multiobjective/constraintbased/SparseCbQuery.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

/*
 * This class represents an achievability query for the constraint based approach (using SMT or LP solvers).
 */
template<class SparseModelType>
class SparseCbAchievabilityQuery : public SparseCbQuery<SparseModelType> {
   public:
    typedef typename SparseModelType::ValueType ValueType;

    SparseCbAchievabilityQuery(preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult);

    virtual ~SparseCbAchievabilityQuery() = default;

    /*
     * Invokes the computation and retrieves the result
     */
    virtual std::unique_ptr<CheckResult> check(Environment const& env) override;

   private:
    /*
     * Returns whether the given thresholds are achievable.
     */
    bool checkAchievability();

    /*
     * Adds variable y_i for each choice i and z_j for each possible bottom state j and asserts that for every solution of the
     * constraint system there is a scheduler under which
     * * if choice i yields reward for an expected value objective, then y_i is the expected number of times choice i is taken.
     * * z_j/(z_j + Sum_{i in Choices(j)} y_i) is a lower bound for the probability that starting from j no more reward is collected
     */
    void initializeConstraintSystem();

    // Adds the thresholds of the objective values
    void addObjectiveConstraints();

    // Returns the action based rewards for the given reward model name.
    std::vector<ValueType> getActionBasedExpectedRewards(std::string const& rewardModelName) const;

    std::vector<storm::expressions::Variable> expectedChoiceVariables;
    std::vector<storm::expressions::Variable> bottomStateVariables;

    std::unique_ptr<storm::solver::SmtSolver> solver;
};

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
