#pragma once

#include <memory>
#include <optional>
#include <string>

#include "storm/modelchecker/certificates/ReachabilityProbabilityCertificate.h"
#include "storm/modelchecker/certificates/ReachabilityRewardCertificate.h"
#include "storm/solver/OptimizationDirection.h"

namespace storm {
class Environment;

namespace storage {
class BitVector;
template<typename ValueType>
class SparseMatrix;
}  // namespace storage

namespace modelchecker {

/*!
 * Computes a certificate for reachability probabilities (or constrained reachability probabilities if constraint states are given).
 * The algorithm used for the computation is selected in the certificate environment of the given environment.
 * Note that the computed certificate is not guaranteed to be valid: The algorithms trade speed for correctness (e.g. due to floating point errors or
 * conversions between value types). Use Certificate::checkValidity to detect invalid certificates.
 * @param dir the optimization direction. Should be empty if the model is deterministic.
 * @see https://doi.org/10.1007/978-3-031-90653-4_7
 */
template<typename ValueType>
std::unique_ptr<ReachabilityProbabilityCertificate<ValueType>> computeReachabilityProbabilityCertificate(
    storm::Environment const& env, std::optional<storm::OptimizationDirection> dir, storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
    storm::storage::BitVector targetStates, std::optional<storm::storage::BitVector> constraintStates = std::nullopt, std::string targetLabel = "goal",
    std::optional<std::string> constraintLabel = std::nullopt);

/*!
 * Computes a certificate for expected rewards accumulated until reaching the target states.
 * The algorithm used for the computation is selected in the certificate environment of the given environment.
 * Note that the computed certificate is not guaranteed to be valid: The algorithms trade speed for correctness (e.g. due to floating point errors or
 * conversions between value types). Use Certificate::checkValidity to detect invalid certificates.
 * @param dir the optimization direction. Should be empty if the model is deterministic.
 * @param stateActionRewardVector the reward for each choice
 * @see https://doi.org/10.1007/978-3-031-90653-4_7
 */
template<typename ValueType>
std::unique_ptr<ReachabilityRewardCertificate<ValueType>> computeReachabilityRewardCertificate(
    storm::Environment const& env, std::optional<storm::OptimizationDirection> dir, storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
    storm::storage::BitVector targetStates, std::vector<ValueType> stateActionRewardVector, std::string targetLabel = "goal", std::string rewardModelName = "");

}  // namespace modelchecker
}  // namespace storm