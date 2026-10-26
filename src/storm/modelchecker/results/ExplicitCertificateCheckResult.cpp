#include "storm/modelchecker/results/ExplicitCertificateCheckResult.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/modelchecker/certificates/Certificate.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/utility/macros.h"

namespace storm::modelchecker {

template<typename ValueType>
ExplicitCertificateCheckResult<ValueType>::ExplicitCertificateCheckResult(std::unique_ptr<Certificate<ValueType>>&& certificate,
                                                                          storm::storage::BitVector relevantStates)
    : certificate(std::move(certificate)), relevantStates(std::move(relevantStates)) {
    // Intentionally left empty
}

template<typename ValueType>
bool ExplicitCertificateCheckResult<ValueType>::isExplicit() const {
    return true;
}

template<typename ValueType>
bool ExplicitCertificateCheckResult<ValueType>::isExplicitCertificateCheckResult() const {
    return true;
}

template<typename ValueType>
bool ExplicitCertificateCheckResult<ValueType>::isResultForAllStates() const {
    return relevantStates.full();
}

template<typename ValueType>
std::unique_ptr<CheckResult> ExplicitCertificateCheckResult<ValueType>::clone() const {
    return std::make_unique<ExplicitCertificateCheckResult<ValueType>>(certificate->clone(), relevantStates);
}

template<typename ValueType>
void ExplicitCertificateCheckResult<ValueType>::filter(QualitativeCheckResult const& filter) {
    STORM_LOG_THROW(filter.isExplicitQualitativeCheckResult(), storm::exceptions::InvalidOperationException,
                    "Cannot filter explicit check result with non-explicit filter.");
    STORM_LOG_THROW(filter.isResultForAllStates(), storm::exceptions::InvalidOperationException, "Cannot filter check result with non-complete filter.");
    STORM_LOG_THROW(filter.hasValueType<ValueType>(), storm::exceptions::InvalidOperationException, "Filter has unexpected value type.");
    relevantStates = filter.template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
}

template<typename ValueType>
std::ostream& ExplicitCertificateCheckResult<ValueType>::writeToStream(std::ostream& out) const {
    out << certificate->summaryString(relevantStates);
    return out;
}

template<typename ValueType>
Certificate<ValueType> const& ExplicitCertificateCheckResult<ValueType>::getCertificate() const {
    return *certificate;
}

template<typename ValueType>
Certificate<ValueType>& ExplicitCertificateCheckResult<ValueType>::getCertificate() {
    return *certificate;
}

template<typename ValueType>
bool ExplicitCertificateCheckResult<ValueType>::checkValidity(storm::models::Model<ValueType> const& model) {
    return certificate->checkValidity(model);
}

template class ExplicitCertificateCheckResult<double>;
template class ExplicitCertificateCheckResult<storm::RationalNumber>;

}  // namespace storm::modelchecker