#pragma once

#include "JaniType.h"

namespace storm {
namespace jani {
class ArrayType : public JaniType {
   public:
    ArrayType(JaniType const& baseType);
    ArrayType(std::unique_ptr<JaniType>&& baseType);
    virtual ~ArrayType() = default;

    bool isArrayType() const override;

    /*!
     * @return the type of which this is an array, e.g., the baseType of int[][] is int[].
     */
    JaniType& getBaseType();
    JaniType const& getBaseType() const;

    /*!
     * @return the most basic base type that is not an array, e.g., for int[][] it is int.
     */
    JaniType const& getBaseTypeRecursive() const;

    /*!
     * @return the nesting degree, e.g., int[] has degree 1, int[][] has degree 2, ...
     */
    uint64_t getNestingDegree() const;

    virtual std::string getStringRepresentation() const override;
    virtual void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                            bool const substituteTranscendentalNumbers) override;
    virtual std::unique_ptr<JaniType> clone() const override;

   private:
    std::unique_ptr<JaniType> baseType;
};
}  // namespace jani
}  // namespace storm
