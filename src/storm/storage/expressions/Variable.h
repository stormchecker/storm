#ifndef STORM_STORAGE_EXPRESSIONS_VARIABLE_H_
#define STORM_STORAGE_EXPRESSIONS_VARIABLE_H_

#include <cstdint>
#include <functional>
#include <memory>

#include "storm/utility/OsDetection.h"
#include "storm/utility/macros.h"

namespace storm {
namespace expressions {
class ExpressionManager;
class Expression;
class Type;

// This class captures a simple variable.
class Variable {
   public:
    Variable();
    ~Variable();

    /*!
     * Constructs a variable with the given index and type.
     *
     * @param manager The manager that is responsible for this variable.
     * @param index The (unique) index of the variable.
     */
    Variable(std::shared_ptr<ExpressionManager const> const& manager, uint_fast64_t index);

    // Default-instantiate some copy/move construction/assignment.
    Variable(Variable const& other) = default;
    Variable& operator=(Variable const& other) = default;
    Variable(Variable&& other) = default;
    Variable& operator=(Variable&& other) = default;

    /*!
     * Checks the two variables for equality.
     *
     * @param other The variable to compare with.
     * @return True iff the two variables are the same.
     */
    bool operator==(Variable const& other) const;

    /*!
     * Checks the two variables for inequality.
     *
     * @param other The variable to compare with.
     * @return True iff the two variables are different.
     */
    bool operator!=(Variable const& other) const;

    /*!
     * Checks whether the variable appears earlier in the total ordering of variables.
     *
     * @param other The variable to compare with.
     * @return True iff the first variable appears earlier than the given one.
     */
    bool operator<(Variable const& other) const;

    /*!
     * Retrieves the name of the variable.
     *
     * @return name The name of the variable.
     */
    std::string const& getName() const;

    /*!
     * Retrieves the type of the variable.
     *
     * @return The type of the variable.
     */
    Type const& getType() const;

    /*!
     * Retrieves an expression that represents the variable.
     *
     * @return An expression that represents the varible.
     */
    storm::expressions::Expression getExpression() const;

    /*!
     * Retrieves the manager responsible for this variable.
     *
     * @return The manager responsible for this variable.
     */
    ExpressionManager const& getManager() const;

    /*!
     * Retrieves the index of the variable.
     *
     * @return The index of the variable.
     */
    uint_fast64_t getIndex() const;

    /*!
     * Retrieves the offset of the variable in the group of all equally typed variables.
     *
     * @return The offset of the variable.
     */
    uint_fast64_t getOffset() const;

    /*!
     * Checks whether the variable is of boolean type.
     *
     * @return True iff the variable if of boolean type.
     */
    bool hasBooleanType() const;

    /*!
     * Checks whether the variable is of integral type.
     *
     * @return True iff the variable if of integral type.
     */
    bool hasIntegerType() const;

    /*!
     * Checks whether the variable is of a bit vector type.
     *
     * @return True iff the variable is of a bit vector type.
     */
    bool hasBitVectorType() const;

    /*!
     * Checks whether the variable is of rational type.
     *
     * @return True iff the variable is of rational type.
     */
    bool hasRationalType() const;

    /*!
     * Checks whether the variable is of numerical type.
     *
     * @return True iff the variable is of numerical type.
     */
    bool hasNumericalType() const;

    /*!
     * Checks whether the variable is of string type.
     *
     * @return True iff the variable is of string type.
     */
    bool hasStringType() const;

   private:
    // The manager that is responsible for this variable.
    ExpressionManager const* manager;

    // The index of the variable.
    uint_fast64_t index;
};
}  // namespace expressions
}  // namespace storm

namespace std {
// Provide a hashing operator, so we can put variables in unordered collections.
template<>
struct hash<storm::expressions::Variable> {
    std::size_t operator()(storm::expressions::Variable const& variable) const {
        return std::hash<uint_fast64_t>()(variable.getIndex());
    }
};
}  // namespace std

#endif /* STORM_STORAGE_EXPRESSIONS_VARIABLE_H_ */
