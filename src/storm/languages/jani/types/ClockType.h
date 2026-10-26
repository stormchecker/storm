#pragma once

#include "JaniType.h"

namespace storm {
namespace jani {
class ClockType : public JaniType {
   public:
    ClockType();
    virtual ~ClockType() = default;
    virtual bool isClockType() const override;
    virtual std::string getStringRepresentation() const override;
    virtual std::unique_ptr<JaniType> clone() const override;

   private:
};
}  // namespace jani
}  // namespace storm
