#include "storm-gamebased-ar/modelchecker/abstraction/BisimulationAbstractionRefinementModelChecker.h"

#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/models/symbolic/StochasticTwoPlayerGame.h"

#include "storm-gamebased-ar/abstraction/SymbolicStateSet.h"

#include "storm/storage/dd/bisimulation/BisimulationDecomposition.h"

#include "storm/modelchecker/propositional/SymbolicPropositionalModelChecker.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/macros.h"

namespace storm::gbar {
namespace modelchecker {

template<typename ModelType>
const std::string BisimulationAbstractionRefinementModelChecker<ModelType>::name = "bisimulation-based astraction refinement";

template<typename ModelType>
BisimulationAbstractionRefinementModelChecker<ModelType>::BisimulationAbstractionRefinementModelChecker(ModelType const& model) : model(model) {
    // Intentionally left empty.
}

template<typename ModelType>
BisimulationAbstractionRefinementModelChecker<ModelType>::~BisimulationAbstractionRefinementModelChecker() {
    // Intentionally left empty.
}

template<typename ModelType>
bool BisimulationAbstractionRefinementModelChecker<ModelType>::supportsReachabilityRewards() const {
    return true;
}

template<typename ModelType>
std::string const& BisimulationAbstractionRefinementModelChecker<ModelType>::getName() const {
    return name;
}

template<typename ModelType>
void BisimulationAbstractionRefinementModelChecker<ModelType>::initializeAbstractionRefinement() {
    // Create the appropriate preservation information.
    auto const& checkTask = this->getCheckTask();
    storm::dd::bisimulation::PreservationInformation<DdType, ValueType> preservationInformation(model, {checkTask.getFormula().asSharedPointer()});
    if (checkTask.getFormula().isEventuallyFormula() && checkTask.getFormula().asEventuallyFormula().getContext() == storm::logic::FormulaContext::Reward) {
        if (!checkTask.isRewardModelSet() || model.hasUniqueRewardModel()) {
            preservationInformation.addRewardModel(model.getUniqueRewardModelName());
        } else if (checkTask.isRewardModelSet()) {
            preservationInformation.addRewardModel(checkTask.getRewardModel());
        }
    }

    // Create the bisimulation object.
    this->bisimulation = std::make_unique<storm::dd::BisimulationDecomposition<DdType, ValueType>>(this->model, storm::storage::BisimulationType::Strong,
                                                                                                   preservationInformation);
}

template<typename ModelType>
std::shared_ptr<storm::models::Model<typename BisimulationAbstractionRefinementModelChecker<ModelType>::ValueType>>
BisimulationAbstractionRefinementModelChecker<ModelType>::getAbstractModel() {
    lastAbstractModel = this->bisimulation->getQuotient(storm::dd::bisimulation::QuotientFormat::Dd);
    return lastAbstractModel;
}

template<typename ModelType>
std::pair<std::unique_ptr<storm::gbar::abstraction::StateSet>, std::unique_ptr<storm::gbar::abstraction::StateSet>>
BisimulationAbstractionRefinementModelChecker<ModelType>::getConstraintAndTargetStates(storm::models::Model<ValueType> const& abstractModel) {
    STORM_LOG_ASSERT(lastAbstractModel, "Expected abstract model.");
    std::pair<storm::dd::Bdd<DdType>, storm::dd::Bdd<DdType>> ddResult;
    if (lastAbstractModel->isOfType(storm::models::ModelType::Dtmc)) {
        ddResult = this->getConstraintAndTargetStates(*lastAbstractModel->template as<storm::models::symbolic::Dtmc<DdType, ValueType>>());
    } else if (lastAbstractModel->isOfType(storm::models::ModelType::Mdp)) {
        ddResult = this->getConstraintAndTargetStates(*lastAbstractModel->template as<storm::models::symbolic::Mdp<DdType, ValueType>>());
    } else {
        ddResult = this->getConstraintAndTargetStates(*lastAbstractModel->template as<storm::models::symbolic::StochasticTwoPlayerGame<DdType, ValueType>>());
    }

    std::pair<std::unique_ptr<storm::gbar::abstraction::StateSet>, std::unique_ptr<storm::gbar::abstraction::StateSet>> result;
    result.first = std::make_unique<storm::gbar::abstraction::SymbolicStateSet<DdType>>(ddResult.first);
    result.second = std::make_unique<storm::gbar::abstraction::SymbolicStateSet<DdType>>(ddResult.second);
    return result;
}

template<typename ModelType>
template<typename QuotientModelType>
std::pair<storm::dd::Bdd<BisimulationAbstractionRefinementModelChecker<ModelType>::DdType>,
          storm::dd::Bdd<BisimulationAbstractionRefinementModelChecker<ModelType>::DdType>>
BisimulationAbstractionRefinementModelChecker<ModelType>::getConstraintAndTargetStates(QuotientModelType const& quotient) {
    std::pair<storm::dd::Bdd<DdType>, storm::dd::Bdd<DdType>> result;

    auto const& checkTask = this->getCheckTask();

    storm::modelchecker::SymbolicPropositionalModelChecker<QuotientModelType> checker(quotient);
    if (checkTask.getFormula().isUntilFormula()) {
        std::unique_ptr<storm::modelchecker::CheckResult> subresult = checker.check(checkTask.getFormula().asUntilFormula().getLeftSubformula());
        result.first = subresult->asSymbolicQualitativeCheckResult<DdType>().getTruthValuesVector();
        subresult = checker.check(checkTask.getFormula().asUntilFormula().getRightSubformula());
        result.second = subresult->asSymbolicQualitativeCheckResult<DdType>().getTruthValuesVector();
    } else if (checkTask.getFormula().isEventuallyFormula()) {
        storm::logic::EventuallyFormula const& eventuallyFormula = checkTask.getFormula().asEventuallyFormula();
        result.first = quotient.getReachableStates();
        std::unique_ptr<storm::modelchecker::CheckResult> subresult = checker.check(eventuallyFormula.getSubformula());
        result.second = subresult->asSymbolicQualitativeCheckResult<DdType>().getTruthValuesVector();
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "The given formula is not supported by this model checker.");
    }

    return result;
}

template<typename ModelType>
uint64_t BisimulationAbstractionRefinementModelChecker<ModelType>::getAbstractionPlayer() const {
    // Usually, the abstraction player is the first player. However, when we have arrived at the actual bisimulation
    // quotient, the abstraction player vanishes.
    return model.getType() == lastAbstractModel->getType() ? 0 : 1;
}

template<typename ModelType>
bool BisimulationAbstractionRefinementModelChecker<ModelType>::requiresSchedulerSynthesis() const {
    return false;
}

template<typename ModelType>
void BisimulationAbstractionRefinementModelChecker<ModelType>::refineAbstractModel() {
    STORM_LOG_ASSERT(bisimulation, "Bisimulation object required.");
    this->bisimulation->compute(10);
}

template class BisimulationAbstractionRefinementModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::CUDD, double>>;
template class BisimulationAbstractionRefinementModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::CUDD, double>>;
template class BisimulationAbstractionRefinementModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, double>>;
template class BisimulationAbstractionRefinementModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, double>>;

}  // namespace modelchecker
}  // namespace storm::gbar
