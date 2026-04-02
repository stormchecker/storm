#include "storm-pomdp-cli/settings/PomdpSettings.h"

#include "storm/settings/SettingsManager.h"

#include "storm/settings/modules/AbstractionSettings.h"
#include "storm/settings/modules/BisimulationSettings.h"
#include "storm/settings/modules/BuildSettings.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/CuddSettings.h"
#include "storm/settings/modules/DebugSettings.h"
#include "storm/settings/modules/EigenEquationSolverSettings.h"
#include "storm/settings/modules/EliminationSettings.h"
#include "storm/settings/modules/ExplorationSettings.h"
#include "storm/settings/modules/GameSolverSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/GlpkSettings.h"
#include "storm/settings/modules/GmmxxEquationSolverSettings.h"
#include "storm/settings/modules/GurobiSettings.h"
#include "storm/settings/modules/HintSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/MinMaxEquationSolverSettings.h"
#include "storm/settings/modules/ModelCheckerSettings.h"
#include "storm/settings/modules/MultiplierSettings.h"
#include "storm/settings/modules/NativeEquationSolverSettings.h"
#include "storm/settings/modules/OviSolverSettings.h"
#include "storm/settings/modules/ResourceSettings.h"
#include "storm/settings/modules/Smt2SmtSolverSettings.h"
#include "storm/settings/modules/SylvanSettings.h"
#include "storm/settings/modules/TopologicalEquationSolverSettings.h"
#include "storm/settings/modules/TransformationSettings.h"

#include "storm-pomdp-cli/settings/modules/BeliefExplorationSettings.h"
#include "storm-pomdp-cli/settings/modules/GenerateMonitorVerifierSettings.h"
#include "storm-pomdp-cli/settings/modules/POMDPSettings.h"
#include "storm-pomdp-cli/settings/modules/QualitativePOMDPAnalysisSettings.h"
#include "storm-pomdp-cli/settings/modules/ToParametricSettings.h"

namespace storm {
namespace settings {
void initializePomdpSettings(std::string const& name, std::string const& executableName) {
    storm::settings::mutableManager().setName(name, executableName);

    storm::settings::addModule<storm::settings::modules::GeneralSettings>();
    storm::settings::addModule<storm::settings::modules::IOSettings>();
    storm::settings::addModule<storm::settings::modules::CoreSettings>();
    storm::settings::addModule<storm::settings::modules::DebugSettings>();
    storm::settings::addModule<storm::settings::modules::BuildSettings>();
    storm::settings::addModule<storm::settings::modules::SylvanSettings>();

    storm::settings::addModule<storm::settings::modules::POMDPSettings>();
    storm::settings::addModule<storm::settings::modules::QualitativePOMDPAnalysisSettings>();
    storm::settings::addModule<storm::settings::modules::BeliefExplorationSettings>();
    storm::settings::addModule<storm::settings::modules::ToParametricSettings>();

    storm::settings::addModule<storm::settings::modules::TransformationSettings>();
    storm::settings::addModule<storm::settings::modules::GmmxxEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::EigenEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::NativeEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::EliminationSettings>();
    storm::settings::addModule<storm::settings::modules::MinMaxEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::GameSolverSettings>();
    storm::settings::addModule<storm::settings::modules::BisimulationSettings>();
    storm::settings::addModule<storm::settings::modules::GlpkSettings>();
    storm::settings::addModule<storm::settings::modules::GurobiSettings>();
    storm::settings::addModule<storm::settings::modules::ExplorationSettings>();
    storm::settings::addModule<storm::settings::modules::ResourceSettings>();
    storm::settings::addModule<storm::settings::modules::TopologicalEquationSolverSettings>();
    storm::settings::addModule<storm::settings::modules::ModelCheckerSettings>();
    storm::settings::addModule<storm::settings::modules::MultiplierSettings>();
    storm::settings::addModule<storm::settings::modules::HintSettings>();
    storm::settings::addModule<storm::settings::modules::OviSolverSettings>();
    storm::settings::addModule<storm::settings::modules::GenerateMonitorVerifierSettings>();
}
}  // namespace settings
}  // namespace storm
