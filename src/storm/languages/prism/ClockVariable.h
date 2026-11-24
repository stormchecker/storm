#pragma once

#include <map>

#include "storm/languages/prism/Variable.h"

namespace storm {
namespace prism {
class ClockVariable : public Variable {
   public:
    // Create default implementations of constructors/assignment.
    ClockVariable() = default;
    ClockVariable(ClockVariable const& other) = default;
    ClockVariable& operator=(ClockVariable const& other) = default;
    ClockVariable(ClockVariable&& other) = default;
    ClockVariable& operator=(ClockVariable&& other) = default;

    /*!
     * Creates a clock variable
     *
     * @param variable The expression variable associated with this variable.
     * @param filename The filename in which the variable is defined.
     * @param lineNumber The line number in which the variable is defined.
     */
    ClockVariable(storm::expressions::Variable const& variable, bool observable, std::string const& filename = "", uint_fast64_t lineNumber = 0);

    /*!
     * Sets a missing initial value (note that for clock variables, this is always zero)
     */
    virtual void createMissingInitialValue() override;

    friend std::ostream& operator<<(std::ostream& stream, ClockVariable const& variable);
};

}  // namespace prism
}  // namespace storm
