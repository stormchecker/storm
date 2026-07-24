#pragma once

#include <cstdint>
#include <iosfwd>
#include <map>
#include <memory>
#include <set>

#include "storm/adapters/RationalNumberForward.h"
#include "storm/storage/expressions/Type.h"
#include "storm/utility/OsDetection.h"

namespace boost {
class any;
}

namespace storm {
namespace expressions {
// Forward-declare expression classes.
class ExpressionManager;
class Variable;
class Expression;
class Valuation;
class ExpressionVisitor;
enum struct OperatorType;

class IfThenElseExpression;
class BinaryBooleanFunctionExpression;
class BinaryNumericalFunctionExpression;
class BinaryRelationExpression;
class BooleanLiteralExpression;
class IntegerLiteralExpression;
class RationalLiteralExpression;
class UnaryBooleanFunctionExpression;
class UnaryNumericalFunctionExpression;
class VariableExpression;
class PredicateExpression;

/*!
 * The base class of all expression classes.
 */
class BaseExpression : public std::enable_shared_from_this<BaseExpression> {
   public:
    /*!
     * Constructs a base expression with the given return type.
     *
     * @param type The type of the expression.
     */
    BaseExpression(ExpressionManager const& manager, Type const& type);

    // Create default versions of constructors and assignments.
    BaseExpression(BaseExpression const&) = default;
    BaseExpression& operator=(BaseExpression const&) = delete;
    BaseExpression(BaseExpression&&) = default;
    BaseExpression& operator=(BaseExpression&&) = delete;

    // Make the destructor virtual (to allow destruction via base class pointer) and default it.
    virtual ~BaseExpression() = default;

    /*!
     * Converts the base expression to a proper expression.
     */
    Expression toExpression() const;

    /*!
     * Evaluates the expression under the valuation of unknowns (variables and constants) given by the
     * valuation and returns the resulting boolean value. If the return type of the expression is not a boolean
     * an exception is thrown.
     *
     * @param valuation The valuation of unknowns under which to evaluate the expression.
     * @return The boolean value of the expression under the given valuation.
     */
    virtual bool evaluateAsBool(Valuation const* valuation = nullptr) const;

    /*!
     * Evaluates the expression under the valuation of unknowns (variables and constants) given by the
     * valuation and returns the resulting integer value. If the return type of the expression is not an integer
     * an exception is thrown.
     *
     * @param valuation The valuation of unknowns under which to evaluate the expression.
     * @return The integer value of the expression under the given valuation.
     */
    virtual int_fast64_t evaluateAsInt(Valuation const* valuation = nullptr) const;

    /*!
     * Evaluates the expression under the valuation of unknowns (variables and constants) given by the
     * valuation and returns the resulting double value. If the return type of the expression is not a double
     * an exception is thrown.
     *
     * @param valuation The valuation of unknowns under which to evaluate the expression.
     * @return The double value of the expression under the given valuation.
     */
    virtual double evaluateAsDouble(Valuation const* valuation = nullptr) const;

    /*!
     * Evaluates the expression and returns the resulting rational number.
     * If the return type of the expression is not a rational number
     * or the expression could not be evaluated, an exception is thrown.
     *
     * @return The rational number value of the expression.
     */
    virtual storm::RationalNumber evaluateAsRational() const;

    /*!
     * Returns the arity of the expression.
     *
     * @return The arity of the expression.
     */
    virtual uint_fast64_t getArity() const;

    /*!
     * Retrieves the given operand from the expression.
     *
     * @param operandIndex The index of the operand to retrieve. This must be lower than the arity of the expression.
     * @return The operand at the given index.
     */
    virtual std::shared_ptr<BaseExpression const> getOperand(uint_fast64_t operandIndex) const;

    /*!
     * Retrieves the identifier associated with this expression. This is only legal to call if the expression
     * is a variable.
     *
     * @return The identifier associated with this expression.
     */
    virtual std::string const& getIdentifier() const;

    /*!
     * Retrieves the operator of a function application. This is only legal to call if the expression is
     * function application.
     *
     * @return The operator associated with the function application.
     */
    virtual OperatorType getOperator() const;

    /*!
     * Retrieves whether the expression contains a variable.
     *
     * @return True iff the expression contains a variable.
     */
    virtual bool containsVariables() const;

    /*!
     * Retrieves whether the expression is a literal.
     *
     * @return True iff the expression is a literal.
     */
    virtual bool isLiteral() const;

    /*!
     * Retrieves whether the expression is a variable.
     *
     * @return True iff the expression is a variable.
     */
    virtual bool isVariable() const;

    /*!
     * Checks if the expression is equal to the boolean literal true.
     *
     * @return True iff the expression is equal to the boolean literal true.
     */
    virtual bool isTrue() const;

    /*!
     * Checks if the expression is equal to the boolean literal false.
     *
     * @return True iff the expression is equal to the boolean literal false.
     */
    virtual bool isFalse() const;

    /*!
     * Checks if the expression is a function application (of any sort).
     *
     * @return True iff the expression is a function application.
     */
    virtual bool isFunctionApplication() const;

    /*!
     * Retrieves the set of all variables that appear in the expression.
     *
     * @param The set into which all variables in this expresson are inserted.
     */
    virtual void gatherVariables(std::set<storm::expressions::Variable>& variables) const = 0;

    /*!
     * Simplifies the expression according to some simple rules.
     *
     * @return A pointer to the simplified expression.
     */
    virtual std::shared_ptr<BaseExpression const> simplify() const = 0;

    /*!
     * Tries to flatten the syntax tree of the expression, e.g., 1 + (2 + (3 + 4)) becomes (1 + 2) + (3 + 4)
     *
     * @return A semantically equivalent expression with reduced nesting
     */
    std::shared_ptr<BaseExpression const> reduceNesting() const;

    /*!
     * Accepts the given visitor by calling its visit method.
     *
     * @param visitor The visitor that is to be accepted.
     */
    virtual boost::any accept(ExpressionVisitor& visitor, boost::any const& data) const = 0;

    /*!
     * Retrieves whether the expression has a numerical type, i.e., integer or double.
     *
     * @return True iff the expression has a numerical type.
     */
    bool hasNumericalType() const;

    /*!
     * Retrieves whether the expression has an integer type.
     *
     * @return True iff the expression has an integer type.
     */
    bool hasIntegerType() const;

    /*!
     * Retrieves whether the expression has a bitvector type.
     *
     * @return True iff the expression has a bitvector type.
     */
    bool hasBitVectorType() const;

    /*!
     * Retrieves whether the expression has a boolean type.
     *
     * @return True iff the expression has a boolean type.
     */
    bool hasBooleanType() const;

    /*!
     * Retrieves whether the expression has a rational return type.
     *
     * @return True iff the expression has a rational return type.
     */
    bool hasRationalType() const;

    /*!
     * Retrieves a shared pointer to this expression.
     *
     * @return A shared pointer to this expression.
     */
    std::shared_ptr<BaseExpression const> getSharedPointer() const;

    /*!
     * Retrieves the manager responsible for this expression.
     *
     * @return The manager responsible for this expression.
     */
    ExpressionManager const& getManager() const;

    /*!
     * Retrieves the type of the expression.
     *
     * @return The type of the expression.
     */
    Type const& getType() const;

    friend std::ostream& operator<<(std::ostream& stream, BaseExpression const& expression);

    virtual bool isIfThenElseExpression() const;
    IfThenElseExpression const& asIfThenElseExpression() const;

    virtual bool isBinaryBooleanFunctionExpression() const;
    BinaryBooleanFunctionExpression const& asBinaryBooleanFunctionExpression() const;

    virtual bool isBinaryNumericalFunctionExpression() const;
    BinaryNumericalFunctionExpression const& asBinaryNumericalFunctionExpression() const;

    virtual bool isBinaryRelationExpression() const;
    BinaryRelationExpression const& asBinaryRelationExpression() const;

    virtual bool isBooleanLiteralExpression() const;
    BooleanLiteralExpression const& asBooleanLiteralExpression() const;

    virtual bool isIntegerLiteralExpression() const;
    IntegerLiteralExpression const& asIntegerLiteralExpression() const;

    virtual bool isRationalLiteralExpression() const;
    RationalLiteralExpression const& asRationalLiteralExpression() const;

    virtual bool isUnaryBooleanFunctionExpression() const;
    UnaryBooleanFunctionExpression const& asUnaryBooleanFunctionExpression() const;

    virtual bool isUnaryNumericalFunctionExpression() const;
    UnaryNumericalFunctionExpression const& asUnaryNumericalFunctionExpression() const;

    virtual bool isVariableExpression() const;
    VariableExpression const& asVariableExpression() const;

    virtual bool isPredicateExpression() const;
    PredicateExpression const& asPredicateExpression() const;

   protected:
    /*!
     * Prints the expression to the given stream.
     *
     * @param stream The stream to which to write the expression.
     */
    virtual void printToStream(std::ostream& stream) const = 0;

   private:
    // The manager responsible for this expression.
    ExpressionManager const& manager;

    // The return type of this expression.
    Type type;
};
}  // namespace expressions
}  // namespace storm
