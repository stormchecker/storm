#pragma once

#include "storm/modelchecker/cvar/CvarMethod.h"

namespace storm {

class CvarModelCheckerEnvironment {
   public:
    CvarModelCheckerEnvironment();
    ~CvarModelCheckerEnvironment();

    storm::modelchecker::cvar::CvarMethod const& getMethod() const;
    void setMethod(storm::modelchecker::cvar::CvarMethod value);

   private:
    storm::modelchecker::cvar::CvarMethod method;
};

}  // namespace storm
