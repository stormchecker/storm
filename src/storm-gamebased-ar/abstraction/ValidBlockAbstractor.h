#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "storm-gamebased-ar/abstraction/LocalExpressionInformation.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/dd/DdType.h"

namespace storm {
namespace utility {
namespace solver {
class SmtSolverFactory;
}
}  // namespace utility
}  // namespace storm

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType>
class AbstractionInformation;

template<storm::dd::DdType DdType>
class ValidBlockAbstractor {
   public:
    ValidBlockAbstractor(AbstractionInformation<DdType>& abstractionInformation,
                         std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory);

    storm::dd::Bdd<DdType> const& getValidBlocks();

    void refine(std::vector<uint64_t> const& predicates);

    void constrain(storm::expressions::Expression const& constraint);

   private:
    /*!
     * Checks which parts of the valid blocks need to be recomputed.
     */
    void recomputeValidBlocks();

    /*!
     * Recomputed the valid blocks for the given predicate block.
     */
    void recomputeValidBlocksForPredicateBlock(uint64_t blockIndex);

    /*!
     * Retrieves the abstraction information object.
     */
    AbstractionInformation<DdType> const& getAbstractionInformation() const;

    /*!
     * Translates the given model to a source state DD.
     *
     * @param model The model to translate.
     * @param blockIndex The index of the block.
     * @return The source state encoded as a DD.
     */
    storm::dd::Bdd<DdType> getSourceStateBdd(storm::solver::SmtSolver::ModelReference const& model, uint64_t blockIndex) const;

    /// The object storing the information about the abstraction (like predicates etc.)
    std::reference_wrapper<AbstractionInformation<DdType> const> abstractionInformation;

    /// An object storing information about how predicates are related.
    LocalExpressionInformation<DdType> localExpressionInformation;

    /// The BDD storing all valid blocks;
    storm::dd::Bdd<DdType> validBlocks;

    /// A vector of SMT solvers that correspond to the predicate blocks in the local expression information.
    std::vector<std::unique_ptr<storm::solver::SmtSolver>> smtSolvers;

    /// A vector of relevant variables and predicates. Every inner vector corresponds to one block of the local
    /// expression information.
    std::vector<std::vector<std::pair<storm::expressions::Variable, uint64_t>>> relevantVariablesAndPredicates;

    /// The decision variables for each predicate block.
    std::vector<std::vector<storm::expressions::Variable>> decisionVariables;

    /// A vector of BDDs that store the valid blocks for the individual predicate blocks to be able to reuse them.
    std::vector<storm::dd::Bdd<DdType>> validBlocksForPredicateBlocks;

    /// A flag that stores whether we need to possibly recompute the valid blocks (or parts).
    bool checkForRecomputation;
};

}  // namespace abstraction
}  // namespace storm::gbar
