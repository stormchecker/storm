#pragma once

#include <memory>

#include "storm/modelchecker/certificates/Certificate.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/storage/BitVector.h"

namespace storm::models {
template<typename ValueType>
class Model;
}

namespace storm::modelchecker {

template<typename ValueType>
class ExplicitCertificateCheckResult : public CheckResult {
   public:
    ExplicitCertificateCheckResult(std::unique_ptr<Certificate<ValueType>>&& certificate, storm::storage::BitVector relevantStates);
    virtual bool isExplicit() const override;
    virtual bool isExplicitCertificateCheckResult() const override;
    virtual bool isResultForAllStates() const override;
    virtual std::unique_ptr<CheckResult> clone() const override;
    virtual void filter(QualitativeCheckResult const& filter) override;
    virtual std::ostream& writeToStream(std::ostream& out) const override;
    Certificate<ValueType> const& getCertificate() const;
    Certificate<ValueType>& getCertificate();
    bool checkValidity(storm::models::Model<ValueType> const& model);

   private:
    std::unique_ptr<Certificate<ValueType>> certificate;
    storm::storage::BitVector relevantStates;
};
}  // namespace storm::modelchecker