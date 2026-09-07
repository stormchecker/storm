#include "DftSettings.h"

#include "storm-conv/settings/modules/JaniExportSettings.h"
#include "storm-dft/settings/modules/DftGspnSettings.h"
#include "storm-dft/settings/modules/DftIOSettings.h"
#include "storm-dft/settings/modules/FaultTreeSettings.h"
#include "storm-gspn/settings/modules/GSPNExportSettings.h"
#include "storm-gspn/settings/modules/GSPNSettings.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/BisimulationSettings.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/CuddSettings.h"
#include "storm/settings/modules/DebugSettings.h"
#include "storm/settings/modules/EigenEquationSolverSettings.h"
#include "storm/settings/modules/EliminationSettings.h"
#include "storm/settings/modules/GameSolverSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/GmmxxEquationSolverSettings.h"
#include "storm/settings/modules/HintSettings.h"
#include "storm/settings/modules/IOSettings.h"
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

namespace storm::dft {
namespace settings {

void initializeDftSettings(std::string const& name, std::string const& executableName) {
    storm::settings::mutableManager().setName(name, executableName);

    // Register relevant settings modules.
    storm::settings::addModule<storm::settings::modules::GeneralSettings>();
    storm::settings::addModule<storm::dft::settings::modules::DftIOSettings>();
    storm::settings::addModule<storm::dft::settings::modules::FaultTreeSettings>();
    storm::settings::addModule<storm::dft::settings::modules::DftGspnSettings>();
    storm::settings::addModule<storm::settings::modules::IOSettings>();
    storm::settings::addModule<storm::settings::modules::CoreSettings>();
    storm::settings::addModule<storm::settings::modules::TransformationSettings>();
    storm::settings::addModule<storm::settings::modules::HintSettings>();

    storm::settings::addModule<storm::settings::modules::DebugSettings>();
    storm::settings::addModule<storm::settings::modules::ModelCheckerSettings>();
    storm::settings::addModule<storm::settings::modules::GmmxxEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::EigenEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::NativeEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::TopologicalEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::EliminationSettings>();
    storm::settings::addModule<storm::settings::modules::MinMaxEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::MultiplierSettings>();
    storm::settings::addModule<storm::settings::modules::OviSolverSettings>();
    storm::settings::addModule<storm::settings::modules::TimeBoundedSolverSettings>();
    storm::settings::addModule<storm::settings::modules::GameSolverSettings>(false);
    // storm::settings::addModule<storm::settings::modules::BisimulationSettings>();
    storm::settings::addModule<storm::settings::modules::ResourceSettings>();

    // For translation into JANI via GSPN.
    storm::settings::addModule<storm::settings::modules::JaniExportSettings>();
    storm::settings::addModule<storm::settings::modules::GSPNSettings>();
    storm::settings::addModule<storm::settings::modules::GSPNExportSettings>();

    // For Decision Diagrams
    storm::settings::addModule<storm::settings::modules::CuddSettings>();
    storm::settings::addModule<storm::settings::modules::SylvanSettings>();
}

}  // namespace settings
}  // namespace storm::dft
