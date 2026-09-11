#include "gspn_transformation.h"

#include <memory>

#include "storm-conv/api/storm-conv.h"
#include "storm-dft/transformations/DftToGspnTransformator.h"
#include "storm-gspn/builder/JaniGSPNBuilder.h"

namespace storm::dft {
namespace api {

template<>
std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> transformToGSPN(storm::dft::storage::DFT<double> const& dft, bool disableDC, bool extendPriorities,
                                                                        bool smartTransformation, bool mergeDCFailed) {
    // Set Don't Care elements
    std::set<uint64_t> dontCareElements;
    if (!disableDC) {
        // Insert all elements as Don't Care elements
        for (std::size_t i = 0; i < dft.nrElements(); i++) {
            dontCareElements.insert(dft.getElement(i)->id());
        }
    }

    // Transform to GSPN
    storm::dft::transformations::DftToGspnTransformator<double> gspnTransformator(dft);
    auto priorities = gspnTransformator.computePriorities(extendPriorities);
    gspnTransformator.transform(priorities, dontCareElements, smartTransformation, mergeDCFailed, extendPriorities);
    std::shared_ptr<storm::gspn::GSPN> gspn(gspnTransformator.obtainGSPN());
    return std::make_pair(gspn, gspnTransformator.toplevelFailedPlaceId());
}

template<>
std::pair<std::shared_ptr<storm::gspn::GSPN>, uint64_t> transformToGSPN(storm::dft::storage::DFT<storm::RationalFunction> const& dft, bool disableDC,
                                                                        bool extendPriorities, bool smartTransformation, bool mergeDCFailed) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Transformation to GSPN not supported for this data type.");
}

std::pair<std::shared_ptr<storm::jani::Model>, std::vector<storm::jani::Property>> transformToJani(storm::gspn::GSPN const& gspn,
                                                                                                   uint64_t toplevelFailedPlace) {
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

    return std::make_pair(model, properties);
}

}  // namespace api
}  // namespace storm::dft
