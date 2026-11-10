#include "storm-cli-utilities/SettingsUtils.h"

#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/EliminationSettings.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace cli {

bool preferEliminationChecker() {
    // Consider moving this to the environments once they stabilized.
    auto const& coreSettings = storm::settings::getModule<storm::settings::modules::CoreSettings>();
    auto const& eliminationSettings = storm::settings::getModule<storm::settings::modules::EliminationSettings>();
    return coreSettings.getEquationSolver() == storm::solver::EquationSolverType::Elimination && eliminationSettings.isUseDedicatedModelCheckerSet();
}

}  // namespace cli
}  // namespace storm