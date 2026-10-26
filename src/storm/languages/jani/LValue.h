#pragma once

#include "storm/expressions/Expressions.h"
#include "storm/languages/jani/Variable.h"

namespace storm {
namespace jani {

class LValue {
   public:
    explicit LValue(storm::jani::Variable const& variable);
    LValue(storm::jani::Variable const&, std::vector<storm::expressions::Expression> const& index);

    bool operator==(LValue const& other) const;

    bool isVariable() const;         /// Returns true if the lValue refers to a variable (potentially of type array).
    bool isArray() const;            /// Returns true if the lValue refers either to an array variable or to an array access
    bool isArrayAccess() const;      /// Returns true if the lValue refers to an array access
    bool isFullArrayAccess() const;  /// To check if the array is fully accessed, so result will be of the most inner child Type. (bool[][] will be bool)
    storm::jani::Variable const& getVariable() const;  /// Returns the referred variable. In case of an array access, this is the referred array variable.
    std::vector<storm::expressions::Expression>& getArrayIndexVector();
    std::vector<storm::expressions::Expression> const& getArrayIndexVector() const;
    std::string getName() const;

    bool arrayIndexContainsVariable() const;
    void setArrayIndex(std::vector<storm::expressions::Expression> const& newIndex);

    /*!
     * Adds an array access index. Assumes that the underlying variable is an array variable that isn't fully accessed yet.
     * For example (using array variable a) a will become a[index] and a[1] will become a[1][index].
     */
    void addArrayAccessIndex(storm::expressions::Expression const& index);

    bool isTransient() const;
    bool operator<(LValue const& other) const;

    LValue changeAssignmentVariables(std::map<Variable const*, std::reference_wrapper<Variable const>> const& remapping) const;

    friend std::ostream& operator<<(std::ostream& stream, LValue const& lvalue);

   private:
    // The variable being assigned, this can either be the array variable or a variable of a different type
    storm::jani::Variable const* variable;

    // In case of an array access LValue, this is the accessed index of the array (if existing)
    std::vector<storm::expressions::Expression> arrayIndexVector;
};
}  // namespace jani
}  // namespace storm