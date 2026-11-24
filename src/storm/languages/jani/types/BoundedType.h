#pragma once

#include "JaniType.h"

namespace storm {
namespace jani {
class BoundedType : public JaniType {
   public:
    enum class BaseType { Int, Real };

    BoundedType(BaseType const& type, storm::expressions::Expression const& lowerBound, storm::expressions::Expression const& upperBound);
    virtual ~BoundedType() = default;

    virtual bool isBoundedType() const override;

    BaseType const& getBaseType() const;
    bool isIntegerType() const;
    bool isRealType() const;
    bool isNumericalType() const;  /// true iff type is either real or int (i.e. it's  always true but let's make it explicit)

    virtual std::string getStringRepresentation() const override;
    virtual void substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution,
                            bool const substituteTranscendentalNumbers) override;
    virtual std::unique_ptr<JaniType> clone() const override;

    void setLowerBound(storm::expressions::Expression const& expression);
    void setUpperBound(storm::expressions::Expression const& expression);
    bool hasLowerBound() const;
    bool hasUpperBound() const;

    storm::expressions::Expression& getLowerBound();
    storm::expressions::Expression& getUpperBound();
    storm::expressions::Expression const& getLowerBound() const;
    storm::expressions::Expression const& getUpperBound() const;

   private:
    BaseType type;
    storm::expressions::Expression lowerBound;
    storm::expressions::Expression upperBound;
};
}  // namespace jani
}  // namespace storm
