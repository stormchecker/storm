#include "gspn_transformation.h"

#include <memory>

#include "storm-conv/api/storm-conv.h"
#include "storm-conv/settings/modules/JaniExportSettings.h"
#include "storm-dft/settings/modules/DftGspnSettings.h"
#include "storm-dft/settings/modules/FaultTreeSettings.h"
#include "storm-dft/transformations/DftToGspnTransformator.h"
#include "storm-gspn/builder/JaniGSPNBuilder.h"
#include "storm/settings/SettingsManager.h"

namespace storm::dft {
namespace api {

template<>
std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> transformToGSPN(storm::dft::storage::DFT<double> const& dft) {
    storm::dft::settings::modules::FaultTreeSettings const& ftSettings = storm::settings::getModule<storm::dft::settings::modules::FaultTreeSettings>();
    storm::dft::settings::modules::DftGspnSettings const& dftGspnSettings = storm::settings::getModule<storm::dft::settings::modules::DftGspnSettings>();

    // Set Don't Care elements
    std::set<uint64_t> dontCareElements;
    if (!ftSettings.isDisableDC()) {
        // Insert all elements as Don't Care elements
        for (std::size_t i = 0; i < dft.nrElements(); i++) {
            dontCareElements.insert(dft.getElement(i)->id());
        }
    }

    // Transform to GSPN
    storm::dft::transformations::DftToGspnTransformator<double> gspnTransformator(dft);
    auto priorities = gspnTransformator.computePriorities(dftGspnSettings.isExtendPriorities());
    gspnTransformator.transform(priorities, dontCareElements, !dftGspnSettings.isDisableSmartTransformation(), dftGspnSettings.isMergeDCFailed(),
                                dftGspnSettings.isExtendPriorities());
    std::shared_ptr<storm::gspn::GSPN> gspn(gspnTransformator.obtainGSPN());
    return std::make_pair(gspn, gspnTransformator.toplevelFailedPlaceId());
}

template<>
std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> transformToGSPN(storm::dft::storage::DFT<storm::RationalFunction> const& dft) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Transformation to GSPN not supported for this data type.");
}

std::shared_ptr<storm::jani::Model> transformToJani(storm::gspn::GSPN const& gspn, uint64_t toplevelFailedPlace) {
    // Build Jani model
    storm::builder::JaniGSPNBuilder builder(gspn);
    std::shared_ptr<storm::jani::Model> model(builder.build("dft_gspn"));

    // Build properties
    std::shared_ptr<storm::expressions::ExpressionManager> const& exprManager = gspn.getExpressionManager();
    storm::jani::Variable const& topfailedVar = builder.getPlaceVariable(toplevelFailedPlace);
    storm::expressions::Expression targetExpression = exprManager->integer(1) == topfailedVar.getExpressionVariable().getExpression();
    // Add variable for easier access to 'failed' state
    builder.addTransientVariable(model.get(), "failed", targetExpression);
    auto failedFormula = std::make_shared<storm::logic::AtomicExpressionFormula>(targetExpression);
    auto properties = builder.getStandardProperties(model.get(), failedFormula, "Failed", "a failed state", true);

    // Export Jani to file
    storm::dft::settings::modules::DftGspnSettings const& dftGspnSettings = storm::settings::getModule<storm::dft::settings::modules::DftGspnSettings>();
    if (dftGspnSettings.isWriteToJaniSet()) {
        auto const& jani = storm::settings::getModule<storm::settings::modules::JaniExportSettings>();
        storm::api::exportJaniToFile(*model, properties, dftGspnSettings.getWriteToJaniFilename(), jani.isCompactJsonSet());
    }

    return model;
}

}  // namespace api
}  // namespace storm::dft
