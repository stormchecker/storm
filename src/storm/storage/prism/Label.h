#pragma once

#include <map>

#include "storm/storage/expressions/Expression.h"
#include "storm/storage/prism/LocatedInformation.h"

namespace storm {
namespace storage {
namespace expressions {
class Variable;
}
}  // namespace storage
}  // namespace storm

namespace storm {
namespace prism {
class Label : public LocatedInformation {
   public:
    /*!
     * Creates a label with the given name and state predicate expression.
     *
     * @param name The name of the label.
     * @param statePredicateExpression The predicate that needs to hold before taking a transition with the previously
     * specified name in order to obtain the reward.
     * @param filename The filename in which the transition reward is defined.
     * @param lineNumber The line number in which the transition reward is defined.
     */
    Label(std::string const& name, storm::expressions::Expression const& statePredicateExpression, std::string const& filename = "",
          uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    Label() = default;
    Label(Label const& other) = default;
    Label& operator=(Label const& other) = default;
    Label(Label&& other) = default;
    Label& operator=(Label&& other) = default;

    /*!
     * Retrieves the name that is associated with this label.
     *
     * @return The name that is associated with this label.
     */
    std::string const& getName() const;

    /*!
     * Retrieves the state predicate expression that is associated with this label.
     *
     * @return The state predicate expression that is associated with this label.
     */
    storm::expressions::Expression const& getStatePredicateExpression() const;

    /*!
     * Substitutes all identifiers in the expression of the label according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting label.
     */
    Label substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;
    Label substituteNonStandardPredicates() const;

    friend std::ostream& operator<<(std::ostream& stream, Label const& label);

   private:
    // The name of the label.
    std::string name;

    // A predicate that needs to be satisfied by states for the label to be attached.
    storm::expressions::Expression statePredicateExpression;
};

class ObservationLabel : public Label {
   public:
    /*!
     * Creates a label with the given name and state predicate expression.
     *
     * @param name The name of the label.
     * @param statePredicateExpression The predicate that needs to hold before taking a transition with the previously
     * specified name in order to obtain the reward.
     * @param filename The filename in which the transition reward is defined.
     * @param lineNumber The line number in which the transition reward is defined.
     */
    ObservationLabel(std::string const& name, storm::expressions::Expression const& statePredicateExpression, std::string const& filename = "",
                     uint_fast64_t lineNumber = 0);

    // Create default implementations of constructors/assignment.
    ObservationLabel() = default;
    ObservationLabel(ObservationLabel const& other) = default;
    ObservationLabel& operator=(ObservationLabel const& other) = default;
    ObservationLabel(ObservationLabel&& other) = default;
    ObservationLabel& operator=(ObservationLabel&& other) = default;

    /*!
     * Substitutes all identifiers in the expression of the label according to the given map.
     *
     * @param substitution The substitution to perform.
     * @return The resulting label.
     */
    ObservationLabel substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;
    ObservationLabel substituteNonStandardPredicates() const;
};

}  // namespace prism
}  // namespace storm
