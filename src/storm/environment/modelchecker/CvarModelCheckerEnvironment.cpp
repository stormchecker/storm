#include "storm/environment/modelchecker/CvarModelCheckerEnvironment.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CvarSettings.h"

namespace storm {

CvarModelCheckerEnvironment::CvarModelCheckerEnvironment() {
    auto const& cvarSettings = storm::settings::getModule<storm::settings::modules::CvarSettings>();
    method = cvarSettings.getCvarMethod();
    interpretationSelection = cvarSettings.getInterpretationSelection();
}

CvarModelCheckerEnvironment::~CvarModelCheckerEnvironment() {
    // Intentionally left empty
}

storm::modelchecker::cvar::CvarMethod const& CvarModelCheckerEnvironment::getMethod() const {
    return method;
}

void CvarModelCheckerEnvironment::setMethod(storm::modelchecker::cvar::CvarMethod value) {
    method = value;
}

storm::modelchecker::cvar::CvarInterpretationSelection const& CvarModelCheckerEnvironment::getInterpretationSelection() const {
    return interpretationSelection;
}

void CvarModelCheckerEnvironment::setInterpretationSelection(storm::modelchecker::cvar::CvarInterpretationSelection value) {
    interpretationSelection = value;
}

}  // namespace storm
