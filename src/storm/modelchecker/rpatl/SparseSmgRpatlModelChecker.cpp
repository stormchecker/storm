#include "storm/modelchecker/rpatl/SparseSmgRpatlModelChecker.h"

#include <memory>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/helper/utility/SetInformationFromCheckTask.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/FilteredRewardModel.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
template<typename SparseSmgModelType>
SparseSmgRpatlModelChecker<SparseSmgModelType>::SparseSmgRpatlModelChecker(SparseSmgModelType const& model)
    : SparsePropositionalModelChecker<SparseSmgModelType>(model) {
    // Intentionally left empty.
}

template<typename SparseSmgModelType>
bool SparseSmgRpatlModelChecker<SparseSmgModelType>::canHandleStatic(CheckTask<storm::logic::Formula, ValueType> const& checkTask,
                                                                     bool* requiresSingleInitialState) {
    storm::logic::Formula const& formula = checkTask.getFormula();

    return formula.isInFragment(storm::logic::rpatl());
}

template<typename SparseSmgModelType>
bool SparseSmgRpatlModelChecker<SparseSmgModelType>::canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const {
    bool requiresSingleInitialState = false;
    if (canHandleStatic(checkTask, &requiresSingleInitialState)) {
        return !requiresSingleInitialState || this->getModel().getInitialStates().getNumberOfSetBits() == 1;
    } else {
        return false;
    }
}

template<typename SparseSmgModelType>
std::unique_ptr<CheckResult> SparseSmgRpatlModelChecker<SparseSmgModelType>::checkGameFormula(
    Environment const& env, CheckTask<storm::logic::GameFormula, ValueType> const& checkTask) {
    storm::logic::GameFormula const& gameFormula = checkTask.getFormula();
    storm::logic::Formula const& subFormula = gameFormula.getSubformula();
    if (subFormula.isOperatorFormula()) {
        return this->checkStateFormula(env, checkTask.substituteFormula(subFormula.asStateFormula()).setPlayerCoalition(gameFormula.getCoalition()));
    }
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Only game formulas with Operatorformulas as subformula are supported.");
}

template<typename SparseSmgModelType>
std::unique_ptr<CheckResult> SparseSmgRpatlModelChecker<SparseSmgModelType>::computeLongRunAverageProbabilities(
    Environment const&, CheckTask<storm::logic::StateFormula, ValueType> const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Not implemented.");
}

template<typename SparseSmgModelType>
std::unique_ptr<CheckResult> SparseSmgRpatlModelChecker<SparseSmgModelType>::computeLongRunAverageRewards(
    Environment const&, CheckTask<storm::logic::LongRunAverageRewardFormula, ValueType> const& checkTask) {
    auto rewardModel = storm::utility::createFilteredRewardModel(this->getModel(), checkTask);
    STORM_LOG_THROW(checkTask.isPlayerCoalitionSet(), storm::exceptions::InvalidPropertyException, "No player coalition was set.");
    auto coalitionStates = this->getModel().computeStatesOfCoalition(checkTask.getPlayerCoalition());
    std::cout << "Found " << coalitionStates.getNumberOfSetBits() << " states in coalition.\n";
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Not implemented.");
}

template class SparseSmgRpatlModelChecker<storm::models::sparse::Smg<double>>;
template class SparseSmgRpatlModelChecker<storm::models::sparse::Smg<storm::RationalNumber>>;
}  // namespace modelchecker
}  // namespace storm
