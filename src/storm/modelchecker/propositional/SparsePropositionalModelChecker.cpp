#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/models/sparse/Smg.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
template<typename SparseModelType>
SparsePropositionalModelChecker<SparseModelType>::SparsePropositionalModelChecker(SparseModelType const& model) : model(model) {
    // Intentionally left empty.
}

template<typename SparseModelType>
bool SparsePropositionalModelChecker<SparseModelType>::canHandle(CheckTask<storm::logic::Formula, SolutionType> const& checkTask) const {
    storm::logic::Formula const& formula = checkTask.getFormula();
    return formula.isInFragment(storm::logic::propositional());
}

template<typename SparseModelType>
std::unique_ptr<CheckResult> SparsePropositionalModelChecker<SparseModelType>::checkBooleanLiteralFormula(
    Environment const& env, CheckTask<storm::logic::BooleanLiteralFormula, SolutionType> const& checkTask) {
    storm::logic::BooleanLiteralFormula const& stateFormula = checkTask.getFormula();
    if (stateFormula.isTrueFormula()) {
        return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<SolutionType>(storm::storage::BitVector(model.getNumberOfStates(), true)));
    } else {
        return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<SolutionType>(storm::storage::BitVector(model.getNumberOfStates())));
    }
}

template<typename SparseModelType>
std::unique_ptr<CheckResult> SparsePropositionalModelChecker<SparseModelType>::checkAtomicLabelFormula(
    Environment const& env, CheckTask<storm::logic::AtomicLabelFormula, SolutionType> const& checkTask) {
    storm::logic::AtomicLabelFormula const& stateFormula = checkTask.getFormula();
    STORM_LOG_THROW(model.hasLabel(stateFormula.getLabel()), storm::exceptions::InvalidPropertyException,
                    "The property refers to unknown label '" << stateFormula.getLabel() << "'.");
    return std::unique_ptr<CheckResult>(new ExplicitQualitativeCheckResult<SolutionType>(model.getStates(stateFormula.getLabel())));
}

template<typename SparseModelType>
SparseModelType const& SparsePropositionalModelChecker<SparseModelType>::getModel() const {
    return model;
}

// Explicitly instantiate the template class.
template class SparsePropositionalModelChecker<storm::models::sparse::Model<double>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Dtmc<double>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Ctmc<double>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Mdp<double>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Pomdp<double>>;
template class SparsePropositionalModelChecker<storm::models::sparse::MarkovAutomaton<double>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Smg<double>>;

template class SparsePropositionalModelChecker<storm::models::sparse::Mdp<double, storm::models::sparse::StandardRewardModel<storm::Interval>>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Smg<double, storm::models::sparse::StandardRewardModel<storm::Interval>>>;

template class SparsePropositionalModelChecker<
    storm::models::sparse::Mdp<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>>;
template class SparsePropositionalModelChecker<
    storm::models::sparse::Smg<storm::RationalNumber, storm::models::sparse::StandardRewardModel<storm::RationalInterval>>>;

template class SparsePropositionalModelChecker<storm::models::sparse::Model<storm::RationalNumber>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Dtmc<storm::RationalNumber>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Ctmc<storm::RationalNumber>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Pomdp<storm::RationalNumber>>;
template class SparsePropositionalModelChecker<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Smg<storm::RationalNumber>>;

template class SparsePropositionalModelChecker<storm::models::sparse::Model<storm::RationalFunction>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Ctmc<storm::RationalFunction>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Mdp<storm::RationalFunction>>;
template class SparsePropositionalModelChecker<storm::models::sparse::MarkovAutomaton<storm::RationalFunction>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Smg<storm::RationalFunction>>;

template class SparsePropositionalModelChecker<storm::models::sparse::Mdp<storm::Interval>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Dtmc<storm::Interval>>;

template class SparsePropositionalModelChecker<storm::models::sparse::Mdp<storm::RationalInterval>>;
template class SparsePropositionalModelChecker<storm::models::sparse::Dtmc<storm::RationalInterval>>;
}  // namespace modelchecker
}  // namespace storm
