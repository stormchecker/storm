#pragma once

#include "storm/storage/dd/DdType.h"

#include "storm-gamebased-ar/abstraction/AbstractionInformation.h"
#include "storm-gamebased-ar/abstraction/MenuGame.h"
#include "storm-gamebased-ar/abstraction/MenuGameAbstractor.h"
#include "storm-gamebased-ar/abstraction/RefinementCommand.h"
#include "storm-gamebased-ar/abstraction/ValidBlockAbstractor.h"
#include "storm-gamebased-ar/abstraction/jani/AutomatonAbstractor.h"

#include "storm/storage/dd/Add.h"

#include "storm/storage/expressions/Expression.h"

#include "storm/settings/modules/AbstractionSettings.h"

namespace storm {
namespace utility {
namespace solver {
class SmtSolverFactory;
}
}  // namespace utility

class Environment;

namespace models {
namespace symbolic {
template<storm::dd::DdType Type, typename ValueType>
class StochasticTwoPlayerGame;
}
}  // namespace models

namespace jani {
// Forward-declare concrete Model class.
class Model;
}  // namespace jani
}  // namespace storm

namespace storm::gbar {
namespace abstraction {
namespace jani {

template<storm::dd::DdType DdType, typename ValueType>
class JaniMenuGameAbstractor : public MenuGameAbstractor<DdType, ValueType> {
   public:
    /*!
     * Constructs an abstractor for the given model.
     *
     * @param env The environment providing the settings for the DD manager.
     * @param model The concrete model for which to build the abstraction.
     * @param smtSolverFactory A factory that is to be used for creating new SMT solvers.
     */
    JaniMenuGameAbstractor(storm::Environment const& env, storm::jani::Model const& model,
                           std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory,
                           MenuGameAbstractorOptions const& options = MenuGameAbstractorOptions());

    JaniMenuGameAbstractor(JaniMenuGameAbstractor const&) = default;
    JaniMenuGameAbstractor& operator=(JaniMenuGameAbstractor const&) = default;
    JaniMenuGameAbstractor(JaniMenuGameAbstractor&&) = default;
    JaniMenuGameAbstractor& operator=(JaniMenuGameAbstractor&&) = default;

    storm::dd::DdManager<DdType> const& getDdManager() const override;

    /*!
     * Uses the current set of predicates to derive the abstract menu game in the form of an ADD.
     *
     * @return The abstract stochastic two player game.
     */
    MenuGame<DdType, ValueType> abstract() override;

    /*!
     * Retrieves information about the abstraction.
     *
     * @return The abstraction information object.
     */
    AbstractionInformation<DdType> const& getAbstractionInformation() const override;

    /*!
     * Retrieves the guard predicate of the given player 1 choice.
     *
     * @param player1Choice The choice for which to retrieve the guard.
     * @return The guard of the player 1 choice.
     */
    storm::expressions::Expression const& getGuard(uint64_t player1Choice) const override;

    /*!
     * Retrieves the number of updates of the specified player 1 choice.
     */
    virtual uint64_t getNumberOfUpdates(uint64_t player1Choice) const override;

    /*!
     * Retrieves a mapping from variables to expressions that define their updates wrt. to the given player
     * 1 choice and auxiliary choice.
     */
    std::map<storm::expressions::Variable, storm::expressions::Expression> getVariableUpdates(uint64_t player1Choice, uint64_t auxiliaryChoice) const override;

    /*!
     * Retrieves the variables assigned by the given player 1 choice.
     */
    virtual std::set<storm::expressions::Variable> const& getAssignedVariables(uint64_t player1Choice) const override;

    /*!
     * Retrieves the range of player 1 choices.
     */
    std::pair<uint64_t, uint64_t> getPlayer1ChoiceRange() const override;

    /*!
     * Retrieves the expression that characterizes the initial states.
     */
    storm::expressions::Expression getInitialExpression() const override;

    storm::dd::Bdd<DdType> getStates(storm::expressions::Expression const& expression) override;

    /*!
     * Performs the given refinement command.
     *
     * @param command The command to perform.
     */
    virtual void refine(RefinementCommand const& command) override;

    /*!
     * Exports the current state of the abstraction in the dot format to the given file.
     *
     * @param filename The name of the file to which to write the dot output.
     * @param highlightStates A BDD characterizing states that will be highlighted.
     * @param filter A filter that is applied to select which part of the game to export.
     */
    virtual void exportToDot(std::string const& filename, storm::dd::Bdd<DdType> const& highlightStates, storm::dd::Bdd<DdType> const& filter) const override;

    virtual uint64_t getNumberOfPredicates() const override;

    virtual void addTerminalStates(storm::expressions::Expression const& expression) override;

    virtual void notifyGuardsArePredicates() override;

   protected:
    using MenuGameAbstractor<DdType, ValueType>::exportToDot;

   private:
    /*!
     * Builds the stochastic game representing the abstraction of the program.
     *
     * @return The stochastic game.
     */
    std::unique_ptr<MenuGame<DdType, ValueType>> buildGame();

    // The concrete model this abstractor refers to.
    std::reference_wrapper<storm::jani::Model const> model;

    // A factory that can be used to create new SMT solvers.
    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory;

    // An object containing all information about the abstraction like predicates and the corresponding DDs.
    AbstractionInformation<DdType> abstractionInformation;

    // The abstract modules of the abstract program.
    std::vector<AutomatonAbstractor<DdType, ValueType>> automata;

    // A state-set abstractor used to determine the initial states of the abstraction.
    StateSetAbstractor<DdType, ValueType> initialStateAbstractor;

    // A flag indicating whether the valid blocks need to be computed and the game restricted to these.
    bool restrictToValidBlocks;

    // An object that is used to compute the valid blocks.
    ValidBlockAbstractor<DdType> validBlockAbstractor;

    // A BDD characterizing the initial locations of the abstracted automata.
    storm::dd::Bdd<DdType> initialLocationsBdd;

    // An ADD characterizing the probabilities and source/target locations of edges and their updates.
    storm::dd::Add<DdType, ValueType> edgeDecoratorAdd;

    // The current game-based abstraction.
    std::unique_ptr<MenuGame<DdType, ValueType>> currentGame;

    // A flag storing whether a refinement was performed.
    bool refinementPerformed;

    // A list of terminal state expressions.
    std::vector<storm::expressions::Expression> terminalStateExpressions;
};
}  // namespace jani
}  // namespace abstraction
}  // namespace storm::gbar
