#pragma once

#include <cstdint>
#include <set>
#include <vector>

#include "storm/adapters/JsonForward.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/expressions/Valuation.h"

namespace storm {
namespace expressions {

/*!
 * A simple implementation of the valuation interface.
 */
class SimpleValuation : public Valuation {
   public:
    friend class SimpleValuationPointerHash;
    friend class SimpleValuationPointerLess;

    /*!
     * Creates an empty simple valuation that is associated to no manager and has no variables.
     */
    SimpleValuation();

    /*!
     * Creates a new valuation over the non-auxiliary variables of the given manager.
     *
     * @param manager The manager responsible for the variables of this valuation.
     */
    SimpleValuation(std::shared_ptr<storm::expressions::ExpressionManager const> const& manager);

    // Define deep-copy and move operators.
    SimpleValuation(SimpleValuation const& other);
    SimpleValuation& operator=(SimpleValuation const& other);
    SimpleValuation(SimpleValuation&& other);
    SimpleValuation& operator=(SimpleValuation&& other);

    /*!
     * Checks whether the two valuations are semantically equivalent.
     *
     * @param other The valuation with which to compare.
     * @return True iff the two valuations are semantically equivalent.
     */
    bool operator==(SimpleValuation const& other) const;

    /*!
     * Checks whether this valuation stores a value for the given variable.
     *
     * The contained variables are those the manager knew at the time this valuation was created. Variables
     * that are declared in the manager afterwards are not contained, as valuations are invalidated by such
     * declarations. Note that array and string variables are never contained, as they are not stored by
     * valuations of this type.
     *
     * @param variable The variable to check.
     * @return True iff this valuation stores a value for the given variable.
     */
    bool contains(Variable const& variable) const;

    // Override virtual functions of base class.
    virtual bool getBooleanValue(Variable const& booleanVariable) const override;
    virtual void setBooleanValue(Variable const& booleanVariable, bool value) override;
    virtual int_fast64_t getIntegerValue(Variable const& integerVariable) const override;
    virtual int_fast64_t getBitVectorValue(Variable const& bitVectorVariable) const override;
    virtual void setIntegerValue(Variable const& integerVariable, int_fast64_t value) override;
    virtual void setBitVectorValue(Variable const& bitVectorVariable, int_fast64_t value) override;
    virtual double getRationalValue(Variable const& rationalVariable) const override;
    virtual void setRationalValue(Variable const& rationalVariable, double value) override;

    /*!
     * Returns a string representation of the valuation of the selected variables.
     *
     * @param selectedVariables The variables to select.
     * @return The string representation.
     */
    virtual std::string toPrettyString(std::set<storm::expressions::Variable> const& selectedVariables) const;

    virtual std::string toString(bool pretty = true) const;
    storm::json<storm::RationalNumber> toJson() const;

    friend std::ostream& operator<<(std::ostream& out, SimpleValuation const& valuation);

   private:
    // Containers that store the values of the variables of the appropriate type.
    std::vector<bool> booleanValues;
    std::vector<int_fast64_t> integerValues;
    std::vector<double> rationalValues;
};

std::ostream& operator<<(std::ostream& out, SimpleValuation const& valuation);

/*!
 * A helper class that can pe used as the hash functor for data structures that need to hash valuations given
 * via pointers.
 */
class SimpleValuationPointerHash {
   public:
    std::size_t operator()(SimpleValuation* valuation) const;
};

/*!
 * A helper class that can be used as the comparison functor wrt. equality for data structures that need to
 * store pointers to valuations and need to compare the elements wrt. their content (rather than pointer
 * equality).
 */
class SimpleValuationPointerCompare {
   public:
    bool operator()(SimpleValuation* valuation1, SimpleValuation* valuation2) const;
};

/*!
 * A helper class that can be used as the comparison functor wrt. "<" for data structures that need to
 * store pointers to valuations and need to compare the elements wrt. their content (rather than pointer
 * equality).
 */
class SimpleValuationPointerLess {
   public:
    bool operator()(SimpleValuation* valuation1, SimpleValuation* valuation2) const;
};
}  // namespace expressions
}  // namespace storm
