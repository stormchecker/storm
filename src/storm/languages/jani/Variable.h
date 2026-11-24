#pragma once

#include <boost/optional.hpp>
#include <string>

#include "storm/expressions/Expression.h"
#include "storm/expressions/Variable.h"
#include "storm/languages/jani/types/AllJaniTypes.h"

namespace storm {
namespace jani {
class Variable {
   public:
    /*!
     * Creates a new variable with initial value construct
     */
    Variable(std::string const& name, JaniType const& type, storm::expressions::Variable const& variable, storm::expressions::Expression const& init,
             bool transient = false);

    /*!
     * Creates a new variable without initial value construct.
     */
    Variable(std::string const& name, JaniType const& type, storm::expressions::Variable const& variable);

    /*!
     * Clones the variable.
     */
    std::unique_ptr<Variable> clone() const;

    /*!
     * Retrieves the associated expression variable
     */
    storm::expressions::Variable const& getExpressionVariable() const;

    /*!
     * Sets the associated expression variable.
     */
    void setExpressionVariable(storm::expressions::Variable const& newVariable);

    /*!
     * Retrieves the name of the variable.
     */
    std::string const& getName() const;

    /*!
     * Sets the name of the variable.
     */
    void setName(std::string const& newName);

    /*!
     * Retrieves whether an initial expression is set.
     */
    bool hasInitExpression() const;

    /*!
     * Retrieves the initial expression
     * Should only be called if an initial expression is set for this variable.
     *
     * @see hasInitExpression()
     */
    storm::expressions::Expression const& getInitExpression() const;

    /*!
     * Sets the initial expression for this variable.
     */
    void setInitExpression(storm::expressions::Expression const& initialExpression);

    bool isTransient() const;

    JaniType& getType();
    JaniType const& getType() const;

    /*!
     * Retrieves an expression characterizing the legal range of the variable.
     * If the type is a bounded type, the expression will be of the form "l <= x && x <= u".
     * Otherwise, an uninitialized expression is returned.
     */
    storm::expressions::Expression getRangeExpression() const;

    ~Variable();

    /*!
     * Substitutes all variables in all expressions according to the given substitution.
     */
    void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution, bool const substituteTranscendentalNumbers);

    /**
     * Convenience functions to call the appropriate constructor and return a shared pointer to the variable.
     */
    static std::shared_ptr<Variable> makeVariable(std::string const& name, JaniType const& type, storm::expressions::Variable const& variable,
                                                  boost::optional<storm::expressions::Expression> const& initValue, bool transient);
    static std::shared_ptr<Variable> makeBasicTypeVariable(std::string const& name, BasicType::Type const& type, storm::expressions::Variable const& variable,
                                                           boost::optional<storm::expressions::Expression> const& initValue, bool transient);
    static std::shared_ptr<Variable> makeBooleanVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                         boost::optional<storm::expressions::Expression> const& initValue, bool transient);
    static std::shared_ptr<Variable> makeIntegerVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                         boost::optional<storm::expressions::Expression> const& initValue, bool transient);
    static std::shared_ptr<Variable> makeRealVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                      boost::optional<storm::expressions::Expression> const& initValue, bool transient);
    static std::shared_ptr<Variable> makeBoundedVariable(std::string const& name, BoundedType::BaseType const& type,
                                                         storm::expressions::Variable const& variable,
                                                         boost::optional<storm::expressions::Expression> const& initValue, bool transient,
                                                         boost::optional<storm::expressions::Expression> const& lowerBound,
                                                         boost::optional<storm::expressions::Expression> const& upperBound);
    static std::shared_ptr<Variable> makeBoundedIntegerVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                                boost::optional<storm::expressions::Expression> const& initValue, bool transient,
                                                                boost::optional<storm::expressions::Expression> const& lowerBound,
                                                                boost::optional<storm::expressions::Expression> const& upperBound);
    static std::shared_ptr<Variable> makeBoundedRealVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                             boost::optional<storm::expressions::Expression> const& initValue, bool transient,
                                                             boost::optional<storm::expressions::Expression> const& lowerBound,
                                                             boost::optional<storm::expressions::Expression> const& upperBound);
    static std::shared_ptr<Variable> makeArrayVariable(std::string const& name, JaniType const& baseType, storm::expressions::Variable const& variable,
                                                       boost::optional<storm::expressions::Expression> const& initValue, bool transient);
    static std::shared_ptr<Variable> makeClockVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                       boost::optional<storm::expressions::Expression> const& initValue, bool transient);
    static std::shared_ptr<Variable> makeContinuousVariable(std::string const& name, storm::expressions::Variable const& variable,
                                                            boost::optional<storm::expressions::Expression> const& initValue, bool transient);

   private:
    /// The name of the variable.
    std::string name;
    /// The type of the variable (for arrays this is the underlying type, e.g. int for int[][])
    std::unique_ptr<JaniType> type;
    /// The expression variable associated with this jani variable.
    storm::expressions::Variable variable;
    /// Expression for initial values
    storm::expressions::Expression init;
    /// Whether this is a transient variable.
    bool transient;
};

bool operator==(Variable const& lhs, Variable const& rhs);
bool operator!=(Variable const& lhs, Variable const& rhs);

}  // namespace jani
}  // namespace storm
