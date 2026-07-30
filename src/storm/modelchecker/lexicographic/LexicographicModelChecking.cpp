#include "storm/modelchecker/lexicographic/LexicographicModelChecking.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace lexicographic {

template<typename SparseModelType, typename ValueType>
helper::MDPSparseModelCheckingHelperReturnType<ValueType> check(Environment const&, SparseModelType const& model,
                                                                CheckTask<storm::logic::MultiObjectiveFormula, ValueType> const& checkTask,
                                                                CheckFormulaCallback const& formulaChecker) {
    storm::logic::MultiObjectiveFormula const& formula = checkTask.getFormula();
    auto const& subformulas = formula.getSubformulas();

    // Ensure that the query is supported
    STORM_LOG_THROW(model.getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::NotSupportedException,
                    "Lexicographic Model checking on model with multiple initial states is not supported.");
    STORM_LOG_THROW(formula.isLexicographic(), storm::exceptions::IllegalFunctionCallException,
                    "Invoked lexicographic model checking with a non-lexicographic formula " << formula << ".");
    STORM_LOG_THROW(std::all_of(subformulas.begin(), subformulas.end(),
                                [](auto const& f) {
                                    return f->isProbabilityOperatorFormula() && !f->asOperatorFormula().hasBound() &&
                                           f->asOperatorFormula().hasOptimalityType() && storm::solver::maximize(f->asOperatorFormula().getOptimalityType());
                                }),
                    storm::exceptions::NotSupportedException,
                    "Lexicographic model checking only supports Pmax=? [...]  subformulas. Got " << formula << " as input.");

    // Define the helper that contains all functions
    helper::lexicographic::LexicographicModelCheckerHelper<SparseModelType, ValueType, true> lMC =
        helper::lexicographic::LexicographicModelCheckerHelper<SparseModelType, ValueType, true>(formula, model.getTransitionMatrix());

    // get the product of (i) the product-automaton of all subformuale, and (ii) the model
    auto res = lMC.getCompleteProductModel(model, formulaChecker);

    std::shared_ptr<storm::transformer::DAProduct<SparseModelType>> completeProductModel = res.first;
    std::vector<uint64_t> accCond = res.second;

    // get the lexicogrpahic array for all MEC of the product-model
    std::pair<storm::storage::MaximalEndComponentDecomposition<ValueType>, std::vector<std::vector<bool>>> result =
        lMC.getLexArrays(completeProductModel, accCond);
    storm::storage::MaximalEndComponentDecomposition<ValueType> mecs = result.first;
    std::vector<std::vector<bool>> mecLexArrays = result.second;

    // solve the reachability query
    // That is: solve reachability for the lexicographic highest condition, restrict the model to optimal actions, repeat
    return lMC.lexReachability(mecs, mecLexArrays, completeProductModel, model);
}

template helper::MDPSparseModelCheckingHelperReturnType<double> check<storm::models::sparse::Mdp<double>, double>(
    Environment const& env, storm::models::sparse::Mdp<double> const& model, CheckTask<storm::logic::MultiObjectiveFormula, double> const& checkTask,
    CheckFormulaCallback const& formulaChecker);
template helper::MDPSparseModelCheckingHelperReturnType<storm::RationalNumber> check<storm::models::sparse::Mdp<storm::RationalNumber>, storm::RationalNumber>(
    Environment const& env, storm::models::sparse::Mdp<storm::RationalNumber> const& model,
    CheckTask<storm::logic::MultiObjectiveFormula, storm::RationalNumber> const& checkTask, CheckFormulaCallback const& formulaChecker);
}  // namespace lexicographic
}  // namespace modelchecker
}  // namespace storm
