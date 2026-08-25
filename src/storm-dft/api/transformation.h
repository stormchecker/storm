#pragma once

#include <utility>

#include "storm-dft/storage/DFT.h"

namespace storm::dft {
namespace api {

/*!
 * Check whether the DFT is well-formed.
 *
 * @param dft DFT.
 * @param validForMarkovianAnalysis If true, additional checks are performed to check whether the DFT is valid for analysis via Markov models.
 * @return Pair where the first entry is true iff the DFT is well-formed. The second entry contains the error messages for ill-formed parts.
 */
template<typename ValueType>
std::pair<bool, std::string> isWellFormed(storm::dft::storage::DFT<ValueType> const& dft, bool validForMarkovianAnalysis = true);

/*!
 * Check whether the DFT has potential modeling issues.
 *
 * @param dft DFT.
 * @return Pair where the first entry is true iff the DFT has potential modeling issues. The second entry contains the warning messages for the issues.
 */
template<typename ValueType>
std::pair<bool, std::string> hasPotentialModelingIssues(storm::dft::storage::DFT<ValueType> const& dft);

/*!
 * Apply transformations for DFT.
 *
 * @param dft DFT.
 * @param uniqueBE Flag whether a unique constant failed BE is created.
 * @param binaryFDEP Flag whether all dependencies should be binary (only one dependent child).
 * @param exponentialDistributions Flag whether distributions should be transformed to exponential distributions (if possible).
 * @return Transformed DFT.
 */
template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> applyTransformations(storm::dft::storage::DFT<ValueType> const& dft, bool uniqueBE, bool binaryFDEP,
                                                                          bool exponentialDistributions);

/*!
 * Apply transformations to make DFT feasible for Markovian analysis.
 *
 * @param dft DFT.
 * @return Transformed DFT.
 */
template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> prepareForMarkovAnalysis(storm::dft::storage::DFT<ValueType> const& dft);

}  // namespace api
}  // namespace storm::dft
