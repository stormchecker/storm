#include "storm/modelchecker/cvar/CvarModelChecking.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/modelchecker/cvar/CvarQueryInformation.h"
#include "storm/modelchecker/cvar/helper/SparseCvarComputationHelper.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {

template<typename SparseMdpModelType>
std::unique_ptr<CheckResult> performCvarModelChecking(
    Environment const& env, SparseMdpModelType const& model,
    CheckTask<storm::logic::CvarFormula, typename SparseMdpModelType::ValueType> const& checkTask,
    std::function<storm::storage::BitVector(storm::logic::Formula const&)> const& formulaChecker) {
    using ValueType = typename SparseMdpModelType::ValueType;

    STORM_LOG_THROW(checkTask.isOnlyInitialStatesRelevantSet(), storm::exceptions::InvalidOperationException,
                    "Computing CVaR is only supported for the initial states of a model.");
    STORM_LOG_THROW(model.getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::InvalidOperationException,
                    "CVaR is not supported on models with multiple initial states.");

    auto cvarQueryInformation = extractCvarQueryInformation(checkTask.getFormula());
    auto targetStates = formulaChecker(*cvarQueryInformation.targetFormula);

    SparseCvarComputationHelper<SparseMdpModelType> cvarHelper(model, cvarQueryInformation, targetStates);
    auto cvarResult = cvarHelper.computeCvar(env, checkTask.isProduceSchedulersSet());

    std::unique_ptr<CheckResult> result(new ExplicitQuantitativeCheckResult<ValueType>(*model.getInitialStates().begin(), std::move(cvarResult.value)));
    if (checkTask.isProduceSchedulersSet() && cvarResult.scheduler) {
        result->asExplicitQuantitativeCheckResult<ValueType>().setScheduler(std::move(cvarResult.scheduler));
    }
    return result;
}

template std::unique_ptr<CheckResult> performCvarModelChecking<storm::models::sparse::Mdp<double>>(
    Environment const& env, storm::models::sparse::Mdp<double> const& model, CheckTask<storm::logic::CvarFormula, double> const& checkTask,
    std::function<storm::storage::BitVector(storm::logic::Formula const&)> const& formulaChecker);

template std::unique_ptr<CheckResult> performCvarModelChecking<storm::models::sparse::Mdp<storm::RationalNumber>>(
    Environment const& env, storm::models::sparse::Mdp<storm::RationalNumber> const& model,
    CheckTask<storm::logic::CvarFormula, storm::RationalNumber> const& checkTask,
    std::function<storm::storage::BitVector(storm::logic::Formula const&)> const& formulaChecker);

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
