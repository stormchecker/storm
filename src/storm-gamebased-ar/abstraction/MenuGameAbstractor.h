#pragma once

#include <map>
#include <vector>

#include "storm-gamebased-ar/abstraction/MenuGame.h"
#include "storm-gamebased-ar/abstraction/RefinementCommand.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace dd {
template<storm::dd::DdType DdType>
class Bdd;
}
}  // namespace storm

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType>
class AbstractionInformation;

struct MenuGameAbstractorOptions {
    MenuGameAbstractorOptions() = default;
    MenuGameAbstractorOptions(std::vector<storm::expressions::Expression>&& constraints) : constraints(std::move(constraints)) {
        // Intentionally left empty.
    }

    std::vector<storm::expressions::Expression> constraints;
};

template<storm::dd::DdType DdType, typename ValueType>
class MenuGameAbstractor {
   public:
    MenuGameAbstractor();
    virtual ~MenuGameAbstractor() = default;

    virtual storm::dd::DdManager<DdType> const& getDdManager() const = 0;

    /// Retrieves the abstraction.
    virtual MenuGame<DdType, ValueType> abstract() = 0;

    /// Retrieves information about the abstraction.
    virtual AbstractionInformation<DdType> const& getAbstractionInformation() const = 0;
    virtual storm::expressions::Expression const& getGuard(uint64_t player1Choice) const = 0;
    virtual std::pair<uint64_t, uint64_t> getPlayer1ChoiceRange() const = 0;
    virtual uint64_t getNumberOfUpdates(uint64_t player1Choice) const = 0;
    std::vector<std::map<storm::expressions::Variable, storm::expressions::Expression>> getVariableUpdates(uint64_t player1Choice) const;
    virtual std::map<storm::expressions::Variable, storm::expressions::Expression> getVariableUpdates(uint64_t player1Choice,
                                                                                                      uint64_t auxiliaryChoice) const = 0;
    virtual std::set<storm::expressions::Variable> const& getAssignedVariables(uint64_t player1Choice) const = 0;
    virtual storm::expressions::Expression getInitialExpression() const = 0;

    /*!
     * Retrieves a BDD that characterizes the states corresponding to the given expression. For this to work,
     * appropriate predicates must have been used to refine the abstraction, otherwise this will fail.
     */
    virtual storm::dd::Bdd<DdType> getStates(storm::expressions::Expression const& expression) = 0;

    /// Methods to refine the abstraction.
    virtual void refine(RefinementCommand const& command) = 0;

    /// Exports a representation of the current abstraction state in the dot format.
    virtual void exportToDot(std::string const& filename, storm::dd::Bdd<DdType> const& highlightStatesBdd, storm::dd::Bdd<DdType> const& filter) const = 0;

    /// Retrieves the number of predicates currently in use.
    virtual uint64_t getNumberOfPredicates() const = 0;

    /*!
     * Adds the expression to the ones characterizing terminal states, i.e. states whose transitions are not
     * explored. For this to work, appropriate predicates must have been used to refine the abstraction,
     * otherwise this will fail.
     */
    virtual void addTerminalStates(storm::expressions::Expression const& expression) = 0;

    /*!
     * Sets the expression characterizing the target states. For this to work, appropriate predicates must have
     * been used to refine the abstraction, otherwise this will fail.
     */
    void setTargetStates(storm::expressions::Expression const& targetStateExpression);

    storm::expressions::Expression const& getTargetStateExpression() const;
    bool hasTargetStateExpression() const;

    /*!
     * Notifies the abstractor that the guards are predicates, which may be used to improve the bottom state
     * computation.
     */
    virtual void notifyGuardsArePredicates() = 0;

   protected:
    bool isRestrictToRelevantStatesSet() const;

    void exportToDot(storm::gbar::abstraction::MenuGame<DdType, ValueType> const& currentGame, std::string const& filename,
                     storm::dd::Bdd<DdType> const& highlightStatesBdd, storm::dd::Bdd<DdType> const& filter) const;

   private:
    bool restrictToRelevantStates;

    // An expression characterizing the target states.
    storm::expressions::Expression targetStateExpression;
};

}  // namespace abstraction
}  // namespace storm::gbar
