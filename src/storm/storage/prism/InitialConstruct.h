#pragma once

#include <map>
#include <string>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/prism/LocatedInformation.h"

namespace storm {
namespace expressions {
class Variable;
}
}  // namespace storm

namespace storm {
namespace prism {
class InitialConstruct : public LocatedInformation {
   public:
    /*!
     * Creates an initial construct with the given expression.
     *
     * @param initialStatesExpression An expression characterizing the initial states.
     * @param filename The filename in which the command is defined.
     * @param lineNumber The line number in which the command is defined.
     */
    InitialConstruct(storm::expressions::Expression initialStatesExpression, std::string const& filename = "", uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    InitialConstruct() = default;
    InitialConstruct(InitialConstruct const& other) = default;
    InitialConstruct& operator=(InitialConstruct const& other) = default;
    InitialConstruct(InitialConstruct&& other) = default;
    InitialConstruct& operator=(InitialConstruct&& other) = default;

    /*!
     * Retrieves the expression characterizing the initial states.
     *
     * @return The expression characterizing the initial states.
     */
    storm::expressions::Expression getInitialStatesExpression() const;

    /*!
     * Substitutes all identifiers in the constant according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting initial construct.
     */
    InitialConstruct substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;

    friend std::ostream& operator<<(std::ostream& stream, InitialConstruct const& initialConstruct);

   private:
    // An expression characterizing the initial states.
    storm::expressions::Expression initialStatesExpression;
};
}  // namespace prism
}  // namespace storm
