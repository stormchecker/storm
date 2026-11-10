#pragma once

#include "storm/modelchecker/certificates/CertificateAlgorithm.h"
#include "storm/settings/modules/ModuleSettings.h"

namespace storm::settings::modules {

/*!
 * This class represents the settings for the production of certificates.
 * @see https://doi.org/10.1007/978-3-031-90653-4_7
 */
class CertificationSettings : public ModuleSettings {
   public:
    CertificationSettings();

    /*!
     * Whether a certificate is to be produced
     */
    bool isProduceCertificateSet() const;

    /*!
     * Whether topological methods are used to produce certificates
     */
    bool isUseTopologicalSet() const;

    /*!
     * The algorithm used to compute the values underlying a certificate
     */
    storm::CertificateAlgorithm getAlgorithm() const;

    /*!
     * The gamma parameter of the algorithm
     */
    double getGamma() const;

    /*!
     * The delta parameter of the algorithm
     */
    double getDelta() const;

    bool check() const override;

    // The name of the module.
    static const std::string moduleName;

   private:
    static const std::string produceCertificateOptionName;
    static const std::string useTopologicalOptionName;
    static const std::string methodOptionName;
};

}  // namespace storm::settings::modules
