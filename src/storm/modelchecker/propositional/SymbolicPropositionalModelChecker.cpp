#include "storm/modelchecker/propositional/SymbolicPropositionalModelChecker.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/symbolic/Ctmc.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/models/symbolic/Mdp.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/models/symbolic/StochasticTwoPlayerGame.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
template<typename ModelType>
SymbolicPropositionalModelChecker<ModelType>::SymbolicPropositionalModelChecker(ModelType const& model) : model(model) {
    // Intentionally left empty.
}

template<typename ModelType>
bool SymbolicPropositionalModelChecker<ModelType>::canHandle(CheckTask<storm::logic::Formula, ValueType> const& checkTask) const {
    storm::logic::Formula const& formula = checkTask.getFormula();
    return formula.isInFragment(storm::logic::propositional());
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicPropositionalModelChecker<ModelType>::checkBooleanLiteralFormula(
    Environment const& env, CheckTask<storm::logic::BooleanLiteralFormula, ValueType> const& checkTask) {
    storm::logic::BooleanLiteralFormula const& stateFormula = checkTask.getFormula();
    if (stateFormula.isTrueFormula()) {
        return std::unique_ptr<CheckResult>(new SymbolicQualitativeCheckResult<DdType>(model.getReachableStates(), model.getReachableStates()));
    } else {
        return std::unique_ptr<CheckResult>(new SymbolicQualitativeCheckResult<DdType>(model.getReachableStates(), model.getManager().getBddZero()));
    }
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicPropositionalModelChecker<ModelType>::checkAtomicLabelFormula(
    Environment const& env, CheckTask<storm::logic::AtomicLabelFormula, ValueType> const& checkTask) {
    storm::logic::AtomicLabelFormula const& stateFormula = checkTask.getFormula();
    STORM_LOG_THROW(model.hasLabel(stateFormula.getLabel()), storm::exceptions::InvalidPropertyException,
                    "The property refers to unknown label '" << stateFormula.getLabel() << "'.");
    return std::unique_ptr<CheckResult>(new SymbolicQualitativeCheckResult<DdType>(model.getReachableStates(), model.getStates(stateFormula.getLabel())));
}

template<typename ModelType>
std::unique_ptr<CheckResult> SymbolicPropositionalModelChecker<ModelType>::checkAtomicExpressionFormula(
    Environment const& env, CheckTask<storm::logic::AtomicExpressionFormula, ValueType> const& checkTask) {
    storm::logic::AtomicExpressionFormula const& stateFormula = checkTask.getFormula();
    return std::unique_ptr<CheckResult>(new SymbolicQualitativeCheckResult<DdType>(model.getReachableStates(), model.getStates(stateFormula.getExpression())));
}

template<typename ModelType>
ModelType const& SymbolicPropositionalModelChecker<ModelType>::getModel() const {
    return model;
}

// Explicitly instantiate the template class.
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Model<storm::dd::DdType::CUDD, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::CUDD, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::CUDD, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::CUDD, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::CUDD, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::CUDD, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, double>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, storm::RationalNumber>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Dtmc<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Ctmc<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::Mdp<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::MarkovAutomaton<storm::dd::DdType::Sylvan, storm::RationalFunction>>;
template class SymbolicPropositionalModelChecker<storm::models::symbolic::StochasticTwoPlayerGame<storm::dd::DdType::Sylvan, storm::RationalFunction>>;

}  // namespace modelchecker
}  // namespace storm
