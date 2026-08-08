#include "storm/modelchecker/multiobjective/pcaa/RewardBoundedMdpPcaaWeightVectorChecker.h"

#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/NativeSolverEnvironment.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UncheckedRequirementException.h"
#include "storm/io/export.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectiveRewardAnalysis.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/solver/LinearEquationSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/utility/ProgressMeasurement.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/macros.h"
#include "storm/utility/vector.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

template<class SparseMdpModelType>
RewardBoundedMdpPcaaWeightVectorChecker<SparseMdpModelType>::RewardBoundedMdpPcaaWeightVectorChecker(
    preprocessing::SparseMultiObjectivePreprocessorResult<SparseMdpModelType> const& preprocessorResult)
    : PcaaWeightVectorChecker<SparseMdpModelType>(preprocessorResult.objectives),
      swAll(true),
      rewardUnfolding(*preprocessorResult.preprocessedModel, preprocessorResult.objectives) {
    auto rewardAnalysis = preprocessing::SparseMultiObjectiveRewardAnalysis<SparseMdpModelType>::analyze(preprocessorResult);
    STORM_LOG_THROW(rewardAnalysis.rewardFinitenessType == preprocessing::RewardFinitenessType::AllFinite, storm::exceptions::NotSupportedException,
                    "There is a scheduler that yields infinite reward for one  objective. This is not supported.");
    STORM_LOG_THROW(preprocessorResult.preprocessedModel->getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::NotSupportedException,
                    "The model has multiple initial states.");

    // Update the objective bounds with what the reward unfolding can compute
    for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        auto& obj = this->objectives[objIndex];
        obj.lowerResultBound = rewardUnfolding.getLowerObjectiveBound(objIndex);
        obj.upperResultBound = rewardUnfolding.getUpperObjectiveBound(objIndex);
        if (!obj.upperResultBound) {
            preprocessing::SparseMultiObjectiveRewardAnalysis<SparseMdpModelType>::computeUpperResultBound(
                *preprocessorResult.preprocessedModel, obj, preprocessorResult.preprocessedModel->getBackwardTransitions());
        }
    }

    numCheckedEpochs = 0;
    numChecks = 0;
}

template<class SparseMdpModelType>
RewardBoundedMdpPcaaWeightVectorChecker<SparseMdpModelType>::~RewardBoundedMdpPcaaWeightVectorChecker() {
    swAll.stop();
    STORM_LOG_STATISTICS("--------------------------------------------------\n");
    STORM_LOG_STATISTICS("Statistics:\n");
    STORM_LOG_STATISTICS("--------------------------------------------------\n");
    STORM_LOG_STATISTICS("           #checked weight vectors: " << numChecks << ".\n");
    STORM_LOG_STATISTICS("           #checked epochs overall: " << numCheckedEpochs << ".\n");
    STORM_LOG_STATISTICS("# checked epochs per weight vector: " << numCheckedEpochs / numChecks << ".\n");
    STORM_LOG_STATISTICS("                      overall Time: " << swAll << ".\n");
    STORM_LOG_STATISTICS("         Epoch Model building time: " << swEpochModelBuild << ".\n");
    STORM_LOG_STATISTICS("         Epoch Model checking time: " << swEpochModelAnalysis << ".\n");
    STORM_LOG_STATISTICS("--------------------------------------------------\n");
}

template<class SparseMdpModelType>
void RewardBoundedMdpPcaaWeightVectorChecker<SparseMdpModelType>::check(Environment const& env, std::vector<ValueType> weightVector) {
    STORM_LOG_THROW(std::any_of(weightVector.begin(), weightVector.end(), [](auto const& w_i) { return !storm::utility::isZero(w_i); }),
                    storm::exceptions::InvalidOperationException, "Weight vector must not be the zero vector.");
    ++numChecks;
    STORM_LOG_INFO("Analyzing weight vector #" << numChecks << ": " << storm::utility::vector::toString(weightVector));

    // Normalize weights so the vector has length 1.
    // This is necessary for ensuring the required accuracy, i.e. distance between halfspace induced by weightedSum and weightvector and achievable point.
    ValueType const inputWeightVectorLength = storm::utility::sqrt(storm::utility::vector::dotProduct(weightVector, weightVector));
    storm::utility::vector::scaleVectorInPlace<ValueType, ValueType>(weightVector, storm::utility::one<ValueType>() / (inputWeightVectorLength));

    // In case we want to export the cdf, we will collect the corresponding data
    std::vector<std::vector<ValueType>> cdfData;

    auto initEpoch = rewardUnfolding.getStartEpoch();
    auto epochOrder = rewardUnfolding.getEpochComputationOrder(initEpoch);
    EpochCheckingData cachedData;
    ValueType const globalPrecision = this->getWeightedPrecision() / storm::utility::convertNumber<ValueType>(4.0);  // 4=2*2:
    // We divide the precision by 2 to distribute the approximation error over weighted (minmax) optimization and linear equation solving
    // We again divide by 2 to account for the fact that we only compute a mid-point p such that the real value is in [p - precision, p + precision].
    ValueType const epochPrecision = rewardUnfolding.getRequiredEpochModelPrecision(initEpoch, globalPrecision);
    Environment newEnv = env;
    newEnv.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(epochPrecision));
    newEnv.solver().setLinearEquationSolverPrecision(storm::utility::convertNumber<storm::RationalNumber>(epochPrecision));
    storm::utility::ProgressMeasurement progress("epochs");
    progress.setMaxCount(epochOrder.size());
    progress.startNewMeasurement(0);
    uint64_t numCheckedEpochs = 0;
    for (auto const& epoch : epochOrder) {
        computeEpochSolution(newEnv, epoch, weightVector, cachedData);
        if (storm::settings::getModule<storm::settings::modules::IOSettings>().isExportCdfSet() &&
            !rewardUnfolding.getEpochManager().hasBottomDimension(epoch)) {
            std::vector<ValueType> cdfEntry;
            for (uint64_t i = 0; i < rewardUnfolding.getEpochManager().getDimensionCount(); ++i) {
                uint64_t offset = rewardUnfolding.getDimension(i).boundType == helper::rewardbounded::DimensionBoundType::LowerBound ? 1 : 0;
                cdfEntry.push_back(storm::utility::convertNumber<ValueType>(rewardUnfolding.getEpochManager().getDimensionOfEpoch(epoch, i) + offset) *
                                   rewardUnfolding.getDimension(i).scalingFactor);
            }
            auto const& solution = rewardUnfolding.getInitialStateResult(epoch);
            auto solutionIt = solution.begin();
            ++solutionIt;
            cdfEntry.insert(cdfEntry.end(), solutionIt, solution.end());
            cdfData.push_back(std::move(cdfEntry));
        }
        ++numCheckedEpochs;
        progress.updateProgress(numCheckedEpochs);
        if (storm::utility::resources::isTerminate()) {
            break;
        }
    }

    if (storm::settings::getModule<storm::settings::modules::IOSettings>().isExportCdfSet()) {
        std::vector<std::string> headers;
        for (uint64_t i = 0; i < rewardUnfolding.getEpochManager().getDimensionCount(); ++i) {
            headers.push_back("obj" + std::to_string(rewardUnfolding.getDimension(i).objectiveIndex) + ":" +
                              rewardUnfolding.getDimension(i).formula->toString());
        }
        for (uint64_t i = 0; i < this->objectives.size(); ++i) {
            headers.push_back("obj" + std::to_string(i));
        }
        storm::io::exportDataToCSVFile<ValueType, ValueType, std::string>(
            storm::settings::getModule<storm::settings::modules::IOSettings>().getExportCdfDirectory() + "cdf" + std::to_string(numChecks) + ".csv", cdfData,
            weightVector, headers);
    }
    auto solution = rewardUnfolding.getInitialStateResult(initEpoch);
    ValueType const precisionOffset = env.solver().isForceExact() ? storm::utility::zero<ValueType>() : globalPrecision;
    // compute a point p that is known to be achievable and a value v with sup_{r is achievable} w*r <= v (when assuming only maximizing objectives)
    // We can use the facts that the computed solutions for the individual objectives are within precisionOffset of the actual values
    weightedSum = solution[0] + precisionOffset;  // upper bound on sup_{r is achievable} w*r
    // reverse the normalization of the weight vector for the returned optimal weighted sum.
    weightedSum.value() *= inputWeightVectorLength;
    achievablePoint.emplace(this->objectives.size());
    for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        auto& p_i = achievablePoint.value()[objIndex];
        bool const isMax = storm::solver::maximize(this->objectives[objIndex].formula->getOptimalityType());
        p_i = solution[objIndex + 1] + (isMax ? -precisionOffset : precisionOffset);
    }
}

template<class SparseMdpModelType>
void RewardBoundedMdpPcaaWeightVectorChecker<SparseMdpModelType>::computeEpochSolution(
    Environment const& env, typename helper::rewardbounded::MultiDimensionalRewardUnfolding<ValueType, false>::Epoch const& epoch,
    std::vector<ValueType> const& weightVector, EpochCheckingData& cachedData) {
    ++numCheckedEpochs;
    swEpochModelBuild.start();
    auto& epochModel = rewardUnfolding.setCurrentEpoch(epoch);
    swEpochModelBuild.stop();
    swEpochModelAnalysis.start();
    std::vector<typename helper::rewardbounded::MultiDimensionalRewardUnfolding<ValueType, false>::SolutionType> result;
    result.reserve(epochModel.epochInStates.getNumberOfSetBits());
    uint64_t solutionSize = this->objectives.size() + 1;

    // If the epoch matrix is empty we do not need to solve linear equation systems
    if (epochModel.epochMatrix.getEntryCount() == 0) {
        std::vector<ValueType> weights = weightVector;
        for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
            if (storm::solver::minimize(this->objectives[objIndex].formula->getOptimalityType())) {
                weights[objIndex] *= -storm::utility::one<ValueType>();
            }
        }

        auto stepSolutionIt = epochModel.stepSolutions.begin();
        auto stepChoiceIt = epochModel.stepChoices.begin();
        for (auto state : epochModel.epochInStates) {
            // Obtain the best choice for this state according to the weighted combination of objectives
            ValueType bestValue;
            uint64_t bestChoice = std::numeric_limits<uint64_t>::max();
            auto bestChoiceStepSolutionIt = epochModel.stepSolutions.end();
            uint64_t lastChoice = epochModel.epochMatrix.getRowGroupIndices()[state + 1];
            bool firstChoice = true;
            for (uint64_t choice = epochModel.epochMatrix.getRowGroupIndices()[state]; choice < lastChoice; ++choice) {
                ValueType choiceValue = storm::utility::zero<ValueType>();
                // Obtain the (weighted) objective rewards
                for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
                    if (epochModel.objectiveRewardFilter[objIndex].get(choice)) {
                        choiceValue += weights[objIndex] * epochModel.objectiveRewards[objIndex][choice];
                    }
                }

                // Obtain the step solution if this is a step choice
                while (*stepChoiceIt < choice) {
                    ++stepChoiceIt;
                    ++stepSolutionIt;
                }
                if (*stepChoiceIt == choice) {
                    choiceValue += stepSolutionIt->front();
                    // Check if this choice is better
                    if (firstChoice || choiceValue > bestValue) {
                        bestValue = std::move(choiceValue);
                        bestChoice = choice;
                        bestChoiceStepSolutionIt = stepSolutionIt;
                    }
                } else if (firstChoice || choiceValue > bestValue) {
                    bestValue = std::move(choiceValue);
                    bestChoice = choice;
                    bestChoiceStepSolutionIt = epochModel.stepSolutions.end();
                }
                firstChoice = false;
            }

            // Insert the solution w.r.t. this choice
            result.emplace_back();
            result.back().reserve(solutionSize);
            result.back().push_back(std::move(bestValue));

            if (bestChoiceStepSolutionIt != epochModel.stepSolutions.end()) {
                for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
                    if (epochModel.objectiveRewardFilter[objIndex].get(bestChoice)) {
                        result.back().push_back((epochModel.objectiveRewards[objIndex][bestChoice] + (*bestChoiceStepSolutionIt)[objIndex + 1]));
                    } else {
                        result.back().push_back((*bestChoiceStepSolutionIt)[objIndex + 1]);
                    }
                }
            } else {
                for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
                    if (epochModel.objectiveRewardFilter[objIndex].get(bestChoice)) {
                        result.back().push_back((epochModel.objectiveRewards[objIndex][bestChoice]));
                    } else {
                        result.back().push_back(storm::utility::zero<ValueType>());
                    }
                }
            }
        }
    } else {
        updateCachedData(env, epochModel, cachedData, weightVector);

        // Formulate a min-max equation system max(A*x+b)=x for the weighted sum of the objectives
        assert(cachedData.bMinMax.capacity() >= epochModel.epochMatrix.getRowCount());
        assert(cachedData.xMinMax.size() == epochModel.epochMatrix.getRowGroupCount());
        cachedData.bMinMax.assign(epochModel.epochMatrix.getRowCount(), storm::utility::zero<ValueType>());
        for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
            ValueType weight =
                storm::solver::minimize(this->objectives[objIndex].formula->getOptimalityType()) ? -weightVector[objIndex] : weightVector[objIndex];
            if (!storm::utility::isZero(weight)) {
                std::vector<ValueType> const& objectiveReward = epochModel.objectiveRewards[objIndex];
                for (auto choice : epochModel.objectiveRewardFilter[objIndex]) {
                    cachedData.bMinMax[choice] += weight * objectiveReward[choice];
                }
            }
        }
        auto stepSolutionIt = epochModel.stepSolutions.begin();
        for (auto choice : epochModel.stepChoices) {
            cachedData.bMinMax[choice] += stepSolutionIt->front();
            ++stepSolutionIt;
        }

        // Invoke the min max solver
        cachedData.minMaxSolver->solveEquations(env, cachedData.xMinMax, cachedData.bMinMax);
        for (auto state : epochModel.epochInStates) {
            result.emplace_back();
            result.back().reserve(solutionSize);
            result.back().push_back(cachedData.xMinMax[state]);
        }

        // Check whether the linear equation solver needs to be updated
        auto const& choices = cachedData.minMaxSolver->getSchedulerChoices();
        if (cachedData.schedulerChoices != choices) {
            std::vector<uint64_t> choicesTmp = choices;
            cachedData.minMaxSolver->setInitialScheduler(std::move(choicesTmp));
            cachedData.schedulerChoices = choices;
            storm::solver::GeneralLinearEquationSolverFactory<ValueType> linEqSolverFactory;
            bool needEquationSystem = linEqSolverFactory.getEquationProblemFormat(env) == storm::solver::LinearEquationSolverProblemFormat::EquationSystem;
            storm::storage::SparseMatrix<ValueType> subMatrix = epochModel.epochMatrix.selectRowsFromRowGroups(choices, needEquationSystem);
            if (needEquationSystem) {
                subMatrix.convertToEquationSystem();
            }
            cachedData.linEqSolver = linEqSolverFactory.create(env, std::move(subMatrix));
            cachedData.linEqSolver->setCachingEnabled(true);
        }

        // Formulate for each objective the linear equation system induced by the performed choices
        assert(cachedData.bLinEq.size() == choices.size());
        for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
            auto const& obj = this->objectives[objIndex];
            std::vector<ValueType> const& objectiveReward = epochModel.objectiveRewards[objIndex];
            auto rowGroupIndexIt = epochModel.epochMatrix.getRowGroupIndices().begin();
            auto choiceIt = choices.begin();
            auto stepChoiceIt = epochModel.stepChoices.begin();
            auto stepSolutionIt = epochModel.stepSolutions.begin();
            std::vector<ValueType>& x = cachedData.xLinEq[objIndex];
            auto xIt = x.begin();
            for (auto& b_i : cachedData.bLinEq) {
                uint64_t i = *rowGroupIndexIt + *choiceIt;
                if (epochModel.objectiveRewardFilter[objIndex].get(i)) {
                    b_i = objectiveReward[i];
                } else {
                    b_i = storm::utility::zero<ValueType>();
                }
                while (*stepChoiceIt < i) {
                    ++stepChoiceIt;
                    ++stepSolutionIt;
                }
                if (i == *stepChoiceIt) {
                    b_i += (*stepSolutionIt)[objIndex + 1];
                    ++stepChoiceIt;
                    ++stepSolutionIt;
                }
                // We can already set x_i correctly if row i is empty.
                // Appearingly, some linear equation solvers struggle to converge otherwise.
                if (epochModel.epochMatrix.getRow(i).getNumberOfEntries() == 0) {
                    *xIt = b_i;
                }
                ++xIt;
                ++rowGroupIndexIt;
                ++choiceIt;
            }
            assert(x.size() == choices.size());
            auto req = cachedData.linEqSolver->getRequirements(env);
            cachedData.linEqSolver->clearBounds();
            if (obj.lowerResultBound) {
                req.clearLowerBounds();
                cachedData.linEqSolver->setLowerBound(*obj.lowerResultBound);
            }
            if (obj.upperResultBound) {
                cachedData.linEqSolver->setUpperBound(*obj.upperResultBound);
                req.clearUpperBounds();
            }
            STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                            "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
            cachedData.linEqSolver->solveEquations(env, x, cachedData.bLinEq);
            auto resultIt = result.begin();
            for (auto state : epochModel.epochInStates) {
                resultIt->push_back(x[state]);
                ++resultIt;
            }
        }
    }
    rewardUnfolding.setSolutionForCurrentEpoch(std::move(result));
    swEpochModelAnalysis.stop();
}

template<class SparseMdpModelType>
void RewardBoundedMdpPcaaWeightVectorChecker<SparseMdpModelType>::updateCachedData(Environment const& env,
                                                                                   helper::rewardbounded::EpochModel<ValueType, false> const& epochModel,
                                                                                   EpochCheckingData& cachedData, std::vector<ValueType> const& weightVector) {
    if (epochModel.epochMatrixChanged) {
        // Update the cached MinMaxSolver data
        cachedData.bMinMax.resize(epochModel.epochMatrix.getRowCount());
        cachedData.xMinMax.assign(epochModel.epochMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
        storm::solver::GeneralMinMaxLinearEquationSolverFactory<ValueType> minMaxSolverFactory;
        cachedData.minMaxSolver = minMaxSolverFactory.create(env, epochModel.epochMatrix);
        cachedData.minMaxSolver->setHasUniqueSolution();
        cachedData.minMaxSolver->setHasNoEndComponents();
        cachedData.minMaxSolver->setTrackScheduler(true);
        cachedData.minMaxSolver->setCachingEnabled(true);
        auto req = cachedData.minMaxSolver->getRequirements(env);
        boost::optional<ValueType> lowerBound = this->computeWeightedResultBound(true, weightVector, storm::storage::BitVector(weightVector.size(), true));
        if (lowerBound) {
            cachedData.minMaxSolver->setLowerBound(lowerBound.get());
            req.clearLowerBounds();
        }
        boost::optional<ValueType> upperBound = this->computeWeightedResultBound(false, weightVector, storm::storage::BitVector(weightVector.size(), true));
        if (upperBound) {
            cachedData.minMaxSolver->setUpperBound(upperBound.get());
            req.clearUpperBounds();
        }
        STORM_LOG_THROW(!req.hasEnabledCriticalRequirement(), storm::exceptions::UncheckedRequirementException,
                        "Solver requirements " + req.getEnabledRequirementsAsString() + " not checked.");
        cachedData.minMaxSolver->setRequirementsChecked(true);
        cachedData.minMaxSolver->setOptimizationDirection(storm::solver::OptimizationDirection::Maximize);

        // Clear the scheduler choices so that an update of the linEqSolver is enforced
        cachedData.schedulerChoices.clear();
        cachedData.schedulerChoices.reserve(epochModel.epochMatrix.getRowGroupCount());

        // Update data for linear equation solving
        cachedData.bLinEq.resize(epochModel.epochMatrix.getRowGroupCount());
        cachedData.xLinEq.resize(this->objectives.size());
        for (auto& x_o : cachedData.xLinEq) {
            x_o.assign(epochModel.epochMatrix.getRowGroupCount(), storm::utility::zero<ValueType>());
        }
    }
}

template<class SparseMdpModelType>
std::vector<typename RewardBoundedMdpPcaaWeightVectorChecker<SparseMdpModelType>::ValueType>
RewardBoundedMdpPcaaWeightVectorChecker<SparseMdpModelType>::getAchievablePoint() const {
    STORM_LOG_THROW(achievablePoint, storm::exceptions::InvalidOperationException, "Tried to retrieve results but check(..) has not been called before.");
    return achievablePoint.value();
}

template<class SparseMdpModelType>
typename RewardBoundedMdpPcaaWeightVectorChecker<SparseMdpModelType>::ValueType
RewardBoundedMdpPcaaWeightVectorChecker<SparseMdpModelType>::getOptimalWeightedSum() const {
    STORM_LOG_THROW(weightedSum, storm::exceptions::InvalidOperationException, "Tried to retrieve results but check(..) has not been called before.");
    return weightedSum.value();
}

template class RewardBoundedMdpPcaaWeightVectorChecker<storm::models::sparse::Mdp<double>>;
template class RewardBoundedMdpPcaaWeightVectorChecker<storm::models::sparse::Mdp<storm::RationalNumber>>;

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
