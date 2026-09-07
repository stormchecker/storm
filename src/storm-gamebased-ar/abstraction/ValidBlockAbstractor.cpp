#include "ValidBlockAbstractor.h"

#include "storm-gamebased-ar/abstraction/AbstractionInformation.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/solver.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType>
ValidBlockAbstractor<DdType>::ValidBlockAbstractor(AbstractionInformation<DdType>& abstractionInformation,
                                                   std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory)
    : abstractionInformation(abstractionInformation),
      localExpressionInformation(abstractionInformation),
      validBlocks(abstractionInformation.getDdManager().getBddOne()),
      checkForRecomputation(false) {
    uint64_t numberOfBlocks = localExpressionInformation.getNumberOfBlocks();
    validBlocksForPredicateBlocks.resize(numberOfBlocks, abstractionInformation.getDdManager().getBddOne());
    relevantVariablesAndPredicates.resize(numberOfBlocks);
    decisionVariables.resize(numberOfBlocks);

    for (uint64_t i = 0; i < numberOfBlocks; ++i) {
        smtSolvers.emplace_back(smtSolverFactory->create(abstractionInformation.getExpressionManager()));
        for (auto const& constraint : abstractionInformation.getConstraints()) {
            smtSolvers.back()->add(constraint);
        }
    }
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> const& ValidBlockAbstractor<DdType>::getValidBlocks() {
    if (checkForRecomputation) {
        recomputeValidBlocks();
    }

    return validBlocks;
}

template<storm::dd::DdType DdType>
void ValidBlockAbstractor<DdType>::constrain(storm::expressions::Expression const& constraint) {
    for (uint64_t i = 0; i < smtSolvers.size(); ++i) {
        smtSolvers[i]->add(constraint);
    }
}

template<storm::dd::DdType DdType>
void ValidBlockAbstractor<DdType>::refine(std::vector<uint64_t> const& predicates) {
    for (auto const& predicate : predicates) {
        std::map<uint64_t, uint64_t> mergeInformation = localExpressionInformation.addExpression(predicate);

        // Perform the remapping caused by merges.
        for (auto const& blockRemapping : mergeInformation) {
            if (blockRemapping.first == blockRemapping.second) {
                continue;
            }

            validBlocksForPredicateBlocks[blockRemapping.second] = validBlocksForPredicateBlocks[blockRemapping.first];
            smtSolvers[blockRemapping.second] = std::move(smtSolvers[blockRemapping.first]);
            relevantVariablesAndPredicates[blockRemapping.second] = std::move(relevantVariablesAndPredicates[blockRemapping.first]);
            decisionVariables[blockRemapping.second] = std::move(decisionVariables[blockRemapping.first]);
        }
        validBlocksForPredicateBlocks.resize(localExpressionInformation.getNumberOfBlocks());
        smtSolvers.resize(localExpressionInformation.getNumberOfBlocks());
        relevantVariablesAndPredicates.resize(localExpressionInformation.getNumberOfBlocks());
        decisionVariables.resize(localExpressionInformation.getNumberOfBlocks());
    }
    checkForRecomputation = true;
}

template<storm::dd::DdType DdType>
void ValidBlockAbstractor<DdType>::recomputeValidBlocks() {
    storm::dd::Bdd<DdType> newValidBlocks = abstractionInformation.get().getDdManager().getBddOne();

    for (uint64_t blockIndex = 0; blockIndex < localExpressionInformation.getNumberOfBlocks(); ++blockIndex) {
        std::set<uint64_t> const& predicateBlock = localExpressionInformation.getExpressionBlock(blockIndex);

        // If the size of the block changed, we need to add the appropriate variables and recompute the solution.
        if (relevantVariablesAndPredicates[blockIndex].size() < predicateBlock.size()) {
            recomputeValidBlocksForPredicateBlock(blockIndex);
        }

        newValidBlocks &= validBlocksForPredicateBlocks[blockIndex];
    }

    validBlocks = newValidBlocks;
}

template<storm::dd::DdType DdType>
void ValidBlockAbstractor<DdType>::recomputeValidBlocksForPredicateBlock(uint64_t blockIndex) {
    std::set<uint64_t> const& predicateBlock = localExpressionInformation.getExpressionBlock(blockIndex);
    std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> newVariables =
        this->getAbstractionInformation().declareNewVariables(relevantVariablesAndPredicates[blockIndex], predicateBlock);
    for (auto const& element : newVariables) {
        smtSolvers[blockIndex]->add(storm::expressions::iff(element.first, this->getAbstractionInformation().getPredicateByIndex(element.second)));
        decisionVariables[blockIndex].push_back(element.first);
    }

    relevantVariablesAndPredicates[blockIndex].insert(relevantVariablesAndPredicates[blockIndex].end(), newVariables.begin(), newVariables.end());
    std::sort(relevantVariablesAndPredicates[blockIndex].begin(), relevantVariablesAndPredicates[blockIndex].end(),
              [](std::pair<storm::expressions::Variable, uint_fast64_t> const& first, std::pair<storm::expressions::Variable, uint_fast64_t> const& second) {
                  return first.second < second.second;
              });

    // Use all-sat to enumerate all valid blocks for this predicate block.
    storm::dd::Bdd<DdType> result = this->getAbstractionInformation().getDdManager().getBddZero();
    uint64_t numberOfSolutions = 0;
    smtSolvers[blockIndex]->allSat(decisionVariables[blockIndex],
                                   [&result, this, &numberOfSolutions, blockIndex](storm::solver::SmtSolver::ModelReference const& model) {
                                       result |= getSourceStateBdd(model, blockIndex);
                                       ++numberOfSolutions;
                                       return true;
                                   });

    STORM_LOG_TRACE("Computed valid blocks for predicate block " << blockIndex << " (" << numberOfSolutions << " solutions enumerated).");

    validBlocksForPredicateBlocks[blockIndex] = result;
}

template<storm::dd::DdType DdType>
AbstractionInformation<DdType> const& ValidBlockAbstractor<DdType>::getAbstractionInformation() const {
    return abstractionInformation.get();
}

template<storm::dd::DdType DdType>
storm::dd::Bdd<DdType> ValidBlockAbstractor<DdType>::getSourceStateBdd(storm::solver::SmtSolver::ModelReference const& model, uint64_t blockIndex) const {
    storm::dd::Bdd<DdType> result = this->getAbstractionInformation().getDdManager().getBddOne();
    for (auto variableIndexPairIt = relevantVariablesAndPredicates[blockIndex].rbegin(),
              variableIndexPairIte = relevantVariablesAndPredicates[blockIndex].rend();
         variableIndexPairIt != variableIndexPairIte; ++variableIndexPairIt) {
        auto const& variableIndexPair = *variableIndexPairIt;
        if (model.getBooleanValue(variableIndexPair.first)) {
            result &= this->getAbstractionInformation().encodePredicateAsSource(variableIndexPair.second);
        } else {
            result &= !this->getAbstractionInformation().encodePredicateAsSource(variableIndexPair.second);
        }
    }

    STORM_LOG_ASSERT(!result.isZero(), "Source must not be empty.");
    return result;
}

template class ValidBlockAbstractor<storm::dd::DdType::CUDD>;
template class ValidBlockAbstractor<storm::dd::DdType::Sylvan>;

}  // namespace abstraction
}  // namespace storm::gbar
