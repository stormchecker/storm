#pragma once

#include <optional>
#include <string>
#include <vector>
#include "storm/modelchecker/certificates/Certificate.h"
#include "storm/modelchecker/certificates/RankingType.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/BitVector.h"

namespace storm {

class Environment;

namespace storage {

template<typename ValueType>
class SparseMatrix;

}

namespace modelchecker {

template<typename ValueType>
class ReachabilityProbabilityCertificate : public Certificate<ValueType> {
   public:
    ReachabilityProbabilityCertificate(std::optional<storm::OptimizationDirection> dir, storm::storage::BitVector targetStates,
                                       std::string targetLabel = "goal");
    ReachabilityProbabilityCertificate(std::optional<storm::OptimizationDirection> dir, storm::storage::BitVector targetStates,
                                       storm::storage::BitVector constraintStates, std::string targetLabel = "goal",
                                       std::string constraintLabel = "constraint");
    virtual ~ReachabilityProbabilityCertificate() = default;
    virtual bool checkValidity(storm::models::Model<ValueType> const& model) const override;
    bool checkValidity(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix) const;
    virtual storm::json<ValueType> toJson() const override;
    virtual std::string summaryString(storm::storage::BitVector const& relevantStates) const override;
    virtual std::unique_ptr<Certificate<ValueType>> clone() const override;

    void setLowerBoundsCertificate(std::vector<ValueType>&& values, std::vector<RankingType>&& ranks);
    void setUpperBoundsCertificate(std::vector<ValueType>&& values);

    bool hasLowerBoundsCertificate() const;
    bool hasUpperBoundsCertificate() const;

   private:
    bool hasConstraintStates() const;
    template<storm::OptimizationDirection Dir>
    bool checkValidityInternal(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix) const;

    storm::storage::BitVector const targetStates;
    std::optional<storm::storage::BitVector> const constraintStates;  // If empty, there is no constraint (which is different from an empty set)
    std::string const targetLabel, constraintLabel;
    std::optional<storm::OptimizationDirection> const dir;

    struct {
        std::vector<ValueType> values;
    } upperBoundsCertificate;

    struct {
        std::vector<ValueType> values;
        std::vector<RankingType> ranks;
    } lowerBoundsCertificate;
};

}  // namespace modelchecker
}  // namespace storm