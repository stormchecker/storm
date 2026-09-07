#pragma once

#include "storm-gamebased-ar/abstraction/prism/CommandAbstractor.h"
#include "storm/settings/modules/AbstractionSettings.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/utility/solver.h"

namespace storm {
namespace prism {
// Forward-declare concrete module class.
class Module;
}  // namespace prism
}  // namespace storm

namespace storm::gbar {
namespace abstraction {
template<storm::dd::DdType DdType>
class AbstractionInformation;

template<storm::dd::DdType DdType>
struct BottomStateResult;

template<storm::dd::DdType DdType>
struct GameBddResult;

namespace prism {
template<storm::dd::DdType DdType, typename ValueType>
class ModuleAbstractor {
   public:
    /*!
     * Constructs an abstract module from the given module.
     *
     * @param module The concrete module for which to build the abstraction.
     * @param abstractionInformation An object holding information about the abstraction such as predicates and BDDs.
     * @param smtSolverFactory A factory that is to be used for creating new SMT solvers.
     * @param useDecomposition A flag that governs whether to use the decomposition in the abstraction.
     */
    ModuleAbstractor(storm::prism::Module const& module, AbstractionInformation<DdType>& abstractionInformation,
                     std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory, bool useDecomposition, bool addPredicatesForValidBlocks,
                     bool debug);

    ModuleAbstractor(ModuleAbstractor const&) = default;
    ModuleAbstractor& operator=(ModuleAbstractor const&) = default;
    ModuleAbstractor(ModuleAbstractor&&) = default;
    ModuleAbstractor& operator=(ModuleAbstractor&&) = default;

    /*!
     * Refines the abstract module with the given predicates.
     *
     * @param predicates The new predicate indices.
     */
    void refine(std::vector<uint_fast64_t> const& predicates);

    /*!
     * Retrieves the guard of the given player 1 choice.
     *
     * @param player1Choice The choice of player 1.
     * @return The guard of the player 1 choice.
     */
    storm::expressions::Expression const& getGuard(uint64_t player1Choice) const;

    /*!
     * Retrieves the number of updates of the specified player 1 choice.
     */
    uint64_t getNumberOfUpdates(uint64_t player1Choice) const;

    /*!
     * Retrieves a mapping from variables to expressions that define their updates wrt. to the given player
     * 1 choice and auxiliary choice.
     */
    std::map<storm::expressions::Variable, storm::expressions::Expression> getVariableUpdates(uint64_t player1Choice, uint64_t auxiliaryChoice) const;

    /*!
     * Retrieves the variables assigned by the given player 1 choice.
     */
    std::set<storm::expressions::Variable> const& getAssignedVariables(uint64_t player1Choice) const;

    /*!
     * Computes the abstraction of the module wrt. to the current set of predicates.
     *
     * @return The abstraction of the module in the form of a BDD together with how many option variables were used.
     */
    GameBddResult<DdType> abstract();

    /*!
     * Retrieves the transitions to bottom states of this module.
     *
     * @param reachableStates A BDD representing the reachable states.
     * @param numberOfPlayer2Variables The number of variables used to encode the choices of player 2.
     * @return The bottom states and the necessary transitions.
     */
    BottomStateResult<DdType> getBottomStateTransitions(storm::dd::Bdd<DdType> const& reachableStates, uint_fast64_t numberOfPlayer2Variables);

    /*!
     * Retrieves an ADD that maps the encodings of commands and their updates to their probabilities.
     *
     * @return The command-update probability ADD.
     */
    storm::dd::Add<DdType, ValueType> getCommandUpdateProbabilitiesAdd() const;

    /*!
     * Retrieves the abstract commands of this abstract module.
     *
     * @return The abstract commands.
     */
    std::vector<CommandAbstractor<DdType, ValueType>> const& getCommands() const;

    /*!
     * Retrieves the abstract commands of this abstract module.
     *
     * @return The abstract commands.
     */
    std::vector<CommandAbstractor<DdType, ValueType>>& getCommands();

    void notifyGuardsArePredicates();

   private:
    /*!
     * Retrieves the abstraction information.
     *
     * @return The abstraction information.
     */
    AbstractionInformation<DdType> const& getAbstractionInformation() const;

    // A factory that can be used to create new SMT solvers.
    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory;

    // The DD-related information.
    std::reference_wrapper<AbstractionInformation<DdType> const> abstractionInformation;

    // The abstract commands of the abstract module.
    std::vector<CommandAbstractor<DdType, ValueType>> commands;

    // The concrete module this abstract module refers to.
    std::reference_wrapper<storm::prism::Module const> module;
};
}  // namespace prism
}  // namespace abstraction
}  // namespace storm::gbar
