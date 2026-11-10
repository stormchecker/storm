#pragma once

#include "JaniType.h"

namespace storm {
namespace jani {
class ContinuousType : public JaniType {
   public:
    ContinuousType();
    virtual ~ContinuousType() = default;

    virtual bool isContinuousType() const override;
    virtual std::string getStringRepresentation() const override;
    virtual std::unique_ptr<JaniType> clone() const override;
};
}  // namespace jani
}  // namespace storm
