#include "transformation.h"

#include <memory>

#include "storm-dft/transformations/DftTransformer.h"
#include "storm-dft/utility/DftValidator.h"

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
