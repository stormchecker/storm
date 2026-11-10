#pragma once

#include "storm/modelchecker/certificates/CertificateAlgorithm.h"

namespace storm {

/*!
 * Options for the computation of certificates for reachability probabilities and expected rewards.
 * @see https://doi.org/10.1007/978-3-031-90653-4_7
 */
class CertificateModelCheckerEnvironment {
   public:
    CertificateModelCheckerEnvironment();
    ~CertificateModelCheckerEnvironment();

    /*!
     * The algorithm used to compute the values underlying a certificate.
     */
    CertificateAlgorithm getAlgorithm() const;
    void setAlgorithm(CertificateAlgorithm value);

    /*!
     * The parameters gamma and delta of the smooth and rounding interval iteration variants.
     * @see https://doi.org/10.1007/978-3-031-90653-4_7
     */
    double getGamma() const;
    void setGamma(double value);
    double getDelta() const;
    void setDelta(double value);

    /*!
     * Whether to decompose the model into strongly connected components and compute the values for each of them separately.
     */
    bool isUseTopological() const;
    void setUseTopological(bool value);

   private:
    CertificateAlgorithm algorithm;
    double gamma;
    double delta;
    bool useTopological;
};

}  // namespace storm
