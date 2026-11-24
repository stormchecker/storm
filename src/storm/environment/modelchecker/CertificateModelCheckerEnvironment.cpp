#include "storm/environment/modelchecker/CertificateModelCheckerEnvironment.h"

namespace storm {

CertificateModelCheckerEnvironment::CertificateModelCheckerEnvironment()
    : algorithm(CertificateAlgorithm::FpII), gamma(0.0), delta(0.0), useTopological(false) {
    // Intentionally left empty.
}

CertificateModelCheckerEnvironment::~CertificateModelCheckerEnvironment() {
    // Intentionally left empty
}

CertificateAlgorithm CertificateModelCheckerEnvironment::getAlgorithm() const {
    return algorithm;
}

void CertificateModelCheckerEnvironment::setAlgorithm(CertificateAlgorithm value) {
    algorithm = value;
}

double CertificateModelCheckerEnvironment::getGamma() const {
    return gamma;
}

void CertificateModelCheckerEnvironment::setGamma(double value) {
    gamma = value;
}

double CertificateModelCheckerEnvironment::getDelta() const {
    return delta;
}

void CertificateModelCheckerEnvironment::setDelta(double value) {
    delta = value;
}

bool CertificateModelCheckerEnvironment::isUseTopological() const {
    return useTopological;
}

void CertificateModelCheckerEnvironment::setUseTopological(bool value) {
    useTopological = value;
}

}  // namespace storm
