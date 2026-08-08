#pragma once

#include "storm/modelchecker/cvar/CvarInterpretation.h"
#include "storm/modelchecker/cvar/CvarMethod.h"

namespace storm {

class CvarModelCheckerEnvironment {
   public:
    CvarModelCheckerEnvironment();
    ~CvarModelCheckerEnvironment();

    storm::modelchecker::cvar::CvarMethod const& getMethod() const;
    void setMethod(storm::modelchecker::cvar::CvarMethod value);
    storm::modelchecker::cvar::CvarInterpretationSelection const& getInterpretationSelection() const;
    void setInterpretationSelection(storm::modelchecker::cvar::CvarInterpretationSelection value);

   private:
    storm::modelchecker::cvar::CvarMethod method;
    storm::modelchecker::cvar::CvarInterpretationSelection interpretationSelection;
};

}  // namespace storm
