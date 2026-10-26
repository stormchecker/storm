#pragma once

#include <optional>
#include <string>
#include <vector>
#include "storm/modelchecker/certificates/Certificate.h"
#include "storm/modelchecker/certificates/RankingType.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/ExtendedNumber.h"

namespace storm {

class Environment;

namespace storage {

template<typename ValueType>
class SparseMatrix;

}

namespace modelchecker {

template<typename ValueType>
class ReachabilityRewardCertificate : public Certificate<ValueType> {
   public:
    using ExtendedValueType = storm::utility::ExtendedValueType<ValueType>;

    ReachabilityRewardCertificate(std::optional<storm::OptimizationDirection> dir, storm::storage::BitVector targetStates,
                                  std::vector<ValueType> stateActionRewardVector, std::string targetLabel = "goal", std::string rewardModelName = "");
    virtual ~ReachabilityRewardCertificate() = default;
    virtual bool checkValidity(storm::models::Model<ValueType> const& model) const override;
    bool checkValidity(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix) const;
    virtual storm::json<ValueType> toJson() const override;
    virtual std::string summaryString(storm::storage::BitVector const& relevantStates) const override;
    virtual std::unique_ptr<Certificate<ValueType>> clone() const override;

    void setLowerBoundsCertificate(std::vector<ExtendedValueType>&& values, std::vector<RankingType>&& ranks);
    void setUpperBoundsCertificate(std::vector<ExtendedValueType>&& values, std::vector<RankingType>&& ranks);

    bool hasLowerBoundsCertificate() const;
    bool hasUpperBoundsCertificate() const;

   private:
    template<storm::OptimizationDirection Dir>
    bool checkValidityInternal(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix) const;

    storm::storage::BitVector const targetStates;
    std::vector<ValueType> const stateActionRewardVector;
    std::string const targetLabel, rewardModelName;
    std::optional<storm::OptimizationDirection> const dir;

    struct {
        std::vector<ExtendedValueType> values;
        std::vector<RankingType> ranks;
    } upperBoundsCertificate;

    struct {
        std::vector<ExtendedValueType> values;
        std::vector<RankingType> ranks;
    } lowerBoundsCertificate;
};

}  // namespace modelchecker
}  // namespace storm