#include "transformation.h"

#include <memory>

#include "storm-conv/api/storm-conv.h"
#include "storm-conv/settings/modules/JaniExportSettings.h"
#include "storm-dft/settings/modules/DftGspnSettings.h"
#include "storm-dft/settings/modules/FaultTreeSettings.h"
#include "storm-dft/storage/DFT.h"
#include "storm-dft/transformations/DftToGspnTransformator.h"
#include "storm-dft/transformations/DftTransformer.h"
#include "storm-dft/utility/DftValidator.h"
#include "storm-gspn/builder/JaniGSPNBuilder.h"
#include "storm/settings/SettingsManager.h"

namespace storm::dft {
namespace api {

template<typename ValueType>
std::pair<bool, std::string> isWellFormed(storm::dft::storage::DFT<ValueType> const& dft, bool validForMarkovianAnalysis) {
    std::stringstream stream;
    bool wellFormed = false;
    if (validForMarkovianAnalysis) {
        wellFormed = storm::dft::utility::DftValidator<ValueType>::isDftValidForMarkovianAnalysis(dft, stream);
    } else {
        wellFormed = storm::dft::utility::DftValidator<ValueType>::isDftWellFormed(dft, stream);
    }
    return std::pair<bool, std::string>(wellFormed, stream.str());
}

template<typename ValueType>
std::pair<bool, std::string> hasPotentialModelingIssues(storm::dft::storage::DFT<ValueType> const& dft) {
    std::stringstream stream;
    bool modelingIssues = storm::dft::utility::DftValidator<ValueType>::hasPotentialModelingIssues(dft, stream);
    return std::pair<bool, std::string>(modelingIssues, stream.str());
}

template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> applyTransformations(storm::dft::storage::DFT<ValueType> const& dft, bool uniqueBE, bool binaryFDEP,
                                                                          bool exponentialDistributions) {
    std::shared_ptr<storm::dft::storage::DFT<ValueType>> transformedDft = std::make_shared<storm::dft::storage::DFT<ValueType>>(dft);
    if (exponentialDistributions && !storm::dft::transformations::DftTransformer<ValueType>::hasOnlyExponentialDistributions(*transformedDft)) {
        transformedDft = storm::dft::transformations::DftTransformer<ValueType>::transformExponentialDistributions(*transformedDft);
    }
    if (uniqueBE && !storm::dft::transformations::DftTransformer<ValueType>::hasUniqueFailedBE(*transformedDft)) {
        transformedDft = storm::dft::transformations::DftTransformer<ValueType>::transformUniqueFailedBE(*transformedDft);
    }
    if (binaryFDEP && storm::dft::transformations::DftTransformer<ValueType>::hasNonBinaryDependency(*transformedDft)) {
        transformedDft = storm::dft::transformations::DftTransformer<ValueType>::transformBinaryDependencies(*transformedDft);
    }
    return transformedDft;
}

template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> prepareForMarkovAnalysis(storm::dft::storage::DFT<ValueType> const& dft) {
    return storm::dft::api::applyTransformations(dft, true, true, true);
}

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

// Explicitly instantiate methods
template std::pair<bool, std::string> isWellFormed(storm::dft::storage::DFT<double> const&, bool);
template std::pair<bool, std::string> hasPotentialModelingIssues(storm::dft::storage::DFT<double> const&);
template std::shared_ptr<storm::dft::storage::DFT<double>> applyTransformations(storm::dft::storage::DFT<double> const&, bool, bool, bool);
template std::shared_ptr<storm::dft::storage::DFT<double>> prepareForMarkovAnalysis(storm::dft::storage::DFT<double> const&);

template std::pair<bool, std::string> isWellFormed(storm::dft::storage::DFT<storm::RationalFunction> const&, bool);
template std::pair<bool, std::string> hasPotentialModelingIssues(storm::dft::storage::DFT<storm::RationalFunction> const&);
template std::shared_ptr<storm::dft::storage::DFT<storm::RationalFunction>> applyTransformations(storm::dft::storage::DFT<storm::RationalFunction> const&, bool,
                                                                                                 bool, bool);
template std::shared_ptr<storm::dft::storage::DFT<storm::RationalFunction>> prepareForMarkovAnalysis(storm::dft::storage::DFT<storm::RationalFunction> const&);

}  // namespace api
}  // namespace storm::dft
