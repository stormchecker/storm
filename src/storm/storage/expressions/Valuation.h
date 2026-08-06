#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace storm {
namespace expressions {
class ExpressionManager;
class Variable;

/*!
 * The base class of all valuations of variables. This is, for example, used for evaluating expressions.
 */
class Valuation {
   public:
    /*!
     * Creates a valuation of all non-auxiliary variables managed by the given manager. If the manager is
     * modified in the sense that additional variables are added, all valuations over its variables are
     * invalidated.
     *
     * @param manager The manager of the variables.
     */
    Valuation(std::shared_ptr<ExpressionManager const> const& manager);

    /*!
     * Declare virtual destructor, so we can properly delete instances later.
     */
    virtual ~Valuation();

    /*!
     * Retrieves the value of the given boolean variable.
     *
     * @param booleanVariable The boolean variable whose value to retrieve.
     * @return The value of the boolean variable.
     */
    virtual bool getBooleanValue(Variable const& booleanVariable) const = 0;

    /*!
     * Sets the value of the given boolean variable to the provided value.
     *
     * @param booleanVariable The variable whose value to set.
     * @param value The new value of the variable.
     */
    virtual void setBooleanValue(Variable const& booleanVariable, bool value) = 0;

    /*!
     * Retrieves the value of the given integer variable.
     *
     * @param integerVariable The integer variable whose value to retrieve.
     * @return The value of the integer variable.
     */
    virtual int_fast64_t getIntegerValue(Variable const& integerVariable) const = 0;

    /*!
     * Retrieves the value of the given bit vector variable.
     *
     * @param bitVectorVariable The bit vector variable whose value to retrieve.
     * @return The value of the bit vector variable.
     */
    virtual int_fast64_t getBitVectorValue(Variable const& bitVectorVariable) const = 0;

    /*!
     * Sets the value of the given integer variable to the provided value.
     *
     * @param integerVariable The variable whose value to set.
     * @param value The new value of the variable.
     */
    virtual void setIntegerValue(Variable const& integerVariable, int_fast64_t value) = 0;

    /*!
     * Sets the value of the given bit vector variable to the provided value.
     *
     * @param bitVectorVariable The variable whose value to set.
     * @param value The new value of the variable.
     */
    virtual void setBitVectorValue(Variable const& bitVectorVariable, int_fast64_t value) = 0;

    /*!
     * Retrieves the value of the given rational variable.
     *
     * @param rationalVariable The rational variable whose value to retrieve.
     * @return The value of the rational variable.
     */
    virtual double getRationalValue(Variable const& rationalVariable) const = 0;

    /*!
     * Sets the value of the given boolean variable to the provided value.
     *
     * @param integerVariable The variable whose value to set.
     * @param value The new value of the variable.
     */
    virtual void setRationalValue(Variable const& rationalVariable, double value) = 0;

    /*!
     * Retrieves the manager responsible for the variables of this valuation.
     *
     * @return The manager.
     */
    ExpressionManager const& getManager() const;

   protected:
    /*!
     * Retrieves the manager responsible for the variables of this valuation.
     *
     * @return The manager.
     */
    std::shared_ptr<ExpressionManager const> const& getManagerAsSharedPtr() const;

    /*!
     * Sets the manager responsible for the variables in this valuation.
     *
     * @param manager The manager to set.
     */
    void setManager(std::shared_ptr<ExpressionManager const> const& manager);

   private:
    // The manager responsible for the variables of this valuation.
    std::shared_ptr<ExpressionManager const> manager;
};
}  // namespace expressions
}  // namespace storm
