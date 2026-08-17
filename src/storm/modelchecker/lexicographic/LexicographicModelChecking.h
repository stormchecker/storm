#pragma once

#include "storm/environment/Environment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/lexicographic/LexicographicModelCheckerHelper.h"

namespace storm {

class Environment;

namespace modelchecker {
namespace lexicographic {
typedef std::function<storm::storage::BitVector(storm::logic::Formula const&)> CheckFormulaCallback;

/**
 * check a lexicographic LTL-formula
 */
template<typename SparseModelType, typename ValueType>
helper::MDPSparseModelCheckingHelperReturnType<ValueType> check(Environment const& env, SparseModelType const& model,
                                                                CheckTask<storm::logic::MultiObjectiveFormula, ValueType> const& checkTask,
                                                                CheckFormulaCallback const& formulaChecker);

}  // namespace lexicographic
}  // namespace modelchecker
}  // namespace storm