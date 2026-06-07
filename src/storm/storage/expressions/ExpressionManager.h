#ifndef STORM_STORAGE_EXPRESSIONS_EXPRESSIONMANAGER_H_
#define STORM_STORAGE_EXPRESSIONS_EXPRESSIONMANAGER_H_

#include <cstdint>
#include <iosfwd>
#include <iterator>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <boost/optional.hpp>

#include "storm/adapters/RationalNumberForward.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/utility/OsDetection.h"

namespace storm {
namespace expressions {
// Forward-declare manager class for iterator class.
class ExpressionManager;

class VariableIterator {
   public:
    // Define iterator
    using iterator_category = std::input_iterator_tag;
    using value_type = std::pair<storm::expressions::Variable, storm::expressions::Type> const;
    using difference_type = std::ptrdiff_t;
    using pointer = std::pair<storm::expressions::Variable, storm::expressions::Type> const*;
    using reference = std::pair<storm::expressions::Variable, storm::expressions::Type> const&;

    enum class VariableSelection { OnlyRegularVariables, OnlyAuxiliaryVariables, AllVariables };

    VariableIterator(ExpressionManager const& manager, std::unordered_map<std::string, uint_fast64_t>::const_iterator nameIndexIterator,
                     std::unordered_map<std::string, uint_fast64_t>::const_iterator nameIndexIteratorEnd, VariableSelection const& selection);
    VariableIterator(VariableIterator&& other) = default;

    // Define the basic input iterator operations.
    bool operator==(VariableIterator const& other) const;
    bool operator!=(VariableIterator const& other) const;
    value_type& operator*();
    VariableIterator& operator++(int);
    VariableIterator& operator++();

   private:
    /*!
     * Moves the current element to the next selected element or the end iterator if there is no such element.
     *
     * @param atLeastOneStep A flag indicating whether at least one step should be made.
     */
    void moveUntilNextSelectedElement(bool atLeastOneStep = true);

    // The manager responsible for the variable to iterate over.
    ExpressionManager const& manager;

    // The underlying iterator that ranges over all names and the corresponding indices.
    std::unordered_map<std::string, uint_fast64_t>::const_iterator nameIndexIterator;

    // The iterator indicating the end of the underlying iterator range.
    std::unordered_map<std::string, uint_fast64_t>::const_iterator nameIndexIteratorEnd;

    // A field indicating which variables we are supposed to iterate over.
    VariableSelection selection;

    // The current element that is shown to the outside upon dereferencing.
    std::pair<storm::expressions::Variable, storm::expressions::Type> currentElement;
};

/*!
 * This class is responsible for managing a set of typed variables and all expressions using these variables.
 */
class ExpressionManager : public std::enable_shared_from_this<ExpressionManager> {
   public:
    friend class VariableIterator;

    typedef VariableIterator const_iterator;

    /*!
     * Creates a new manager that is unaware of any variables.
     */
    ExpressionManager();

    ~ExpressionManager();

    /*!
     * Creates a new expression manager with the same set of variables.
     */
    std::shared_ptr<ExpressionManager> clone() const;

    // Create default instantiations for the move construction/assignment.
    ExpressionManager(ExpressionManager&& other) = default;
    ExpressionManager& operator=(ExpressionManager&& other) = default;

    /*!
     * Creates an expression that characterizes the given boolean literal.
     *
     * @param value The value of the boolean literal.
     * @return The resulting expression.
     */
    Expression boolean(bool value) const;

    /*!
     * Creates an expression that characterizes the given integer literal.
     *
     * @param value The value of the integer literal.
     * @return The resulting expression.
     */
    Expression integer(int_fast64_t value) const;

    /*!
     * Creates an expression that characterizes the given rational literal.
     *
     * @param value The value of the rational literal.
     * @return The resulting expression.
     */
    Expression rational(double value) const;

    /*!
     * Creates an expression that characterizes the given rational literal.
     *
     * @param value The value of the rational literal.
     * @return The resulting expression.
     */
    Expression rational(storm::RationalNumber const& value) const;

    /*!
     * Compares the two expression managers for equality, which holds iff they are the very same object.
     */
    bool operator==(ExpressionManager const& other) const;

    /*!
     * Retrieves the boolean type.
     *
     * @return The boolean type.
     */
    Type const& getBooleanType() const;

    /*!
     * Retrieves the unbounded integer type.
     *
     * @return The unbounded integer type.
     */
    Type const& getIntegerType() const;

    /*!
     * Retrieves the bit vector type of the given width.
     *
     * @param width The bit width of the bounded type.
     * @return The bounded integer type.
     */
    Type const& getBitVectorType(std::size_t width) const;

    /*!
     * Retrieves the rational type.
     *
     * @return The rational type.
     */
    Type const& getRationalType() const;

    /*!
     * Retrieves the array type with the given element type
     */
    Type const& getArrayType(Type elementType) const;

    /*!
     * Retrieves the transcendental numbers type (i.e. pi and e)
     * @return The transcendental numbers type
     */
    Type const& getTranscendentalNumberType() const;

    /*!
     * Retrieves the string type
     * @return The string type
     */
    Type const& getStringType() const;

    /*!
     * Declares a variable that is a copy of the provided variable (i.e. has the same type).
     *
     * @param variable The variable of which to create a copy.
     * @return The newly declared variable.
     */
    Variable declareVariableCopy(Variable const& variable);

    /*!
     * Declares a variable with a name that must not yet exist and its corresponding type. Note that the name
     * must not start with two underscores since these variables are reserved for internal use only.
     *
     * @param name The name of the variable.
     * @param variableType The type of the variable.
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     * @return The newly declared variable.
     */
    Variable declareVariable(std::string const& name, storm::expressions::Type const& variableType, bool auxiliary = false);

    /*!
     * Declares a new boolean variable with a name that must not yet exist and its corresponding type. Note that
     * the name must not start with two underscores since these variables are reserved for internal use only.
     *
     * @param name The name of the variable.
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     * @return The newly declared variable.
     */
    Variable declareBooleanVariable(std::string const& name, bool auxiliary = false);

    /*!
     * Declares a new integer variable with a name that must not yet exist and its corresponding type. Note that
     * the name must not start with two underscores since these variables are reserved for internal use only.
     *
     * @param name The name of the variable.
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     * @return The newly declared variable.
     */
    Variable declareIntegerVariable(std::string const& name, bool auxiliary = false);

    /*!
     * Declares a new bit vector variable with a name that must not yet exist and the bounded type of the
     * given bit width. Note that the name must not start with two underscores since these variables are
     * reserved for internal use only.
     *
     * @param name The name of the variable.
     * @param width The bit width of the bit vector type.
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     * @return The newly declared variable.
     */
    Variable declareBitVectorVariable(std::string const& name, std::size_t width, bool auxiliary = false);

    /*!
     * Declares a new rational variable with a name that must not yet exist and its corresponding type. Note that
     * the name must not start with two underscores since these variables are reserved for internal use only.
     *
     * @param name The name of the variable.
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     * @return The newly declared variable.
     */
    Variable declareRationalVariable(std::string const& name, bool auxiliary = false);

    /*!
     * Declares a new array variable with the given name and the given element type.
     */
    Variable declareArrayVariable(std::string const& name, Type const& elementType, bool auxiliary = false);

    /*!
     * Declares a new string variable with the given name
     *
     * @param name The name of the variable.
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     */
    Variable declareStringVariable(std::string const& name, bool auxiliary = false);

    /*!
     * Declares a variable with the given name if it does not yet exist.
     *
     * @param name The name of the variable to declare.
     * @param variableType The type of the variable to declare.
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     * @return The variable.
     */
    Variable declareOrGetVariable(std::string const& name, storm::expressions::Type const& variableType, bool auxiliary = false);

    /*!
     * Retrieves the expression that represents the variable with the given name.
     *
     * @param name The name of the variable to retrieve.
     */
    Variable getVariable(std::string const& name) const;

    /*!
     * Retrieves the set of all variables known to this manager.
     */
    std::set<Variable> const& getVariables() const;

    /*!
     * Retrieves whether a variable with the given name is known to the manager.
     *
     * @param name The name of the variable whose membership to query.
     * @return True iff a variable with the given name is known to the manager.
     */
    bool hasVariable(std::string const& name) const;

    /*!
     * Retrieves an expression that represents the variable with the given name.
     *
     * @param name The name of the variable
     * @return An expression that represents the variable with the given name.
     */
    Expression getVariableExpression(std::string const& name) const;

    /*!
     * Declares a variable with the given type whose name is guaranteed to be unique and not yet in use.
     *
     * @param variableType The type of the variable to declare.
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     * @param prefix The prefix which should be used.
     * @return The variable.
     */
    Variable declareFreshVariable(storm::expressions::Type const& variableType, bool auxiliary = false, std::string const& prefix = "_x");

    /*!
     * Declares a variable with rational type whose name is guaranteed to be unique and not yet in use.
     *
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     * @param prefix The prefix which should be used.
     * @return The variable.
     */
    Variable declareFreshRationalVariable(bool auxiliary = false, std::string const& prefix = "_x");

    /*!
     * Declares a variable with Boolean type whose name is guaranteed to be unique and not yet in use.
     *
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     * @param prefix The prefix which should be used.
     * @return The variable.
     */
    Variable declareFreshBooleanVariable(bool auxiliary = false, std::string const& prefix = "_x");

    /*!
     * Declares a variable with integer type whose name is guaranteed to be unique and not yet in use.
     *
     * @param auxiliary A flag indicating whether the new variable should be tagged as an auxiliary variable.
     * @param prefix The prefix which should be used.
     * @return The variable.
     */
    Variable declareFreshIntegerVariable(bool auxiliary = false, std::string const& prefix = "_x");

    /*!
     * Retrieves the number of variables.
     *
     * @return The number of variables.
     */
    uint_fast64_t getNumberOfVariables() const;

    /*!
     * Retrieves the number of boolean variables.
     *
     * @return The number of boolean variables.
     */
    uint_fast64_t getNumberOfBooleanVariables() const;

    /*!
     * Retrieves the number of integer variables.
     *
     * @return The number of integer variables.
     */
    uint_fast64_t getNumberOfIntegerVariables() const;

    /*!
     * Retrieves the number of bit vector variables.
     *
     * @return The number of bit vector variables.
     */
    uint_fast64_t getNumberOfBitVectorVariables() const;

    /*!
     * Retrieves the number of rational variables.
     *
     * @return The number of rational variables.
     */
    uint_fast64_t getNumberOfRationalVariables() const;

    /*!
     * Retrieves the number of array variables.
     */
    uint_fast64_t getNumberOfArrayVariables() const;

    /*!
     * Retrieves the number of string variables.
     */
    uint_fast64_t getNumberOfStringVariables() const;

    /*!
     * Retrieves the name of the variable with the given index.
     *
     * @param index The index of the variable whose name to retrieve.
     * @return The name of the variable.
     */
    std::string const& getVariableName(uint_fast64_t index) const;

    /*!
     * Retrieves the type of the variable with the given index.
     *
     * @param index The index of the variable whose name to retrieve.
     * @return The type of the variable.
     */
    Type const& getVariableType(uint_fast64_t index) const;

    /*!
     * Retrieves the offset of the variable with the given index within the group of equally typed variables.
     *
     * @param index The index of the variable.
     * @return The offset of the variable.
     */
    uint_fast64_t getOffset(uint_fast64_t index) const;

    /*!
     * Retrieves an iterator to all variables managed by this manager.
     *
     * @return An iterator to all variables managed by this manager.
     */
    const_iterator begin() const;

    /*!
     * Retrieves an iterator that points beyond the last variable managed by this manager.
     *
     * @return An iterator that points beyond the last variable managed by this manager.
     */
    const_iterator end() const;

    /*!
     * Retrieves a shared pointer to the expression manager.
     *
     * @return A shared pointer to the expression manager.
     */
    std::shared_ptr<ExpressionManager> getSharedPointer();

    /*!
     * Retrieves a shared pointer to the expression manager.
     *
     * @return A shared pointer to the expression manager.
     */
    std::shared_ptr<ExpressionManager const> getSharedPointer() const;

    friend std::ostream& operator<<(std::ostream& out, ExpressionManager const& manager);

   private:
    // Explicitly make copy construction/assignment private, since the manager is supposed to be stored as a pointer
    // of some sort. This is because the expression classes store a reference to the manager and it must
    // therefore be guaranteed that they do not become invalid, because the manager has been copied.
    ExpressionManager(ExpressionManager const& other) = default;
    ExpressionManager& operator=(ExpressionManager const& other) = default;

    /*!
     * Checks whether the given variable name is valid.
     *
     * @param name The name to check.
     * @return True iff the variable name is valid.
     */
    static bool isValidVariableName(std::string const& name);

    /*!
     * Retrieves whether a variable with the given name exists.
     *
     * @param name The name of the variable to check for.
     * @return True iff a variable with this name exists.
     */
    bool variableExists(std::string const& name) const;

    /*!
     * Declares a variable with the given name if it does not yet exist.
     *
     * @param name The name of the variable to declare.
     * @param variableType The type of the variable to declare.
     * @param auxiliary A flag indicating whether the variable is an auxiliary one.
     * @param checkName If set to true, the variable's name is checked that prevents internal variables from
     * being declared from the outside.
     * @return The variable.
     */
    Variable declareOrGetVariable(std::string const& name, storm::expressions::Type const& variableType, bool auxiliary, bool checkName);

    /*!
     * Retrieves the number of variables with the given type. Note that this considers bounded integer variables
     * to be of the same type, no matter which bit width they have.
     *
     * @param variableType The type for which to query the number of variables.
     */
    uint_fast64_t getNumberOfVariables(storm::expressions::Type const& variableType) const;

    // The set of all known variables.
    std::set<Variable> variableSet;

    // A mapping from all variable names (auxiliary + normal) to their indices.
    std::unordered_map<std::string, uint_fast64_t> nameToIndexMapping;

    // A mapping from all variable indices to their names.
    std::unordered_map<uint64_t, std::string> indexToNameMapping;

    // A mapping from all variable indices to their types.
    std::unordered_map<uint64_t, Type> indexToTypeMapping;

    // Store counts for variables.
    uint_fast64_t numberOfBooleanVariables;
    uint_fast64_t numberOfIntegerVariables;
    uint_fast64_t numberOfBitVectorVariables;
    uint_fast64_t numberOfRationalVariables;
    uint_fast64_t numberOfArrayVariables;
    uint_fast64_t numberOfStringVariables;

    // A counter used to create fresh variables.
    uint_fast64_t freshVariableCounter;

    // The types managed by this manager.
    mutable boost::optional<Type> booleanType;
    mutable boost::optional<Type> integerType;
    mutable std::unordered_set<Type> bitvectorTypes;
    mutable boost::optional<Type> rationalType;
    mutable std::unordered_set<Type> arrayTypes;
    mutable boost::optional<Type> transcendentalNumberType;
    mutable boost::optional<Type> stringType;

    // A mask that can be used to query whether a variable is an auxiliary variable.
    static const uint64_t auxiliaryMask = (1ull << 50);

    // A mask that can be used to project a variable index to its offset (with the group of equally typed variables).
    static const uint64_t offsetMask = (1ull << 50) - 1;
};

std::ostream& operator<<(std::ostream& out, ExpressionManager const& manager);
}  // namespace expressions
}  // namespace storm

#endif /* STORM_STORAGE_EXPRESSIONS_EXPRESSIONMANAGER_H_ */
