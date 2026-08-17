#include "MemoryIncorporation.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/Formulas.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/memorystructure/MemoryStructureBuilder.h"
#include "storm/storage/memorystructure/NondeterministicMemoryStructureBuilder.h"
#include "storm/storage/memorystructure/SparseModelMemoryProduct.h"
#include "storm/storage/memorystructure/SparseModelMemoryProductReverseData.h"
#include "storm/storage/memorystructure/SparseModelNondeterministicMemoryProduct.h"
#include "storm/utility/macros.h"

namespace storm {
namespace transformer {

template<class SparseModelType>
storm::storage::MemoryStructure getGoalMemory(SparseModelType const& model, storm::logic::Formula const& propositionalGoalStateFormula) {
    STORM_LOG_THROW(propositionalGoalStateFormula.isInFragment(storm::logic::propositional()), storm::exceptions::NotSupportedException,
                    "The  subformula " << propositionalGoalStateFormula << " should be propositional.");

    storm::modelchecker::SparsePropositionalModelChecker<SparseModelType> mc(model);
    storm::storage::BitVector goalStates =
        mc.check(propositionalGoalStateFormula)->template asExplicitQualitativeCheckResult<typename SparseModelType::ValueType>().getTruthValuesVector();

    // Check if the formula is already satisfied for all initial states. In such a case the trivial memory structure suffices.
    if (model.getInitialStates().isSubsetOf(goalStates)) {
        STORM_LOG_INFO("One objective is already satisfied for all initial states.");
        return storm::storage::MemoryStructureBuilder<typename SparseModelType::ValueType,
                                                      typename SparseModelType::RewardModelType>::buildTrivialMemoryStructure(model);
    }

    // Create a memory structure that stores whether a goal state has already been reached
    storm::storage::MemoryStructureBuilder<typename SparseModelType::ValueType, typename SparseModelType::RewardModelType> builder(2, model);
    builder.setTransition(0, 0, ~goalStates);
    builder.setTransition(0, 1, goalStates);
    builder.setTransition(1, 1, storm::storage::BitVector(model.getNumberOfStates(), true));
    for (auto initState : model.getInitialStates()) {
        builder.setInitialMemoryState(initState, goalStates.get(initState) ? 1 : 0);
    }
    return builder.build();
}

template<class SparseModelType>
storm::storage::MemoryStructure getUntilFormulaMemory(SparseModelType const& model, storm::logic::Formula const& leftSubFormula,
                                                      storm::logic::Formula const& rightSubFormula) {
    auto notLeftOrRight = std::make_shared<storm::logic::BinaryBooleanStateFormula>(
        storm::logic::BinaryBooleanStateFormula::OperatorType::Or,
        std::make_shared<storm::logic::UnaryBooleanStateFormula>(storm::logic::UnaryBooleanStateFormula::OperatorType::Not, leftSubFormula.asSharedPointer()),
        rightSubFormula.asSharedPointer());
    return getGoalMemory<SparseModelType>(model, *notLeftOrRight);
}

template<class SparseModelType>
storm::storage::MemoryStructure incorporateGoalMemoryHelper(SparseModelType const& model,
                                                            std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
    storm::storage::MemoryStructure memory =
        storm::storage::MemoryStructureBuilder<typename SparseModelType::ValueType, typename SparseModelType::RewardModelType>::buildTrivialMemoryStructure(
            model);

    for (auto const& subFormula : formulas) {
        STORM_LOG_THROW(subFormula->isOperatorFormula(), storm::exceptions::NotSupportedException, "The given Formula " << *subFormula << " is not supported.");
        auto const& subsubFormula = subFormula->asOperatorFormula().getSubformula();
        if (subsubFormula.isEventuallyFormula()) {
            memory = memory.product(getGoalMemory(model, subsubFormula.asEventuallyFormula().getSubformula()));
        } else if (subsubFormula.isUntilFormula()) {
            memory = memory.product(
                getUntilFormulaMemory(model, subsubFormula.asUntilFormula().getLeftSubformula(), subsubFormula.asUntilFormula().getRightSubformula()));
        } else if (subsubFormula.isBoundedUntilFormula()) {
            // For bounded formulas it is only reasonable to add the goal memory if it considers a single upper step/time bound.
            auto const& buf = subsubFormula.asBoundedUntilFormula();
            if (!buf.isMultiDimensional() && !buf.getTimeBoundReference().isRewardBound() &&
                (!buf.hasLowerBound() || (!buf.isLowerBoundStrict() && storm::utility::isZero(buf.template getLowerBound<storm::RationalNumber>())))) {
                memory = memory.product(getUntilFormulaMemory(model, buf.getLeftSubformula(), buf.getRightSubformula()));
            }
        } else if (subsubFormula.isGloballyFormula()) {
            auto notPhi = std::make_shared<storm::logic::UnaryBooleanStateFormula>(storm::logic::UnaryBooleanStateFormula::OperatorType::Not,
                                                                                   subsubFormula.asGloballyFormula().getSubformula().asSharedPointer());
            memory = memory.product(getGoalMemory(model, *notPhi));
        } else {
            STORM_LOG_THROW(subFormula->isLongRunAverageOperatorFormula() || subsubFormula.isTotalRewardFormula() ||
                                subsubFormula.isCumulativeRewardFormula() || subsubFormula.isLongRunAverageRewardFormula(),
                            storm::exceptions::NotSupportedException, "The given Formula " << subsubFormula << " is not supported.");
        }
    }

    return memory;
}

template<class SparseModelType>
std::shared_ptr<SparseModelType> MemoryIncorporation<SparseModelType>::incorporateGoalMemory(
    SparseModelType const& model, std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
    auto memory = incorporateGoalMemoryHelper(model, formulas);
    auto product = memory.product(model);
    return product.build()->template as<SparseModelType>();
}

template<class SparseModelType>
std::pair<std::shared_ptr<SparseModelType>, storm::storage::SparseModelMemoryProductReverseData>
MemoryIncorporation<SparseModelType>::incorporateGoalMemoryWithReverseData(SparseModelType const& model,
                                                                           std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) {
    auto memory = incorporateGoalMemoryHelper(model, formulas);
    auto product = memory.product(model);
    auto result = product.build();
    return std::make_pair(result->template as<SparseModelType>(), product.computeReverseData());
}

template<class SparseModelType>
std::shared_ptr<SparseModelType> MemoryIncorporation<SparseModelType>::incorporateFullMemory(SparseModelType const& model, uint64_t memoryStates) {
    auto memory = storm::storage::NondeterministicMemoryStructureBuilder().build(storm::storage::NondeterministicMemoryStructurePattern::Full, memoryStates);
    return storm::storage::SparseModelNondeterministicMemoryProduct<SparseModelType>(model, memory).build();
}

template<class SparseModelType>
std::shared_ptr<SparseModelType> MemoryIncorporation<SparseModelType>::incorporateCountingMemory(SparseModelType const& model, uint64_t memoryStates) {
    auto memory =
        storm::storage::NondeterministicMemoryStructureBuilder().build(storm::storage::NondeterministicMemoryStructurePattern::SelectiveCounter, memoryStates);
    return storm::storage::SparseModelNondeterministicMemoryProduct<SparseModelType>(model, memory).build();
}

template class MemoryIncorporation<storm::models::sparse::Mdp<double>>;
template class MemoryIncorporation<storm::models::sparse::MarkovAutomaton<double>>;

template class MemoryIncorporation<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class MemoryIncorporation<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;

}  // namespace transformer
}  // namespace storm
