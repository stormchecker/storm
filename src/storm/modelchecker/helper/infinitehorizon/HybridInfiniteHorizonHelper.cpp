#include "storm/modelchecker/helper/infinitehorizon/HybridInfiniteHorizonHelper.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/helper/infinitehorizon/SparseDeterministicInfiniteHorizonHelper.h"
#include "storm/modelchecker/helper/infinitehorizon/SparseNondeterministicInfiniteHorizonHelper.h"
#include "storm/modelchecker/helper/utility/SetInformationFromOtherHelper.h"
#include "storm/models/symbolic/NondeterministicModel.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace helper {

template<typename ValueType, storm::dd::DdType DdType, bool Nondeterministic>
HybridInfiniteHorizonHelper<ValueType, DdType, Nondeterministic>::HybridInfiniteHorizonHelper(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                                              storm::dd::Add<DdType, ValueType> const& transitionMatrix)
    : _model(model), _transitionMatrix(transitionMatrix), _markovianStates(nullptr), _exitRates(nullptr) {
    STORM_LOG_ASSERT(model.isNondeterministicModel() == Nondeterministic, "Template Parameter does not match model type.");
}

template<typename ValueType, storm::dd::DdType DdType, bool Nondeterministic>
HybridInfiniteHorizonHelper<ValueType, DdType, Nondeterministic>::HybridInfiniteHorizonHelper(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                                              storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                                              storm::dd::Bdd<DdType> const& markovianStates,
                                                                                              storm::dd::Add<DdType, ValueType> const& exitRateVector)
    : _model(model), _transitionMatrix(transitionMatrix), _markovianStates(&markovianStates), _exitRates(&exitRateVector) {
    STORM_LOG_ASSERT(model.isNondeterministicModel() == Nondeterministic, "Template Parameter does not match model type.");
}

template<typename ValueType, storm::dd::DdType DdType, bool Nondeterministic>
HybridInfiniteHorizonHelper<ValueType, DdType, Nondeterministic>::HybridInfiniteHorizonHelper(storm::models::symbolic::Model<DdType, ValueType> const& model,
                                                                                              storm::dd::Add<DdType, ValueType> const& transitionMatrix,
                                                                                              storm::dd::Add<DdType, ValueType> const& exitRateVector)
    : _model(model), _transitionMatrix(transitionMatrix), _markovianStates(nullptr), _exitRates(&exitRateVector) {
    STORM_LOG_ASSERT(model.isNondeterministicModel() == Nondeterministic, "Template Parameter does not match model type.");
}

template<typename ValueType, storm::dd::DdType DdType, bool Nondeterministic>
std::unique_ptr<HybridQuantitativeCheckResult<DdType, ValueType>>
HybridInfiniteHorizonHelper<ValueType, DdType, Nondeterministic>::computeLongRunAverageProbabilities(Environment const& env,
                                                                                                     storm::dd::Bdd<DdType> const& psiStates) {
    // Convert this query to an instance for the sparse engine.
    // Create ODD for the translation.
    storm::dd::Odd odd = _model.getReachableStates().createOdd();
    // Translate all required components
    storm::storage::SparseMatrix<ValueType> explicitTransitionMatrix;
    if (Nondeterministic) {
        explicitTransitionMatrix = _transitionMatrix.toMatrix(
            dynamic_cast<storm::models::symbolic::NondeterministicModel<DdType, ValueType> const&>(_model).getNondeterminismVariables(), odd, odd);
    } else {
        explicitTransitionMatrix = _transitionMatrix.toMatrix(odd, odd);
    }
    std::vector<ValueType> explicitExitRateVector;
    storm::storage::BitVector explicitMarkovianStates;
    if (isContinuousTime()) {
        explicitExitRateVector = _exitRates->toVector(odd);
        if (_markovianStates) {
            explicitMarkovianStates = _markovianStates->toVector(odd);
        }
    }
    auto sparseHelper = createSparseHelper(explicitTransitionMatrix, explicitMarkovianStates, explicitExitRateVector, odd);
    auto explicitResult = sparseHelper->computeLongRunAverageProbabilities(env, psiStates.toVector(odd));
    return std::make_unique<HybridQuantitativeCheckResult<DdType, ValueType>>(_model.getReachableStates(), _model.getManager().getBddZero(),
                                                                              _model.getManager().template getAddZero<ValueType>(), _model.getReachableStates(),
                                                                              std::move(odd), std::move(explicitResult));
}

template<typename ValueType, storm::dd::DdType DdType, bool Nondeterministic>
std::unique_ptr<HybridQuantitativeCheckResult<DdType, ValueType>>
HybridInfiniteHorizonHelper<ValueType, DdType, Nondeterministic>::computeLongRunAverageRewards(
    Environment const& env, storm::models::symbolic::StandardRewardModel<DdType, ValueType> const& rewardModel) {
    // Convert this query to an instance for the sparse engine.
    // Create ODD for the translation.
    storm::dd::Odd odd = _model.getReachableStates().createOdd();

    // Translate all required components
    // Transitions and rewards
    storm::storage::SparseMatrix<ValueType> explicitTransitionMatrix;
    std::vector<ValueType> explicitStateRewards, explicitActionRewards;
    if (rewardModel.hasStateRewards()) {
        explicitStateRewards = rewardModel.getStateRewardVector().toVector(odd);
    }
    if (Nondeterministic && rewardModel.hasStateActionRewards()) {
        // Matrix and action-based vector have to be produced at the same time to guarantee the correct order
        auto matrixRewards = _transitionMatrix.toMatrixVector(
            rewardModel.getStateActionRewardVector(),
            dynamic_cast<storm::models::symbolic::NondeterministicModel<DdType, ValueType> const&>(_model).getNondeterminismVariables(), odd, odd);
        explicitTransitionMatrix = std::move(matrixRewards.first);
        explicitActionRewards = std::move(matrixRewards.second);
    } else {
        // Translate matrix only
        if (Nondeterministic) {
            explicitTransitionMatrix = _transitionMatrix.toMatrix(
                dynamic_cast<storm::models::symbolic::NondeterministicModel<DdType, ValueType> const&>(_model).getNondeterminismVariables(), odd, odd);
        } else {
            explicitTransitionMatrix = _transitionMatrix.toMatrix(odd, odd);
        }
        if (rewardModel.hasStateActionRewards()) {
            // For deterministic models we can translate the action rewards easily
            explicitActionRewards = rewardModel.getStateActionRewardVector().toVector(odd);
        }
    }
    STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotSupportedException, "Transition rewards are not supported in this engine.");
    // Continuous time information
    std::vector<ValueType> explicitExitRateVector;
    storm::storage::BitVector explicitMarkovianStates;
    if (isContinuousTime()) {
        explicitExitRateVector = _exitRates->toVector(odd);
        if (_markovianStates) {
            explicitMarkovianStates = _markovianStates->toVector(odd);
        }
    }
    auto sparseHelper = createSparseHelper(explicitTransitionMatrix, explicitMarkovianStates, explicitExitRateVector, odd);
    auto explicitResult = sparseHelper->computeLongRunAverageValues(env, rewardModel.hasStateRewards() ? &explicitStateRewards : nullptr,
                                                                    rewardModel.hasStateActionRewards() ? &explicitActionRewards : nullptr);
    return std::make_unique<HybridQuantitativeCheckResult<DdType, ValueType>>(_model.getReachableStates(), _model.getManager().getBddZero(),
                                                                              _model.getManager().template getAddZero<ValueType>(), _model.getReachableStates(),
                                                                              std::move(odd), std::move(explicitResult));
}

template<typename ValueType, storm::dd::DdType DdType, bool Nondeterministic>
bool HybridInfiniteHorizonHelper<ValueType, DdType, Nondeterministic>::isContinuousTime() const {
    STORM_LOG_ASSERT((_markovianStates == nullptr) || (_exitRates != nullptr), "Inconsistent information given: Have Markovian states but no exit rates.");
    return _exitRates != nullptr;
}

template<typename ValueType, storm::dd::DdType DdType, bool Nondeterministic>
template<bool N, std::enable_if_t<N, int>>
std::unique_ptr<SparseInfiniteHorizonHelper<ValueType, Nondeterministic>> HybridInfiniteHorizonHelper<ValueType, DdType, Nondeterministic>::createSparseHelper(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& markovianStates, std::vector<ValueType> const& exitRates,
    storm::dd::Odd const& odd) const {
    std::unique_ptr<SparseInfiniteHorizonHelper<ValueType, Nondeterministic>> result;
    if (isContinuousTime()) {
        result =
            std::make_unique<storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType>>(transitionMatrix, markovianStates, exitRates);
    } else {
        result = std::make_unique<storm::modelchecker::helper::SparseNondeterministicInfiniteHorizonHelper<ValueType>>(transitionMatrix);
    }
    storm::modelchecker::helper::setInformationFromOtherHelperNondeterministic(*result, *this,
                                                                               [&odd](storm::dd::Bdd<DdType> const& s) { return s.toVector(odd); });
    STORM_LOG_WARN_COND(!this->isProduceSchedulerSet(), "Scheduler extraction not supported in Hybrid engine.");
    return result;
}

template<typename ValueType, storm::dd::DdType DdType, bool Nondeterministic>
template<bool N, std::enable_if_t<!N, int>>
std::unique_ptr<SparseInfiniteHorizonHelper<ValueType, Nondeterministic>> HybridInfiniteHorizonHelper<ValueType, DdType, Nondeterministic>::createSparseHelper(
    storm::storage::SparseMatrix<ValueType> const& transitionMatrix, storm::storage::BitVector const& markovianStates, std::vector<ValueType> const& exitRates,
    storm::dd::Odd const& odd) const {
    std::unique_ptr<SparseInfiniteHorizonHelper<ValueType, Nondeterministic>> result;
    if (isContinuousTime()) {
        result = std::make_unique<storm::modelchecker::helper::SparseDeterministicInfiniteHorizonHelper<ValueType>>(transitionMatrix, exitRates);
    } else {
        result = std::make_unique<storm::modelchecker::helper::SparseDeterministicInfiniteHorizonHelper<ValueType>>(transitionMatrix);
    }
    storm::modelchecker::helper::setInformationFromOtherHelperDeterministic(*result, *this,
                                                                            [&odd](storm::dd::Bdd<DdType> const& s) { return s.toVector(odd); });
    return result;
}

template class HybridInfiniteHorizonHelper<double, storm::dd::DdType::CUDD, false>;
template class HybridInfiniteHorizonHelper<double, storm::dd::DdType::CUDD, true>;
template class HybridInfiniteHorizonHelper<double, storm::dd::DdType::Sylvan, false>;
template class HybridInfiniteHorizonHelper<double, storm::dd::DdType::Sylvan, true>;
template class HybridInfiniteHorizonHelper<storm::RationalNumber, storm::dd::DdType::Sylvan, false>;
template class HybridInfiniteHorizonHelper<storm::RationalNumber, storm::dd::DdType::Sylvan, true>;
template class HybridInfiniteHorizonHelper<storm::RationalFunction, storm::dd::DdType::Sylvan, false>;

}  // namespace helper
}  // namespace modelchecker
}  // namespace storm