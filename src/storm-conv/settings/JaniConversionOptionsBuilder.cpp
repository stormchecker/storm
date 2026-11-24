#include "storm-conv/settings/JaniConversionOptionsBuilder.h"

#include "storm-conv/settings/modules/JaniExportSettings.h"
#include "storm/settings/SettingsManager.h"

namespace storm::settings {

storm::converter::JaniConversionOptions JaniConversionOptionsBuilder::buildJaniConversionOptions() {
    auto const& settings = storm::settings::getModule<storm::settings::modules::JaniExportSettings>();

    storm::converter::JaniConversionOptions options;
    options.locationVariables = settings.getLocationVariables();
    options.edgeAssignments = settings.isAllowEdgeAssignmentsSet();
    options.flatten = settings.isExportFlattenedSet();
    options.localVars = settings.isLocalVarsSet();
    options.globalVars = settings.isGlobalVarsSet();
    options.replaceUnassignedVariablesWithConstants = settings.isReplaceUnassignedVariablesWithConstantsSet();
    options.simplifyComposition = settings.isSimplifyCompositionSet();
    options.locationElimination = settings.isLocationEliminationSet();
    options.locationEliminationLocationHeuristic = settings.getLocationEliminationLocationHeuristic();
    options.locationEliminationEdgeHeuristic = settings.getLocationEliminationEdgesHeuristic();
    if (settings.isEliminateFunctionsSet()) {
        options.allowedModelFeatures.remove(storm::jani::ModelFeature::Functions);
    }
    if (settings.isEliminateArraysSet()) {
        options.allowedModelFeatures.remove(storm::jani::ModelFeature::Arrays);
    }
    return options;
}

}  // namespace storm::settings
