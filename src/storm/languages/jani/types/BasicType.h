#pragma once

#include "JaniType.h"

namespace storm {
namespace jani {
class BasicType : public JaniType {
   public:
    enum class Type { Bool, Int, Real };

    BasicType(Type const& type);
    virtual ~BasicType() = default;
    virtual bool isBasicType() const override;

    Type const& get() const;
    bool isBooleanType() const;
    bool isIntegerType() const;
    bool isRealType() const;
    bool isNumericalType() const;  /// true if the type is either int or real

    virtual std::string getStringRepresentation() const override;
    virtual std::unique_ptr<JaniType> clone() const override;

   private:
    Type type;
};
}  // namespace jani
}  // namespace storm
