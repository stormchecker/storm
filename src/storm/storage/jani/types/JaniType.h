#pragma once

#include <iosfwd>

#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace jani {
class BasicType;
class BoundedType;
class ArrayType;
class ClockType;
class ContinuousType;

class JaniType {
   public:
    JaniType();
    virtual ~JaniType() = default;
    virtual bool isBasicType() const;
    virtual bool isBoundedType() const;
    virtual bool isArrayType() const;
    virtual bool isClockType() const;
    virtual bool isContinuousType() const;

    BasicType const& asBasicType() const;
    BasicType& asBasicType();
    BoundedType const& asBoundedType() const;
    BoundedType& asBoundedType();
    ArrayType const& asArrayType() const;
    ArrayType& asArrayType();
    ClockType const& asClockType() const;
    ClockType& asClockType();
    ContinuousType const& asContinuousType() const;
    ContinuousType& asContinuousType();

    /*!
     * Retrieves a string representation of the type.
     *
     * @return A string representation of the type.
     */
    virtual std::string getStringRepresentation() const = 0;

    /*!
     * Retrieves a clone of this type
     */
    virtual std::unique_ptr<JaniType> clone() const = 0;

    /*!
     * Substitutes all variables in all expressions according to the given substitution.
     */
    virtual void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                            bool const substituteTranscendentalNumbers);

    friend std::ostream& operator<<(std::ostream& stream, JaniType const& type);
};
}  // namespace jani
}  // namespace storm
