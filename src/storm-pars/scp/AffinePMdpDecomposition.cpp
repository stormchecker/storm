#include "storm-pars/scp/AffinePMdpDecomposition.h"

#include <algorithm>

#include "storm-pars/utility/parametric.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::pars::scp {

namespace {
constexpr char const* NotAffine =
    "SCP requires an affine pMDP (every transition probability, and every state-action reward for reward properties, must have total degree <= 1 in the "
    "parameters), but ";
}  // namespace

UnpackedValue AffinePMdpDecomposition::decomposeAffine(storm::RationalFunction const& value) {
    std::set<storm::RationalFunctionVariable> occurring;
    storm::utility::parametric::gatherOccurringVariables(value, occurring);

    UnpackedValue unpacked;
    storm::utility::parametric::Valuation<storm::RationalFunction> zeroValuation;
    for (auto const& var : occurring) {
        zeroValuation[var] = storm::utility::zero<storm::RationalFunctionCoefficient>();
    }
    unpacked.constant = storm::utility::parametric::evaluate<double>(value, zeroValuation);

    unpacked.parameterCoefficients.reserve(occurring.size());
    for (auto const& var : occurring) {
        storm::RationalFunction derivative = value.derivative(var);
        STORM_LOG_ASSERT(derivative.isConstant(), "Derivative of an affine function w.r.t. one of its own parameters must be constant.");
        unpacked.parameterCoefficients.emplace_back(var, storm::utility::convertNumber<double>(derivative.constantPart()));
    }

    parameters.insert(occurring.begin(), occurring.end());
    return unpacked;
}

AffinePMdpDecomposition::AffinePMdpDecomposition(storm::models::sparse::Model<storm::RationalFunction> const& model,
                                                 storm::models::sparse::StandardRewardModel<storm::RationalFunction> const* rewardModel) {
    auto const& matrix = model.getTransitionMatrix();
    auto const& rowGroupIndices = matrix.getRowGroupIndices();
    auto stateOfRow = [&](uint64_t row) { return std::upper_bound(rowGroupIndices.begin(), rowGroupIndices.end(), row) - rowGroupIndices.begin() - 1; };

    rows.reserve(matrix.getRowCount());
    for (uint64_t row = 0; row < matrix.getRowCount(); ++row) {
        std::vector<UnpackedTransition> unpackedRow;
        unpackedRow.reserve(matrix.getRow(row).getNumberOfEntries());

        for (auto const& entry : matrix.getRow(row)) {
            STORM_LOG_THROW(storm::utility::parametric::isLinear(entry.getValue()), storm::exceptions::NotSupportedException,
                            NotAffine << "the transition from state " << stateOfRow(row) << " (row " << row << ") to state " << entry.getColumn() << " is '"
                                      << entry.getValue() << "', which is not affine.");
            UnpackedValue decomposed = decomposeAffine(entry.getValue());
            UnpackedTransition unpacked;
            unpacked.successor = entry.getColumn();
            unpacked.constant = decomposed.constant;
            unpacked.parameterCoefficients = std::move(decomposed.parameterCoefficients);
            unpackedRow.push_back(std::move(unpacked));
        }
        rows.push_back(std::move(unpackedRow));
    }

    if (rewardModel != nullptr) {
        std::vector<storm::RationalFunction> rewardVector = rewardModel->getTotalRewardVector(matrix);
        rewardRows.reserve(rewardVector.size());
        for (uint64_t row = 0; row < rewardVector.size(); ++row) {
            STORM_LOG_THROW(
                storm::utility::parametric::isLinear(rewardVector[row]), storm::exceptions::NotSupportedException,
                NotAffine << "the reward of state " << stateOfRow(row) << " (row " << row << ") is '" << rewardVector[row] << "', which is not affine.");
            rewardRows.push_back(decomposeAffine(rewardVector[row]));
        }
    }
}

std::vector<UnpackedTransition> const& AffinePMdpDecomposition::getRow(uint64_t row) const {
    return rows.at(row);
}

uint64_t AffinePMdpDecomposition::getNumberOfRows() const {
    return rows.size();
}

UnpackedValue const& AffinePMdpDecomposition::getReward(uint64_t row) const {
    return rewardRows.at(row);
}

std::set<storm::RationalFunctionVariable> const& AffinePMdpDecomposition::getParameters() const {
    return parameters;
}

}  // namespace storm::pars::scp
