#include "storm/modelchecker/certificates/computation/ReachabilityCertificateComputer.h"

#include <algorithm>
#include <cfenv>
#include <vector>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/CertificateModelCheckerEnvironment.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/environment/solver/EigenSolverEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnmetRequirementException.h"
#include "storm/modelchecker/certificates/RankingType.h"
#include "storm/modelchecker/certificates/ReachabilityProbabilityCertificate.h"
#include "storm/modelchecker/certificates/ReachabilityRewardCertificate.h"
#include "storm/modelchecker/certificates/utility/ExtendedValueUtility.h"
#include "storm/modelchecker/certificates/utility/LowerUpperBoundsComputer.h"
#include "storm/modelchecker/certificates/utility/ReachabilityProbabilityToRewardTransformer.h"
#include "storm/modelchecker/certificates/utility/ReachabilityRewardTransformer.h"
#include "storm/solver/IterativeMinMaxLinearEquationSolver.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/solver/helper/IntervalterationHelper.h"
#include "storm/solver/helper/ValueIterationOperator.h"
#include "storm/storage/BackwardTransitionCache.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/transformer/EndComponentEliminator.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/Extremum.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/OptionalRef.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/vector.h"

namespace storm::modelchecker {

namespace {
/*!
 * Sets the rounding mode of the floating point environment and restores the previous one when going out of scope (also if an exception is thrown).
 */
class ScopedRoundingMode {
   public:
    explicit ScopedRoundingMode(int mode) : previousMode(std::fegetround()) {
        std::fesetround(mode);
    }
    ~ScopedRoundingMode() {
        std::fesetround(previousMode);
    }
    ScopedRoundingMode(ScopedRoundingMode const&) = delete;
    ScopedRoundingMode& operator=(ScopedRoundingMode const&) = delete;

   private:
    int const previousMode;
};
}  // namespace

template<typename ValueType>
struct LowerUpperValueCertificateComputerReturnType {
    std::vector<ValueType> lowerValues;  /// Lower values for each state
    std::vector<ValueType> upperValues;  // Upper values for each state
};

template<typename ValueType, bool Nondeterministic, storm::OptimizationDirection Dir>
class LowerUpperValueCertificateComputer {
   public:
    using AlgorithmType = storm::CertificateAlgorithm;
    struct Algorithm {
        AlgorithmType type;
        ValueType gamma;
        ValueType delta;
    };

    LowerUpperValueCertificateComputer(storm::CertificateModelCheckerEnvironment const& certificateEnv,
                                       storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
                                       TotalRewardSpecification<ValueType> const& totalRewardSpecification)
        : certificateEnv(certificateEnv), transitionProbabilityMatrix(transitionProbabilityMatrix), totalRewardSpecification(totalRewardSpecification) {}

    Algorithm getAlgorithm() const {
        if (certificateEnv.getAlgorithm() == AlgorithmType::FpSmoothII || certificateEnv.getAlgorithm() == AlgorithmType::FpRoundII) {
            // For gamma = 1, the bounds are never updated, so the iteration would not terminate.
            STORM_LOG_THROW(certificateEnv.getGamma() < 1.0, storm::exceptions::InvalidEnvironmentException,
                            "The parameter gamma for smooth and rounding interval iteration must be smaller than 1.");
        }
        return {certificateEnv.getAlgorithm(), storm::utility::convertNumber<ValueType>(certificateEnv.getGamma()),
                storm::utility::convertNumber<ValueType>(certificateEnv.getDelta())};
    }

    LowerUpperValueCertificateComputerReturnType<ValueType> compute(bool relative, ValueType const& precision) {
        auto alg = getAlgorithm();
        initializeValueVectors(alg);
        if (!totalRewardSpecification.terminalStates.full()) {
            if (certificateEnv.isUseTopological()) {
                computeTopological(alg, relative, precision, ~totalRewardSpecification.terminalStates);
            } else {
                computeForSubsystem(alg, relative, precision, ~totalRewardSpecification.terminalStates);
            }
        }
        if (globalUpperValues.empty()) {
            // Exact methods do not populate the upper values.
            globalUpperValues = globalLowerValues;  // copy lower values
        }
        return {std::move(globalLowerValues), std::move(globalUpperValues)};
    }

   private:
    template<typename VT, bool SingleValue>
    struct SubsystemData {
        storm::storage::SparseMatrix<VT> transitions;
        storm::storage::BitVector exitChoices;
        using ValueVectorType = std::conditional_t<SingleValue, std::vector<VT>, std::pair<std::vector<VT>, std::vector<VT>>>;
        ValueVectorType offsets;
        ValueVectorType operands;
        std::optional<std::vector<VT>> exitProbabilities;  // for each row the probability to exit the subsystem
        // if originalToReducedStateMapping is given, this subsystem has been reduced and the ith entry is the index in the reduced system corresponding to
        // the ith subsystem state
        std::optional<std::vector<uint64_t>> originalToReducedStateMapping;
    };

    void initializeValueVectors(Algorithm const& alg) {
        // Initialize value vector and set values for terminal states.
        uint64_t const numberOfStates = totalRewardSpecification.terminalStates.size();
        globalLowerValues.assign(numberOfStates, storm::utility::zero<ValueType>());
        for (auto const& [value, stateSet] : totalRewardSpecification.terminalStateValues) {
            if (value.isFinite()) {
                storm::utility::vector::setVectorValues(globalLowerValues, stateSet, value.getFiniteValue());
            } else {
                STORM_LOG_ASSERT(value.isPositiveInfinity(), "Unexpected terminal state value.");
                // Infinity is not representable in ValueType (in particular not for rational numbers).
                // The values of these states are never used in any computation, because choices leading to such states are either cut away or do not exist.
                // The actual infinite values are set by the caller.
            }
        }
        // We assume that 0 is a valid lower bound for all states.
        STORM_LOG_THROW(!totalRewardSpecification.actionRewards.has_value() ||
                            std::all_of(totalRewardSpecification.actionRewards->begin(), totalRewardSpecification.actionRewards->end(),
                                        [](auto const& v) { return v >= storm::utility::zero<ValueType>(); }),
                        storm::exceptions::NotSupportedException, "Certificates for negative rewards are not supported.");

        // Initialize second vector for upper bounds (if needed by the selected algorithm)
        if (alg.type == AlgorithmType::FpII || alg.type == AlgorithmType::FpSmoothII || alg.type == AlgorithmType::FpRoundII) {
            globalUpperValues = globalLowerValues;
        } else {
            STORM_LOG_ASSERT(alg.type == AlgorithmType::ExPI, "Unsupported algorithm.");
            globalUpperValues.clear();  // Do not use upper values for this.
        }
    }

    storm::storage::BitVector getSubsystemExitChoices(storm::storage::BitVector const& subsystem, uint64_t numSubystemRows,
                                                      std::optional<storm::storage::BitVector> const& optionalSubsystemChoices) const {
        storm::storage::BitVector result(numSubystemRows, false);
        uint64_t currSubsystemRow = 0;
        for (auto const state : subsystem) {
            for (auto const rowIndex : transitionProbabilityMatrix.getRowGroupIndices(state)) {
                if (optionalSubsystemChoices.has_value() && !optionalSubsystemChoices->get(rowIndex)) {
                    continue;
                }
                auto row = transitionProbabilityMatrix.getRow(rowIndex);
                if (std::any_of(row.begin(), row.end(), [&subsystem](auto const& entry) { return !subsystem.get(entry.getColumn()); })) {
                    result.set(currSubsystemRow);
                }
                ++currSubsystemRow;
            }
        }
        return result;
    }

    template<typename VT>
    std::vector<VT> getSubsystemOffsets(storm::storage::BitVector const& subsystem, std::optional<storm::storage::BitVector> const& optionalSubsystemChoices,
                                        storm::storage::BitVector const& subsystemExitChoices, std::vector<ValueType> const& globalValues) const {
        std::vector<VT> result;
        result.reserve(subsystemExitChoices.size());
        for (auto const state : subsystem) {
            for (auto const rowIndex : transitionProbabilityMatrix.getRowGroupIndices(state)) {
                if (optionalSubsystemChoices.has_value() && !optionalSubsystemChoices->get(rowIndex)) {
                    continue;
                }
                auto rowValue = totalRewardSpecification.actionRewards.has_value()
                                    ? storm::utility::convertNumber<VT>(totalRewardSpecification.actionRewards.value()[rowIndex])
                                    : storm::utility::zero<VT>();
                if (subsystemExitChoices.get(result.size())) {
                    for (auto const& entry : transitionProbabilityMatrix.getRow(rowIndex)) {
                        if (!subsystem.get(entry.getColumn())) {
                            rowValue += storm::utility::convertNumber<VT, ValueType>(entry.getValue() * globalValues[entry.getColumn()]);
                        }
                    }
                }
                result.push_back(std::move(rowValue));
            }
        }
        result.shrink_to_fit();
        STORM_LOG_ASSERT(result.size() == subsystemExitChoices.size(), "Unexpected number of rows in subsystem.");
        return result;
    }

    template<typename VT>
    std::vector<VT> getSubsystemExitProbabilities(storm::storage::BitVector const& subsystem,
                                                  std::optional<storm::storage::BitVector> const& optionalSubsystemChoices,
                                                  storm::storage::BitVector const& subsystemExitChoices) const {
        std::vector<VT> result;
        result.reserve(subsystemExitChoices.size());
        for (auto const state : subsystem) {
            for (auto const rowIndex : transitionProbabilityMatrix.getRowGroupIndices(state)) {
                if (optionalSubsystemChoices.has_value() && !optionalSubsystemChoices->get(rowIndex)) {
                    continue;
                }
                auto rowValue = storm::utility::zero<VT>();
                if (subsystemExitChoices.get(result.size())) {
                    for (auto const& entry : transitionProbabilityMatrix.getRow(rowIndex)) {
                        if (!subsystem.get(entry.getColumn())) {
                            rowValue += storm::utility::convertNumber<VT, ValueType>(entry.getValue());
                        }
                    }
                }
                result.push_back(std::move(rowValue));
            }
        }
        result.shrink_to_fit();
        STORM_LOG_ASSERT(result.size() == subsystemExitChoices.size(), "Unexpected number of rows in subsystem.");
        return result;
    }

    std::optional<storm::storage::BitVector> getSubsystemChoicesIfNonTrivial(storm::storage::BitVector const& subsystem) const {
        STORM_LOG_ASSERT(totalRewardSpecification.terminalStateValues.size() == 1, "Unexpected number of terminal state values.");
        STORM_LOG_ASSERT(subsystem.isDisjointFrom(totalRewardSpecification.terminalStates), "subsystem contains terminal state.");
        auto const& [value, stateSet] = totalRewardSpecification.terminalStateValues.front();
        STORM_LOG_ASSERT(value.isFinite() || value.isPositiveInfinity(), "Unexpected terminal state value.");
        if (storm::solver::minimize(Dir) && value.isPositiveInfinity()) {
            // We have an infinite terminal state value, so we need to cut away all choices leading to those states.
            return transitionProbabilityMatrix.getRowFilter(subsystem, ~stateSet);
        }
        return std::nullopt;
    }

    template<typename VT>
    storm::storage::SparseMatrix<VT> getSubsystemMatrix(storm::storage::BitVector const& subsystemStates,
                                                        std::optional<storm::storage::BitVector> const& subsystemChoices) const {
        storm::storage::SparseMatrix<ValueType> subMatrix = subsystemChoices.has_value()
                                                                ? transitionProbabilityMatrix.getSubmatrix(false, *subsystemChoices, subsystemStates)
                                                                : transitionProbabilityMatrix.getSubmatrix(true, subsystemStates, subsystemStates);
        // Convert to desired value type
        if constexpr (std::is_same_v<VT, ValueType>) {
            return subMatrix;
        } else {
            return subMatrix.template toValueType<VT>();
        }
    }

    template<typename VT, bool SingleValue>
    SubsystemData<VT, SingleValue> initializeSubsystemData(storm::storage::BitVector const& subsystem, AlgorithmType const algorithmType) const {
        auto optionalSubsystemChoices = getSubsystemChoicesIfNonTrivial(subsystem);
        auto result = SubsystemData<VT, SingleValue>{getSubsystemMatrix<VT>(subsystem, optionalSubsystemChoices), {}, {}, {}, {}, {}};
        result.exitChoices = getSubsystemExitChoices(subsystem, result.transitions.getRowCount(), optionalSubsystemChoices);
        if constexpr (SingleValue) {
            STORM_LOG_ASSERT(globalUpperValues.empty(), "Expected upper values not to be initialized.");
            result.offsets = getSubsystemOffsets<VT>(subsystem, optionalSubsystemChoices, result.exitChoices, globalLowerValues);
        } else {
            STORM_LOG_ASSERT(!globalUpperValues.empty(), "Expected upper values to be initialized.");
            result.offsets = {getSubsystemOffsets<VT>(subsystem, optionalSubsystemChoices, result.exitChoices, globalLowerValues),
                              getSubsystemOffsets<VT>(subsystem, optionalSubsystemChoices, result.exitChoices, globalUpperValues)};
        }
        bool const computeInitialBounds =
            algorithmType == AlgorithmType::FpII || algorithmType == AlgorithmType::FpSmoothII || algorithmType == AlgorithmType::FpRoundII;
        if (computeInitialBounds) {
            result.exitProbabilities = getSubsystemExitProbabilities<VT>(subsystem, optionalSubsystemChoices, result.exitChoices);
        }
        if (!totalRewardSpecification.terminalStatesUniversallyAlmostSurelyReached) {
            result = eliminateECs(std::move(result));
        }
        if (computeInitialBounds) {
            std::optional<storm::OptimizationDirection> constexpr optionalDir =
                Nondeterministic ? std::optional<storm::OptimizationDirection>(Dir) : std::nullopt;
            storm::OptionalRef<std::vector<VT>> lowerBounds, upperBounds;
            if constexpr (SingleValue) {
                result.operands = computeLowerOrUpperBoundsTotalReward<VT, true>(result.transitions, result.offsets, result.exitChoices,
                                                                                 result.exitProbabilities.value(), optionalDir);
            } else {
                result.operands.first = computeLowerOrUpperBoundsTotalReward<VT, true>(result.transitions, result.offsets.first, result.exitChoices,
                                                                                       result.exitProbabilities.value(), optionalDir);
                result.operands.second = computeLowerOrUpperBoundsTotalReward<VT, false>(result.transitions, result.offsets.second, result.exitChoices,
                                                                                         result.exitProbabilities.value(), optionalDir);
            }
        } else {
            if constexpr (SingleValue) {
                result.operands = std::vector<VT>(result.transitions.getRowGroupCount(), storm::utility::zero<VT>());
            } else {
                result.operands = {std::vector<VT>(result.transitions.getRowGroupCount(), storm::utility::zero<VT>()),
                                   std::vector<VT>(result.transitions.getRowGroupCount(), storm::utility::zero<VT>())};
            }
        }

        return result;
    }

    template<typename VT, bool SingleValue>
    SubsystemData<VT, SingleValue> eliminateECs(SubsystemData<VT, SingleValue>&& original) const {
        storm::storage::BitVector allStates(original.transitions.getRowGroupCount(), true);
        auto possibleECChoices = ~original.exitChoices;
        if (totalRewardSpecification.actionRewards) {
            // Do not consider choices with rewards
            auto const candidateChoices = possibleECChoices;
            for (auto const choice : candidateChoices) {
                if constexpr (SingleValue) {
                    if (!storm::utility::isZero(original.offsets[choice])) {
                        possibleECChoices.set(choice, false);
                    }
                } else {
                    STORM_LOG_ASSERT(storm::utility::isZero(original.offsets.first[choice]) == storm::utility::isZero(original.offsets.second[choice]),
                                     "Unexpected offsets in non-exiting choice.");
                    if (!storm::utility::isZero(original.offsets.first[choice])) {
                        possibleECChoices.set(choice, false);
                    }
                }
            }
        }
        storm::storage::MaximalEndComponentDecomposition<VT> ecs(original.transitions, original.transitions.transpose(true), allStates, possibleECChoices);
        if (ecs.empty()) {
            // No ECs to eliminate
            return std::move(original);
        }
        auto reductionRes = storm::transformer::EndComponentEliminator<VT>::transform(original.transitions, ecs, allStates, ~allStates, false);
        SubsystemData<VT, SingleValue> reduced{
            std::move(reductionRes.matrix), std::move(reductionRes.sinkRows), {}, {}, {}, std::move(reductionRes.oldToNewStateMapping)};
        if constexpr (SingleValue) {
            reduced.offsets.reserve(reduced.exitChoices.size());
        } else {
            reduced.offsets.first.reserve(reduced.exitChoices.size());
            reduced.offsets.second.reserve(reduced.exitChoices.size());
        }
        if (original.exitProbabilities.has_value()) {
            reduced.exitProbabilities.emplace();
            reduced.exitProbabilities->reserve(reduced.exitChoices.size());
        }
        for (uint64_t newRow = 0; newRow < reduced.exitChoices.size(); ++newRow) {
            auto const oldRow = reductionRes.newToOldRowMapping[newRow];
            if (original.exitProbabilities.has_value()) {
                // Set exit probability to '1' for all sinkRows, i.e., those that correspond to staying in the EC forever
                reduced.exitProbabilities->push_back(reduced.exitChoices.get(newRow) ? storm::utility::one<VT>() : original.exitProbabilities->at(oldRow));
            }
            if (original.exitChoices.get(oldRow)) {
                reduced.exitChoices.set(newRow, true);
            }
            if constexpr (SingleValue) {
                reduced.offsets.push_back(original.offsets[oldRow]);
            } else {
                reduced.offsets.first.push_back(original.offsets.first[oldRow]);
                reduced.offsets.second.push_back(original.offsets.second[oldRow]);
            }
        }
        STORM_LOG_ASSERT(std::all_of(reduced.originalToReducedStateMapping->begin(), reduced.originalToReducedStateMapping->end(),
                                     [&reduced](auto const& i) { return i < reduced.transitions.getRowGroupCount(); }),
                         "No representative for some state found in the reduced subsystem.");
        return reduced;
    }

    template<typename VT>
    void setGlobalValuesFromSubsystemVector(storm::storage::BitVector const& subsystemStates, std::vector<VT> const& localValues,
                                            std::vector<ValueType>& globalValues, std::optional<std::vector<uint64_t>> const& reducedStateMapping,
                                            bool invert) {
        STORM_LOG_ASSERT(subsystemStates.size() == globalValues.size(), "Unexpected number of values.");
        if (reducedStateMapping) {
            STORM_LOG_ASSERT(subsystemStates.getNumberOfSetBits() == reducedStateMapping->size(), "Unexpected number of states in unreduced subsystem.");
            uint64_t unreducedSubsystemState = 0;
            for (auto globalState : subsystemStates) {
                auto const reducedSubsystemState = reducedStateMapping->at(unreducedSubsystemState);
                STORM_LOG_ASSERT(reducedSubsystemState < localValues.size(), "unexpeced index in reduced subsystem.");
                globalValues[globalState] = storm::utility::convertNumber<ValueType>(localValues[reducedSubsystemState]);
                if (invert) {
                    globalValues[globalState] = -globalValues[globalState];
                }
                ++unreducedSubsystemState;
            }
        } else {
            STORM_LOG_ASSERT(subsystemStates.getNumberOfSetBits() == localValues.size(), "Unexpected number of values in subsystem.");
            auto localValuesIt = localValues.cbegin();
            for (auto i : subsystemStates) {
                globalValues[i] = storm::utility::convertNumber<ValueType>(*localValuesIt);
                if (invert) {
                    globalValues[i] = -globalValues[i];
                }
                ++localValuesIt;
            }
        }
    }

    template<typename VT, bool SingleValue>
    void setGlobalValuesFromSubsystem(storm::storage::BitVector const& subsystemStates, SubsystemData<VT, SingleValue> const& subsystemData,
                                      bool invertLowerValues) {
        if constexpr (SingleValue) {
            setGlobalValuesFromSubsystemVector<VT>(subsystemStates, subsystemData.operands, globalLowerValues, subsystemData.originalToReducedStateMapping,
                                                   invertLowerValues);
            STORM_LOG_ASSERT(globalUpperValues.empty(), "Expected upper values not to be initialized.");
        } else {
            setGlobalValuesFromSubsystemVector<VT>(subsystemStates, subsystemData.operands.first, globalLowerValues,
                                                   subsystemData.originalToReducedStateMapping, invertLowerValues);
            setGlobalValuesFromSubsystemVector<VT>(subsystemStates, subsystemData.operands.second, globalUpperValues,
                                                   subsystemData.originalToReducedStateMapping, false);
        }
    }

    void computeForSingleStateSubsystem(uint64_t state) {
        storm::utility::Extremum<Dir, ValueType> lowerValue, upperValue;
        bool const computeUpperValues = !globalUpperValues.empty();
        // If we minimize, choices that lead to states with infinite value can be ignored
        // (If we maximize, there are no such choices since the optimal value would be infinite as well.)
        auto const& infinityStates = totalRewardSpecification.terminalStateValues.front().second;
        bool const ignoreChoicesToInfinity = storm::solver::minimize(Dir) && totalRewardSpecification.terminalStateValues.front().first.isPositiveInfinity();
        for (auto const rowIndex : transitionProbabilityMatrix.getRowGroupIndices(state)) {
            if (ignoreChoicesToInfinity) {
                auto const row = transitionProbabilityMatrix.getRow(rowIndex);
                if (std::any_of(row.begin(), row.end(), [&infinityStates](auto const& entry) { return infinityStates.get(entry.getColumn()); })) {
                    continue;
                }
            }
            auto const reward = totalRewardSpecification.actionRewards.has_value() ? totalRewardSpecification.actionRewards.value()[rowIndex]
                                                                                   : storm::utility::zero<ValueType>();
            auto lowerRowValue = reward;
            auto upperRowValue = reward;
            std::optional<ValueType> diagonalEntry;
            for (auto const& entry : transitionProbabilityMatrix.getRow(rowIndex)) {
                if (state == entry.getColumn()) {
                    STORM_LOG_ASSERT(!diagonalEntry, "Already have diag entry.");
                    diagonalEntry = entry.getValue();
                } else {
                    lowerRowValue += entry.getValue() * globalLowerValues[entry.getColumn()];
                    if (computeUpperValues) {
                        upperRowValue += entry.getValue() * globalUpperValues[entry.getColumn()];
                    }
                }
            }
            if (diagonalEntry.has_value()) {
                if (diagonalEntry.value() == storm::utility::one<ValueType>()) {
                    // This choice is a self-loop and can therefore never be optimal
                    continue;
                }
                lowerRowValue /= storm::utility::one<ValueType>() - diagonalEntry.value();
                if (computeUpperValues) {
                    upperRowValue /= storm::utility::one<ValueType>() - diagonalEntry.value();
                }
            }
            lowerValue &= lowerRowValue;
            if (computeUpperValues) {
                upperValue &= upperRowValue;
            }
        }
        STORM_LOG_ASSERT(!lowerValue.empty() && !(computeUpperValues && upperValue.empty()),
                         "Failed to compute lower and/or upper value for state " << state << ".");
        globalLowerValues[state] = *lowerValue;
        if (computeUpperValues) {
            globalUpperValues[state] = *upperValue;
        }
    }

    void computeForSubsystemFpII(Algorithm const& alg, bool relative, ValueType const& precision, storm::storage::BitVector const& subsystemStates) {
        using VT = std::conditional_t<storm::NumberTraits<ValueType>::IsExact, double, ValueType>;  // VT is imprecise
        std::optional<storm::OptimizationDirection> constexpr optionalDir = Nondeterministic ? std::optional<storm::OptimizationDirection>(Dir) : std::nullopt;
        auto subsystemData = initializeSubsystemData<VT, false>(subsystemStates, alg.type);
        if (alg.type == AlgorithmType::FpRoundII) {
            auto invertVector = [](auto& vector) {
                for (auto& value : vector) {
                    value = -value;
                }
            };
            invertVector(subsystemData.operands.first);
            invertVector(subsystemData.offsets.first);
        }

        auto viOp = std::make_shared<storm::solver::helper::ValueIterationOperator<VT, !Nondeterministic>>();
        viOp->setMatrixBackwards(subsystemData.transitions);
        storm::solver::helper::IntervalIterationHelper<VT, !Nondeterministic> iiHelper(viOp);
        uint64_t numIterations = 0;
        if (alg.type == AlgorithmType::FpII) {
            iiHelper.II(subsystemData.operands, subsystemData.offsets, numIterations, relative, storm::utility::convertNumber<VT>(precision), optionalDir);
        } else if (alg.type == AlgorithmType::FpRoundII) {
            ScopedRoundingMode const roundUpwards(FE_UPWARD);
            iiHelper.roundII(subsystemData.operands, subsystemData.offsets, numIterations, relative, storm::utility::convertNumber<VT>(precision),
                             storm::utility::convertNumber<VT>(alg.gamma), storm::utility::convertNumber<VT>(alg.delta), optionalDir);
        } else {
            STORM_LOG_ASSERT(alg.type == AlgorithmType::FpSmoothII, "Unsupported algorithm.");
            iiHelper.smoothII(subsystemData.operands, subsystemData.offsets, numIterations, relative, storm::utility::convertNumber<VT>(precision),
                              storm::utility::convertNumber<VT>(alg.gamma), storm::utility::convertNumber<VT>(alg.delta), optionalDir);
        }
        setGlobalValuesFromSubsystem<VT>(subsystemStates, subsystemData, alg.type == AlgorithmType::FpRoundII);
    }

    void computeForSubsystemExPI(storm::storage::BitVector const& subsystemStates) {
        using VT = std::conditional_t<storm::NumberTraits<ValueType>::IsExact, ValueType, storm::RationalNumber>;  // VT is exact

        auto subsystemData = initializeSubsystemData<VT, true>(subsystemStates, AlgorithmType::ExPI);

        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ViToPi);
        env.solver().setLinearEquationSolverType(storm::solver::EquationSolverType::Eigen);
        env.solver().eigen().setMethod(solver::EigenLinearEquationSolverMethod::SparseLU);
        env.solver().setForceExact(true);

        if constexpr (Nondeterministic) {
            auto solverFactory = storm::solver::GeneralMinMaxLinearEquationSolverFactory<VT>();
            auto req = solverFactory.getRequirements(env, true, false, Dir, false, false);
            std::vector<uint64_t> initSched;
            if (req.validInitialScheduler()) {
                storm::storage::SparseMatrix<VT> const& submatrix = subsystemData.transitions;
                auto exitStates = submatrix.getRowGroupFilter(subsystemData.exitChoices, false);
                storm::storage::Scheduler<VT> validScheduler(exitStates.size());
                storm::utility::graph::computeSchedulerProb1E(storm::storage::BitVector(exitStates.size(), true), submatrix, submatrix.transpose(true),
                                                              ~exitStates, exitStates, validScheduler);

                // Extract the relevant parts of the scheduler for the solver.
                initSched.reserve(exitStates.size());
                for (uint64_t i = 0; i < exitStates.size(); ++i) {
                    if (exitStates.get(i)) {
                        initSched.push_back(validScheduler.getChoice(i).getDeterministicChoice());
                    } else {
                        auto const groupStart = submatrix.getRowGroupIndices()[i];
                        auto globalChoice = subsystemData.exitChoices.getNextSetIndex(groupStart);
                        STORM_LOG_ASSERT(globalChoice < submatrix.getRowGroupIndices()[i + 1], "No valid choice for exit state " << i << ".");
                        initSched.push_back(globalChoice - groupStart);
                    }
                }
                req.clearValidInitialScheduler();
            }
            STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UnmetRequirementException,
                            "Unmet requirements for MinMax linear equation solver: " << req.getEnabledRequirementsAsString() << ".");
            auto solver = solverFactory.create(env, std::move(subsystemData.transitions));
            solver->setHasUniqueSolution(true);  // TODO: Has no end components?
            if (!initSched.empty()) {
                solver->setInitialScheduler(std::move(initSched));
            }
            solver->setRequirementsChecked(true);
            solver->solveEquations(env, Dir, subsystemData.operands, subsystemData.offsets);
        } else {
            auto solverFactory = storm::solver::GeneralLinearEquationSolverFactory<VT>();
            if (solverFactory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem) {
                subsystemData.transitions = storm::storage::SparseMatrix<VT>(subsystemData.transitions, true);  // insert diag entries
                subsystemData.transitions.convertToEquationSystem();  // Note: Invalidates uses of this matrix as 'transitions'
            }
            auto req = solverFactory.getRequirements(env);
            STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UnmetRequirementException,
                            "Unmet requirements for linear equation solver: " << req.getEnabledRequirementsAsString() << ".");
            auto solver = solverFactory.create(env, std::move(subsystemData.transitions));
            solver->solveEquations(env, subsystemData.operands, subsystemData.offsets);
        }
        setGlobalValuesFromSubsystem<VT>(subsystemStates, subsystemData, false);
    }

    void computeForSubsystem(Algorithm const& alg, bool relative, ValueType const& precision, storm::storage::BitVector const& subsystemStates) {
        switch (alg.type) {
            case AlgorithmType::FpII:
            case AlgorithmType::FpSmoothII:
            case AlgorithmType::FpRoundII:
                computeForSubsystemFpII(alg, relative, precision, subsystemStates);
                break;
            case AlgorithmType::ExPI:
                computeForSubsystemExPI(subsystemStates);
                break;
            default:
                STORM_LOG_ASSERT(false, "Unsupported algorithm.");
        }
    }

    void computeTopological(Algorithm const& alg, bool relative, ValueType const& precision, storm::storage::BitVector const& nonTerminalStates) {
        STORM_LOG_TRACE("Creating SCC decomposition.");
        storm::utility::Stopwatch sccSw(true);
        storm::storage::StronglyConnectedComponentDecomposition<ValueType> const sccDecomposition(
            transitionProbabilityMatrix,
            storm::storage::StronglyConnectedComponentDecompositionOptions().forceTopologicalSort().computeSccDepths().subsystem(nonTerminalStates));
        uint64_t const longestSccChainSize = sccDecomposition.getMaxSccDepth() + 1;
        sccSw.stop();
        STORM_LOG_INFO("SCC decomposition computed in "
                       << sccSw << ". Found " << sccDecomposition.size() << " SCC(s) containing a total of " << nonTerminalStates.getNumberOfSetBits()
                       << " states. Average SCC size is "
                       << static_cast<double>(transitionProbabilityMatrix.getRowGroupCount()) / static_cast<double>(sccDecomposition.size())
                       << ". Longest SCC chain size is " << longestSccChainSize << ".");
        if (sccDecomposition.size() == 1) {
            // Handle the case where there is just one large scc
            computeForSubsystem(alg, relative, precision, nonTerminalStates);
            return;
        }
        // At this point, solve each SCC individually
        storm::storage::BitVector sccAsBitvector(transitionProbabilityMatrix.getRowGroupCount(), false);  // allocate memory outside of loop
        for (uint64_t sccIndex = 0; sccIndex < sccDecomposition.size(); ++sccIndex) {
            auto const& scc = sccDecomposition.getBlock(sccIndex);
            if (scc.size() == 1) {
                // Directly solve single-state SCCs
                computeForSingleStateSubsystem(*scc.begin());
                continue;
            }
            sccAsBitvector.set(scc.begin(), scc.end());
            ValueType const sccPrecision =
                precision / storm::utility::convertNumber<ValueType, uint64_t>(longestSccChainSize - sccDecomposition.getSccDepth(sccIndex));
            computeForSubsystem(alg, relative, sccPrecision, sccAsBitvector);
            sccAsBitvector.clear();
        }
    }

    storm::CertificateModelCheckerEnvironment const& certificateEnv;
    storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix;
    TotalRewardSpecification<ValueType> const& totalRewardSpecification;
    // lower/upper values for each state. If the computation is exact, upper values are not used and remain empty.
    std::vector<ValueType> globalLowerValues, globalUpperValues;
};  // namespace storm::modelchecker

template<storm::OptimizationDirection Dir, typename ValueType, typename VectorValueType>
storm::storage::BitVector computeInductiveChoices(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
                                                  std::vector<VectorValueType> const& valueVector, storm::storage::BitVector const& relevantStates,
                                                  storm::OptionalRef<std::vector<ValueType> const> rewardVector = {}) {
    storm::storage::BitVector inductiveChoices(transitionProbabilityMatrix.getRowCount(), false);
    bool warnIfEmpty = true;
    for (auto state : relevantStates) {
        bool stateHasInductiveChoice = false;
        for (auto choice : transitionProbabilityMatrix.getRowGroupIndices(state)) {
            VectorValueType const choiceValue = detail::multiplyRowWithVectorAddNumber(
                transitionProbabilityMatrix, choice, valueVector, rewardVector.has_value() ? rewardVector->at(choice) : storm::utility::zero<ValueType>());
            if (storm::solver::maximize(Dir) ? valueVector[state] <= choiceValue : choiceValue <= valueVector[state]) {
                inductiveChoices.set(choice);
                stateHasInductiveChoice = true;
            }
        }
        if (!stateHasInductiveChoice) {
            STORM_LOG_WARN_COND(!warnIfEmpty, "State " << state << " has no inductive choice. The certificate might not be valid");
            warnIfEmpty = false;
        }
    }
    return inductiveChoices;
}

template<typename ValueType, storm::OptimizationDirection Dir>
std::vector<RankingType> computeDistanceRanking(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
                                                storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                storm::storage::BitVector const& targetStates,
                                                storm::OptionalRef<storm::storage::BitVector const> constraintStates,
                                                std::optional<storm::storage::BitVector> const& choiceConstraint = std::nullopt) {
    auto const& rowGroupIndices = transitionProbabilityMatrix.getRowGroupIndices();

    std::vector<RankingType> ranks(targetStates.size(), InfRank);
    storm::utility::vector::setVectorValues<RankingType>(ranks, targetStates, 0);
    std::vector<uint64_t> stateQueue(targetStates.begin(), targetStates.end());
    while (!stateQueue.empty()) {
        auto const state = stateQueue.back();
        stateQueue.pop_back();
        for (auto const& preEntry : backwardTransitions.getRow(state)) {
            auto const preState = preEntry.getColumn();
            if (ranks[preState] != InfRank) {
                continue;
            }
            if (constraintStates.has_value() && !constraintStates->get(preState)) {
                continue;
            }
            storm::utility::Extremum<Dir, RankingType> newRankMinusOne;
            // Iterate over all (selected) choices of the pre-state
            uint64_t currentChoice = rowGroupIndices[preState];
            if (choiceConstraint.has_value()) {
                currentChoice = choiceConstraint->getNextSetIndex(currentChoice);
            }
            auto nextChoice = [&choiceConstraint](auto c) { return choiceConstraint.has_value() ? choiceConstraint->getNextSetIndex(c + 1) : c + 1; };
            for (auto const endChoice = rowGroupIndices[preState + 1]; currentChoice < endChoice; currentChoice = nextChoice(currentChoice)) {
                // Compute current rank for this choice
                storm::utility::Minimum<RankingType> choiceRank;
                for (auto const& entry : transitionProbabilityMatrix.getRow(currentChoice)) {
                    if (storm::utility::isZero(entry.getValue())) {
                        continue;
                    }
                    choiceRank &= ranks[entry.getColumn()];
                }
                STORM_LOG_ASSERT(!choiceRank.empty(),
                                 "Ranking operator failed to compute rank for state " << state << " since the row " << currentChoice << " is empty.");
                newRankMinusOne &= *choiceRank;
            }
            if (!newRankMinusOne.empty() && *newRankMinusOne != InfRank) {
                ranks[preState] = *newRankMinusOne + 1;
                stateQueue.push_back(preState);
            }
        }
    }
    return ranks;
}

template<typename ValueType, storm::OptimizationDirection Dir>
std::vector<RankingType> computeModifiedDistanceRanking(storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
                                                        storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                        storm::storage::BitVector const& probLess1States) {
    std::vector<RankingType> ranks(probLess1States.size(), InfRank);
    storm::utility::vector::setVectorValues<RankingType>(ranks, probLess1States, 0);

    // Apply the modified ranking operator until no more states are updated
    while (true) {
        bool updated = false;
        for (auto state : probLess1States) {
            storm::utility::Extremum<Dir, RankingType> newStateRank;
            for (auto const currentChoice : transitionProbabilityMatrix.getRowGroupIndices(state)) {
                storm::utility::Minimum<RankingType> minSuccessorRank;
                bool allSuccessorsEqual = true;
                for (auto const& entry : transitionProbabilityMatrix.getRow(currentChoice)) {
                    if (storm::utility::isZero(entry.getValue())) {
                        continue;
                    }
                    if (!minSuccessorRank.empty() && ranks[entry.getColumn()] != *minSuccessorRank) {
                        allSuccessorsEqual = false;
                    }
                    minSuccessorRank &= ranks[entry.getColumn()];
                }
                STORM_LOG_ASSERT(!minSuccessorRank.empty(),
                                 "Ranking operator failed to compute rank for state " << state << " since the row " << currentChoice << " is empty.");
                auto choiceRank = *minSuccessorRank;
                if (choiceRank != InfRank && !allSuccessorsEqual) {
                    ++choiceRank;
                }
                newStateRank &= choiceRank;
            }
            STORM_LOG_ASSERT(!newStateRank.empty(), "Failed to compute rank for state " << state << ".");
            if (ranks[state] != *newStateRank) {
                STORM_LOG_ASSERT(ranks[state] <= *newStateRank, "Ranking operator did not generate an increasing sequence for state " << state << ".");
                ranks[state] = *newStateRank;
                updated = true;
            }
        }
        if (!updated) {
            return ranks;
        }
    }
}

template<typename ValueType>
struct ProbabilityCertificateData {
    std::vector<ValueType> lowerValues, upperValues;
    std::vector<RankingType> ranks;
};

template<typename ValueType, bool Nondeterministic, storm::OptimizationDirection Dir = storm::OptimizationDirection::Minimize>
ProbabilityCertificateData<ValueType> computeReachabilityProbabilityCertificateData(storm::Environment const& env,
                                                                                    storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
                                                                                    storm::storage::BitVector const& targetStates,
                                                                                    storm::OptionalRef<storm::storage::BitVector const> constraintStates) {
    std::optional<storm::OptimizationDirection> constexpr optionalDir = Nondeterministic ? std::optional<storm::OptimizationDirection>(Dir) : std::nullopt;
    storm::storage::BackwardTransitionCache<ValueType> backwardTransitionCache(transitionProbabilityMatrix);
    auto toRewardData = ReachabilityProbabilityToRewardTransformer<ValueType>(transitionProbabilityMatrix, backwardTransitionCache)
                            .transform(optionalDir, targetStates, constraintStates);
    STORM_LOG_ASSERT(toRewardData.terminalStateValues.size() == 1 && toRewardData.terminalStateValues.front().first.isOne(),
                     "Expected exactly one terminal state value with value 1.");
    LowerUpperValueCertificateComputer<ValueType, Nondeterministic, Dir> computer(env.modelchecker().certificate(), transitionProbabilityMatrix, toRewardData);

    auto [lowerValues, upperValues] = computer.compute(env.solver().minMax().getRelativeTerminationCriterion(),
                                                       storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()));
    std::optional<storm::storage::BitVector> inductiveChoices;
    if (optionalDir.has_value() && optionalDir.value() == storm::OptimizationDirection::Maximize) {
        auto relevantStates = ~targetStates;
        if (constraintStates.has_value()) {
            relevantStates &= *constraintStates;
        }
        inductiveChoices = computeInductiveChoices<storm::OptimizationDirection::Maximize, ValueType>(transitionProbabilityMatrix, lowerValues, relevantStates);
    }
    auto ranks = computeDistanceRanking<ValueType, storm::solver::invert(Dir)>(transitionProbabilityMatrix, backwardTransitionCache.get(), targetStates,
                                                                               constraintStates, inductiveChoices);
    return {std::move(lowerValues), std::move(upperValues), std::move(ranks)};
}

template<typename ValueType>
ProbabilityCertificateData<ValueType> computeReachabilityProbabilityCertificateData(storm::Environment const& env,
                                                                                    std::optional<storm::OptimizationDirection> dir,
                                                                                    storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
                                                                                    storm::storage::BitVector const& targetStates,
                                                                                    storm::OptionalRef<storm::storage::BitVector const> constraintStates) {
    ProbabilityCertificateData<ValueType> data;
    if (dir.has_value()) {
        if (storm::solver::maximize(*dir)) {
            data = computeReachabilityProbabilityCertificateData<ValueType, true, storm::OptimizationDirection::Maximize>(env, transitionProbabilityMatrix,
                                                                                                                          targetStates, constraintStates);
        } else {
            data = computeReachabilityProbabilityCertificateData<ValueType, true, storm::OptimizationDirection::Minimize>(env, transitionProbabilityMatrix,
                                                                                                                          targetStates, constraintStates);
        }
    } else {
        data = computeReachabilityProbabilityCertificateData<ValueType, false>(env, transitionProbabilityMatrix, targetStates, constraintStates);
    }
    return data;
}

template<typename ValueType>
std::unique_ptr<ReachabilityProbabilityCertificate<ValueType>> computeReachabilityProbabilityCertificate(
    storm::Environment const& env, std::optional<storm::OptimizationDirection> dir, storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
    storm::storage::BitVector targetStates, std::optional<storm::storage::BitVector> constraintStates, std::string targetLabel,
    std::optional<std::string> constraintLabel) {
    ProbabilityCertificateData<ValueType> data;
    std::unique_ptr<ReachabilityProbabilityCertificate<ValueType>> result;
    if (constraintStates.has_value()) {
        data = computeReachabilityProbabilityCertificateData<ValueType>(env, dir, transitionProbabilityMatrix, targetStates, constraintStates.value());
        result = std::make_unique<ReachabilityProbabilityCertificate<ValueType>>(dir, std::move(targetStates), std::move(constraintStates.value()),
                                                                                 std::move(targetLabel), constraintLabel.value_or("constraint"));
    } else {
        data = computeReachabilityProbabilityCertificateData<ValueType>(env, dir, transitionProbabilityMatrix, targetStates, storm::NullRef);
        result = std::make_unique<ReachabilityProbabilityCertificate<ValueType>>(dir, std::move(targetStates), std::move(targetLabel));
    }
    result->setLowerBoundsCertificate(std::move(data.lowerValues), std::move(data.ranks));
    result->setUpperBoundsCertificate(std::move(data.upperValues));
    return result;
}

template<typename ValueType>
struct RewardCertificateData {
    std::vector<storm::utility::ExtendedValueType<ValueType>> lowerValues, upperValues;
    std::vector<RankingType> lowerRanks, upperRanks;
};

template<typename ValueType, bool Nondeterministic, storm::OptimizationDirection Dir = storm::OptimizationDirection::Minimize>
RewardCertificateData<ValueType> computeReachabilityRewardCertificateData(storm::Environment const& env,
                                                                          storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
                                                                          storm::storage::BitVector const& targetStates,
                                                                          std::vector<ValueType> const& stateActionRewardVector) {
    std::optional<storm::OptimizationDirection> constexpr optionalDir = Nondeterministic ? std::optional<storm::OptimizationDirection>(Dir) : std::nullopt;
    storm::storage::BackwardTransitionCache<ValueType> backwardTransitionCache(transitionProbabilityMatrix);
    auto toRewardData = ReachabilityRewardTransformer<ValueType>(transitionProbabilityMatrix, backwardTransitionCache)
                            .transform(optionalDir, targetStates, stateActionRewardVector);
    STORM_LOG_ASSERT(toRewardData.terminalStateValues.size() == 1 && toRewardData.terminalStateValues.front().first.isPositiveInfinity(),
                     "Expected exactly one terminal state value with value +infinity.");
    auto const& infinityStates = toRewardData.terminalStateValues.front().second;
    LowerUpperValueCertificateComputer<ValueType, Nondeterministic, Dir> computer(env.modelchecker().certificate(), transitionProbabilityMatrix, toRewardData);

    std::vector<storm::utility::ExtendedValueType<ValueType>> extendedLowerValues, extendedUpperValues;
    {
        auto [lowerValues, upperValues] = computer.compute(env.solver().minMax().getRelativeTerminationCriterion(),
                                                           storm::utility::convertNumber<ValueType>(env.solver().minMax().getPrecision()));
        extendedLowerValues.reserve(lowerValues.size());
        extendedUpperValues.reserve(upperValues.size());
        for (uint64_t i = 0; i < lowerValues.size(); ++i) {
            if (infinityStates.get(i)) {
                extendedLowerValues.push_back(storm::utility::positiveInfinity<ValueType>());
                extendedUpperValues.push_back(storm::utility::positiveInfinity<ValueType>());
            } else {
                extendedLowerValues.emplace_back(std::move(lowerValues[i]));
                extendedUpperValues.emplace_back(std::move(upperValues[i]));
            }
        }
    }

    std::optional<storm::storage::BitVector> inductiveChoices;
    if (optionalDir.has_value() && optionalDir.value() == storm::OptimizationDirection::Minimize) {
        inductiveChoices = computeInductiveChoices<storm::OptimizationDirection::Minimize, ValueType>(transitionProbabilityMatrix, extendedUpperValues,
                                                                                                      ~targetStates, stateActionRewardVector);
    }
    auto upperRanks =
        computeDistanceRanking<ValueType, Dir>(transitionProbabilityMatrix, backwardTransitionCache.get(), targetStates, storm::NullRef, inductiveChoices);
    // The lower bound ranks consider all choices (not only the inductive ones), see Proposition 7 of https://doi.org/10.1007/978-3-031-90653-4_7.
    auto lowerRanks =
        computeModifiedDistanceRanking<ValueType, storm::solver::invert(Dir)>(transitionProbabilityMatrix, backwardTransitionCache.get(), infinityStates);

    return {std::move(extendedLowerValues), std::move(extendedUpperValues), std::move(lowerRanks), std::move(upperRanks)};
}

template<typename ValueType>
std::unique_ptr<ReachabilityRewardCertificate<ValueType>> computeReachabilityRewardCertificate(
    storm::Environment const& env, std::optional<storm::OptimizationDirection> dir, storm::storage::SparseMatrix<ValueType> const& transitionProbabilityMatrix,
    storm::storage::BitVector targetStates, std::vector<ValueType> stateActionRewardVector, std::string targetLabel, std::string rewardModelName) {
    RewardCertificateData<ValueType> data;
    if (dir.has_value()) {
        if (storm::solver::maximize(*dir)) {
            data = computeReachabilityRewardCertificateData<ValueType, true, storm::OptimizationDirection::Maximize>(env, transitionProbabilityMatrix,
                                                                                                                     targetStates, stateActionRewardVector);
        } else {
            data = computeReachabilityRewardCertificateData<ValueType, true, storm::OptimizationDirection::Minimize>(env, transitionProbabilityMatrix,
                                                                                                                     targetStates, stateActionRewardVector);
        }
    } else {
        data = computeReachabilityRewardCertificateData<ValueType, false>(env, transitionProbabilityMatrix, targetStates, stateActionRewardVector);
    }
    auto result = std::make_unique<ReachabilityRewardCertificate<ValueType>>(dir, std::move(targetStates), std::move(stateActionRewardVector),
                                                                             std::move(targetLabel), std::move(rewardModelName));
    result->setLowerBoundsCertificate(std::move(data.lowerValues), std::move(data.lowerRanks));
    result->setUpperBoundsCertificate(std::move(data.upperValues), std::move(data.upperRanks));
    return result;
}

template std::unique_ptr<ReachabilityProbabilityCertificate<double>> computeReachabilityProbabilityCertificate<double>(
    storm::Environment const& env, std::optional<storm::OptimizationDirection> dir, storm::storage::SparseMatrix<double> const& transitionProbabilityMatrix,
    storm::storage::BitVector targetStates, std::optional<storm::storage::BitVector> constraintStates, std::string targetLabel,
    std::optional<std::string> constraintLabel);
template std::unique_ptr<ReachabilityProbabilityCertificate<storm::RationalNumber>> computeReachabilityProbabilityCertificate<storm::RationalNumber>(
    storm::Environment const& env, std::optional<storm::OptimizationDirection> dir,
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionProbabilityMatrix, storm::storage::BitVector targetStates,
    std::optional<storm::storage::BitVector> constraintStates, std::string targetLabel, std::optional<std::string> constraintLabel);

template std::unique_ptr<ReachabilityRewardCertificate<double>> computeReachabilityRewardCertificate<double>(
    storm::Environment const& env, std::optional<storm::OptimizationDirection> dir, storm::storage::SparseMatrix<double> const& transitionProbabilityMatrix,
    storm::storage::BitVector targetStates, std::vector<double> stateActionRewardVector, std::string targetLabel, std::string rewardModelName);

template std::unique_ptr<ReachabilityRewardCertificate<storm::RationalNumber>> computeReachabilityRewardCertificate<storm::RationalNumber>(
    storm::Environment const& env, std::optional<storm::OptimizationDirection> dir,
    storm::storage::SparseMatrix<storm::RationalNumber> const& transitionProbabilityMatrix, storm::storage::BitVector targetStates,
    std::vector<storm::RationalNumber> stateActionRewardVector, std::string targetLabel, std::string rewardModelName);

}  // namespace storm::modelchecker