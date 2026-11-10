#include "storm/modelchecker/certificates/CertificateAlgorithm.h"

#include "storm/exceptions/UnexpectedException.h"
#include "storm/utility/macros.h"

namespace storm {

std::ostream& operator<<(std::ostream& stream, CertificateAlgorithm const& algorithm) {
    switch (algorithm) {
        case CertificateAlgorithm::FpII:
            return stream << "fp-ii";
        case CertificateAlgorithm::FpSmoothII:
            return stream << "fp-smoothii";
        case CertificateAlgorithm::FpRoundII:
            return stream << "fp-roundii";
        case CertificateAlgorithm::ExPI:
            return stream << "ex-pi";
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unknown certificate algorithm.");
    return stream;
}

CertificateAlgorithm certificateAlgorithmFromString(std::string const& algorithm) {
    if (algorithm == "fp-ii") {
        return CertificateAlgorithm::FpII;
    } else if (algorithm == "fp-smoothii") {
        return CertificateAlgorithm::FpSmoothII;
    } else if (algorithm == "fp-roundii") {
        return CertificateAlgorithm::FpRoundII;
    } else if (algorithm == "ex-pi") {
        return CertificateAlgorithm::ExPI;
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unknown certificate algorithm: " << algorithm << ".");
}

}  // namespace storm
