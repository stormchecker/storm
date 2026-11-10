#pragma once

#include <memory>

#include "storm/expressions/ExpressionManager.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectivePreprocessorResult.h"
#include "storm/modelchecker/results/CheckResult.h"

namespace storm {

class Environment;

namespace modelchecker {
namespace multiobjective {

/*
 * This class represents a multi-objective  query for the constraint based approach (using SMT or LP solvers).
 */
template<class SparseModelType>
class SparseCbQuery {
   public:
    virtual ~SparseCbQuery() = default;

    /*
     * Invokes the computation and retrieves the result
     */
    virtual std::unique_ptr<CheckResult> check(Environment const& env) = 0;

   protected:
    SparseCbQuery(preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType> const& preprocessorResult);

    SparseModelType const& originalModel;
    storm::logic::MultiObjectiveFormula const& originalFormula;
    std::vector<Objective<typename SparseModelType::ValueType>> objectives;
    std::shared_ptr<SparseModelType> preprocessedModel;
    storm::storage::BitVector reward0EStates;
    std::shared_ptr<storm::expressions::ExpressionManager> expressionManager;
};

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
