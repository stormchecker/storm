#include "storm-pars/settings/ParsSettings.h"

#include "storm-pars/settings/modules/DerivativeSettings.h"
#include "storm-pars/settings/modules/FeasibilitySettings.h"
#include "storm-pars/settings/modules/MonotonicitySettings.h"
#include "storm-pars/settings/modules/ParametricSettings.h"
#include "storm-pars/settings/modules/PartitionSettings.h"
#include "storm-pars/settings/modules/RegionSettings.h"
#include "storm-pars/settings/modules/RegionVerificationSettings.h"
#include "storm-pars/settings/modules/SamplingSettings.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/BisimulationSettings.h"
#include "storm/settings/modules/BuildSettings.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/DebugSettings.h"
#include "storm/settings/modules/EigenEquationSolverSettings.h"
#include "storm/settings/modules/EliminationSettings.h"
#include "storm/settings/modules/GameSolverSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/GmmxxEquationSolverSettings.h"
#include "storm/settings/modules/HintSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/LongRunAverageSolverSettings.h"
#include "storm/settings/modules/MinMaxEquationSolverSettings.h"
#include "storm/settings/modules/ModelCheckerSettings.h"
#include "storm/settings/modules/MultiplierSettings.h"
#include "storm/settings/modules/NativeEquationSolverSettings.h"
#include "storm/settings/modules/OviSolverSettings.h"
#include "storm/settings/modules/ResourceSettings.h"
#include "storm/settings/modules/SylvanSettings.h"
#include "storm/settings/modules/TimeBoundedSolverSettings.h"
#include "storm/settings/modules/TopologicalEquationSolverSettings.h"
#include "storm/settings/modules/TransformationSettings.h"

namespace storm {
namespace settings {
void initializeParsSettings(std::string const& name, std::string const& executableName) {
    storm::settings::mutableManager().setName(name, executableName);

    // Register relevant settings modules.
    storm::settings::addModule<storm::settings::modules::GeneralSettings>();
    storm::settings::addModule<storm::settings::modules::IOSettings>();
    storm::settings::addModule<storm::settings::modules::CoreSettings>();
    storm::settings::addModule<storm::settings::modules::ParametricSettings>();
    storm::settings::addModule<storm::settings::modules::MonotonicitySettings>();
    storm::settings::addModule<storm::settings::modules::DerivativeSettings>();
    storm::settings::addModule<storm::settings::modules::FeasibilitySettings>();
    storm::settings::addModule<storm::settings::modules::PartitionSettings>();
    storm::settings::addModule<storm::settings::modules::RegionVerificationSettings>();
    storm::settings::addModule<storm::settings::modules::SamplingSettings>();
    storm::settings::addModule<storm::settings::modules::RegionSettings>();
    storm::settings::addModule<storm::settings::modules::BuildSettings>();
    storm::settings::addModule<storm::settings::modules::ModelCheckerSettings>();
    storm::settings::addModule<storm::settings::modules::DebugSettings>();
    storm::settings::addModule<storm::settings::modules::SylvanSettings>();
    storm::settings::addModule<storm::settings::modules::GmmxxEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::EigenEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::NativeEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::TopologicalEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::EliminationSettings>();
    storm::settings::addModule<storm::settings::modules::LongRunAverageSolverSettings>();
    storm::settings::addModule<storm::settings::modules::MinMaxEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::GameSolverSettings>();
    storm::settings::addModule<storm::settings::modules::BisimulationSettings>();
    storm::settings::addModule<storm::settings::modules::ResourceSettings>();
    storm::settings::addModule<storm::settings::modules::MultiplierSettings>();
    storm::settings::addModule<storm::settings::modules::TransformationSettings>();
    storm::settings::addModule<storm::settings::modules::OviSolverSettings>();
    storm::settings::addModule<storm::settings::modules::TimeBoundedSolverSettings>();

    storm::settings::addModule<storm::settings::modules::HintSettings>();
}

}  // namespace settings
}  // namespace storm
