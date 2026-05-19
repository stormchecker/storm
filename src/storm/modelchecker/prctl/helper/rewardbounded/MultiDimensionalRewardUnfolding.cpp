#include "storm/modelchecker/prctl/helper/rewardbounded/MultiDimensionalRewardUnfolding.h"

#include <functional>
#include <set>
#include <string>

#include "storm/logic/Formulas.h"
#include "storm/utility/macros.h"

#include "storm/modelchecker/prctl/helper/BaierUpperRewardBoundsComputer.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/storage/expressions/Expressions.h"

#include "storm/transformer/EndComponentEliminator.h"

#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace rewardbounded {

template<typename ValueType, bool SingleObjectiveMode>
MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::MultiDimensionalRewardUnfolding(
    storm::models::sparse::Model<ValueType> const& model, std::vector<storm::modelchecker::multiobjective::Objective<ValueType>> const& objectives)
    : model(model), objectives(objectives) {
    initialize();
}

template<typename ValueType, bool SingleObjectiveMode>
MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::MultiDimensionalRewardUnfolding(
    storm::models::sparse::Model<ValueType> const& model, std::shared_ptr<storm::logic::OperatorFormula const> objectiveFormula,
    std::set<storm::expressions::Variable> const& infinityBoundVariables)
    : model(model) {
    STORM_LOG_TRACE("initializing multi-dimensional reward unfolding for formula " << *objectiveFormula << " and " << infinityBoundVariables.size()
                                                                                   << " bound variables should approach infinity.");

    if (objectiveFormula->isProbabilityOperatorFormula()) {
        if (objectiveFormula->getSubformula().isMultiObjectiveFormula()) {
            for (auto const& subFormula : objectiveFormula->getSubformula().asMultiObjectiveFormula().getSubformulas()) {
                STORM_LOG_THROW(subFormula->isBoundedUntilFormula(), storm::exceptions::InvalidPropertyException,
                                "Formula " << objectiveFormula << " is not supported. Invalid subformula " << *subFormula << ".");
            }
        } else {
            STORM_LOG_THROW(objectiveFormula->getSubformula().isBoundedUntilFormula(), storm::exceptions::InvalidPropertyException,
                            "Formula " << objectiveFormula << " is not supported. Invalid subformula " << objectiveFormula->getSubformula() << ".");
        }
    } else {
        STORM_LOG_THROW(objectiveFormula->isRewardOperatorFormula() && objectiveFormula->getSubformula().isCumulativeRewardFormula(),
                        storm::exceptions::InvalidPropertyException, "Formula " << objectiveFormula << " is not supported.");
    }

    // Build an objective from the formula.
    storm::modelchecker::multiobjective::Objective<ValueType> objective;
    objective.formula = objectiveFormula;
    objective.originalFormula = objective.formula;
    objective.considersComplementaryEvent = false;
    objectives.push_back(std::move(objective));

    initialize(infinityBoundVariables);
}

template<typename ValueType, bool SingleObjectiveMode>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::initialize(std::set<storm::expressions::Variable> const& infinityBoundVariables) {
    STORM_LOG_ASSERT(!SingleObjectiveMode || (this->objectives.size() == 1), "Enabled single objective mode but there are multiple objectives.");
    std::vector<Epoch> epochSteps;
    initializeObjectives(epochSteps, infinityBoundVariables);
    initializeMemoryProduct(epochSteps);

    // collect which epoch steps are possible
    possibleEpochSteps.clear();
    for (auto const& step : epochSteps) {
        possibleEpochSteps.insert(step);
    }
}

template<typename ValueType, bool SingleObjectiveMode>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::initializeObjectives(
    std::vector<Epoch>& epochSteps, std::set<storm::expressions::Variable> const& infinityBoundVariables) {
    std::vector<std::vector<uint64_t>> dimensionWiseEpochSteps;
    // collect the time-bounded subobjectives
    for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        auto const& formula = *this->objectives[objIndex].formula;
        if (formula.isProbabilityOperatorFormula()) {
            STORM_LOG_THROW(formula.getSubformula().isBoundedUntilFormula(), storm::exceptions::NotSupportedException,
                            "Unexpected type of subformula for formula " << formula);
            auto const& subformula = formula.getSubformula().asBoundedUntilFormula();
            for (uint64_t dim = 0; dim < subformula.getDimension(); ++dim) {
                Dimension<ValueType> dimension;
                dimension.formula = subformula.restrictToDimension(dim);
                dimension.objectiveIndex = objIndex;
                std::string memLabel = "dim" + std::to_string(dimensions.size()) + "_maybe";
                while (model.getStateLabeling().containsLabel(memLabel)) {
                    memLabel = "_" + memLabel;
                }
                dimension.memoryLabel = memLabel;
                // for simplicity we do not allow interval formulas.
                STORM_LOG_THROW(!subformula.hasLowerBound(dim) || !subformula.hasUpperBound(dim), storm::exceptions::NotSupportedException,
                                "Bounded until formulas are only supported by this method if they consider either an upper bound or a lower bound. Got "
                                    << subformula << " instead.");
                // lower bounded until formulas with non-trivial left hand side are excluded as this would require some additional effort (in particular the
                // ProductModel::transformMemoryState method).
                STORM_LOG_THROW(
                    subformula.hasUpperBound(dim) || subformula.getLeftSubformula(dim).isTrueFormula(), storm::exceptions::NotSupportedException,
                    "Lower bounded until formulas are only supported by this method if the left subformula is 'true'. Got " << subformula << " instead.");

                // Treat formulas that aren't acutally bounded differently
                bool formulaUnbounded =
                    (!subformula.hasLowerBound(dim) && !subformula.hasUpperBound(dim)) ||
                    (subformula.hasLowerBound(dim) && !subformula.isLowerBoundStrict(dim) && !subformula.getLowerBound(dim).containsVariables() &&
                     storm::utility::isZero(subformula.getLowerBound(dim).evaluateAsRational())) ||
                    (subformula.hasUpperBound(dim) && subformula.getUpperBound(dim).isVariable() &&
                     infinityBoundVariables.count(subformula.getUpperBound(dim).getBaseExpression().asVariableExpression().getVariable()) > 0);
                if (formulaUnbounded) {
                    dimensionWiseEpochSteps.push_back(std::vector<uint64_t>(model.getTransitionMatrix().getRowCount(), 0));
                    dimension.scalingFactor = storm::utility::zero<ValueType>();
                    dimension.boundType = DimensionBoundType::Unbounded;
                } else {
                    if (subformula.getTimeBoundReference(dim).isTimeBound() || subformula.getTimeBoundReference(dim).isStepBound()) {
                        dimensionWiseEpochSteps.push_back(std::vector<uint64_t>(model.getTransitionMatrix().getRowCount(), 1));
                        dimension.scalingFactor = storm::utility::one<ValueType>();
                    } else {
                        STORM_LOG_ASSERT(subformula.getTimeBoundReference(dim).isRewardBound(), "Unexpected type of time bound.");
                        STORM_LOG_ASSERT(subformula.getTimeBoundReference(dim).hasRewardModelName() || this->model.hasUniqueRewardModel(),
                                         "Model has several reward models, but no reward model has been specified in the formula.");
                        std::string const& rewardName = subformula.getTimeBoundReference(dim).hasRewardModelName()
                                                            ? subformula.getTimeBoundReference(dim).getRewardName()
                                                            : this->model.getUniqueRewardModelName();
                        STORM_LOG_THROW(this->model.hasRewardModel(rewardName), storm::exceptions::IllegalArgumentException,
                                        "No reward model with name '" << rewardName << "' found.");
                        auto const& rewardModel = this->model.getRewardModel(rewardName);
                        STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                                        "Transition rewards are currently not supported as reward bounds.");
                        std::vector<ValueType> actionRewards = rewardModel.getTotalRewardVector(this->model.getTransitionMatrix());
                        auto discretizedRewardsAndFactor = storm::utility::vector::toIntegralVector<ValueType, uint64_t>(actionRewards);
                        dimensionWiseEpochSteps.push_back(std::move(discretizedRewardsAndFactor.first));
                        dimension.scalingFactor = std::move(discretizedRewardsAndFactor.second);
                    }
                    if (subformula.hasLowerBound(dim)) {
                        if (subformula.getLowerBound(dim).isVariable() &&
                            infinityBoundVariables.count(subformula.getLowerBound(dim).getBaseExpression().asVariableExpression().getVariable()) > 0) {
                            dimension.boundType = DimensionBoundType::LowerBoundInfinity;
                        } else {
                            dimension.boundType = DimensionBoundType::LowerBound;
                        }
                    } else {
                        dimension.boundType = DimensionBoundType::UpperBound;
                    }
                }
                dimensions.emplace_back(std::move(dimension));
            }
        } else if (formula.isRewardOperatorFormula() && formula.getSubformula().isCumulativeRewardFormula()) {
            auto const& subformula = formula.getSubformula().asCumulativeRewardFormula();
            for (uint64_t dim = 0; dim < subformula.getDimension(); ++dim) {
                Dimension<ValueType> dimension;
                dimension.formula = subformula.restrictToDimension(dim);
                STORM_LOG_THROW(!(dimension.formula->asCumulativeRewardFormula().getBound().isVariable() &&
                                  infinityBoundVariables.count(
                                      dimension.formula->asCumulativeRewardFormula().getBound().getBaseExpression().asVariableExpression().getVariable()) > 0),
                                storm::exceptions::NotSupportedException, "Letting cumulative reward bounds approach infinite is not supported.");
                dimension.objectiveIndex = objIndex;
                dimension.boundType = DimensionBoundType::UpperBound;
                if (subformula.getTimeBoundReference(dim).isTimeBound() || subformula.getTimeBoundReference(dim).isStepBound()) {
                    dimensionWiseEpochSteps.push_back(std::vector<uint64_t>(model.getTransitionMatrix().getRowCount(), 1));
                    dimension.scalingFactor = storm::utility::one<ValueType>();
                } else {
                    STORM_LOG_ASSERT(subformula.getTimeBoundReference(dim).isRewardBound(), "Unexpected type of time bound.");
                    std::string const& rewardName = subformula.getTimeBoundReference(dim).getRewardName();
                    STORM_LOG_THROW(this->model.hasRewardModel(rewardName), storm::exceptions::IllegalArgumentException,
                                    "No reward model with name '" << rewardName << "' found.");
                    auto const& rewardModel = this->model.getRewardModel(rewardName);
                    STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                                    "Transition rewards are currently not supported as reward bounds.");
                    std::vector<ValueType> actionRewards = rewardModel.getTotalRewardVector(this->model.getTransitionMatrix());
                    auto discretizedRewardsAndFactor = storm::utility::vector::toIntegralVector<ValueType, uint64_t>(actionRewards);
                    dimensionWiseEpochSteps.push_back(std::move(discretizedRewardsAndFactor.first));
                    dimension.scalingFactor = std::move(discretizedRewardsAndFactor.second);
                }
                dimensions.emplace_back(std::move(dimension));
            }
        }
    }

    // Compute a mapping for each objective to the set of dimensions it considers
    // Also store for each dimension dim, which dimensions should be unsatisfiable whenever the bound of dim is violated
    uint64_t dim = 0;
    for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
        storm::storage::BitVector objDimensions(dimensions.size(), false);
        uint64_t objDimensionCount = 0;
        bool objDimensionsCanBeSatisfiedIndividually = false;
        if (objectives[objIndex].formula->isProbabilityOperatorFormula() && objectives[objIndex].formula->getSubformula().isBoundedUntilFormula()) {
            objDimensionCount = objectives[objIndex].formula->getSubformula().asBoundedUntilFormula().getDimension();
            objDimensionsCanBeSatisfiedIndividually = objectives[objIndex].formula->getSubformula().asBoundedUntilFormula().hasMultiDimensionalSubformulas();
        } else if (objectives[objIndex].formula->isRewardOperatorFormula() && objectives[objIndex].formula->getSubformula().isCumulativeRewardFormula()) {
            objDimensionCount = objectives[objIndex].formula->getSubformula().asCumulativeRewardFormula().getDimension();
        }
        for (uint64_t currDim = dim; currDim < dim + objDimensionCount; ++currDim) {
            objDimensions.set(currDim);
        }
        for (uint64_t currDim = dim; currDim < dim + objDimensionCount; ++currDim) {
            if (!objDimensionsCanBeSatisfiedIndividually || dimensions[currDim].boundType == DimensionBoundType::UpperBound) {
                dimensions[currDim].dependentDimensions = objDimensions;
            } else {
                dimensions[currDim].dependentDimensions = storm::storage::BitVector(dimensions.size(), false);
                dimensions[currDim].dependentDimensions.set(currDim, true);
            }
        }
        dim += objDimensionCount;
        objectiveDimensions.push_back(std::move(objDimensions));
    }
    assert(dim == dimensions.size());

    // Initialize the epoch manager
    epochManager = EpochManager(dimensions.size());

    // Convert the epoch steps to a choice-wise representation
    epochSteps.reserve(model.getTransitionMatrix().getRowCount());
    for (uint64_t choice = 0; choice < model.getTransitionMatrix().getRowCount(); ++choice) {
        Epoch step;
        uint64_t dim = 0;
        for (auto const& dimensionSteps : dimensionWiseEpochSteps) {
            epochManager.setDimensionOfEpoch(step, dim, dimensionSteps[choice]);
            ++dim;
        }
        epochSteps.push_back(step);
    }

    // Set the maximal values we need to consider for each dimension
    computeMaxDimensionValues();
    translateLowerBoundInfinityDimensions(epochSteps);
}

template<typename ValueType, bool SingleObjectiveMode>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::initializeMemoryProduct(std::vector<Epoch> const& epochSteps) {
    productModel = std::make_unique<ProductModel<ValueType>>(model, objectives, dimensions, objectiveDimensions, epochManager, epochSteps);
}

template<typename ValueType, bool SingleObjectiveMode>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::computeMaxDimensionValues() {
    for (uint64_t dim = 0; dim < epochManager.getDimensionCount(); ++dim) {
        storm::expressions::Expression bound;
        bool isStrict = false;
        storm::logic::Formula const& dimFormula = *dimensions[dim].formula;
        if (dimFormula.isBoundedUntilFormula()) {
            assert(!dimFormula.asBoundedUntilFormula().hasMultiDimensionalSubformulas());
            if (dimFormula.asBoundedUntilFormula().hasUpperBound()) {
                STORM_LOG_ASSERT(!dimFormula.asBoundedUntilFormula().hasLowerBound(), "Bounded until formulas with interval bounds are not supported.");
                bound = dimFormula.asBoundedUntilFormula().getUpperBound();
                isStrict = dimFormula.asBoundedUntilFormula().isUpperBoundStrict();
            } else {
                STORM_LOG_ASSERT(dimFormula.asBoundedUntilFormula().hasLowerBound(), "Bounded until formulas without any bounds are not supported.");
                bound = dimFormula.asBoundedUntilFormula().getLowerBound();
                isStrict = dimFormula.asBoundedUntilFormula().isLowerBoundStrict();
            }
        } else if (dimFormula.isCumulativeRewardFormula()) {
            assert(!dimFormula.asCumulativeRewardFormula().isMultiDimensional());
            bound = dimFormula.asCumulativeRewardFormula().getBound();
            isStrict = dimFormula.asCumulativeRewardFormula().isBoundStrict();
        }

        if (!bound.containsVariables()) {
            // We always consider upper bounds to be non-strict and lower bounds to be strict.
            // Thus, >=N would become >N-1. However, note that the case N=0 is treated separately.
            if (dimensions[dim].boundType == DimensionBoundType::LowerBound || dimensions[dim].boundType == DimensionBoundType::UpperBound) {
                ValueType discretizedBound = storm::utility::convertNumber<ValueType>(bound.evaluateAsRational());
                discretizedBound /= dimensions[dim].scalingFactor;
                if (storm::utility::isInteger(discretizedBound)) {
                    if (isStrict == (dimensions[dim].boundType == DimensionBoundType::UpperBound)) {
                        discretizedBound -= storm::utility::one<ValueType>();
                    }
                } else {
                    discretizedBound = storm::utility::floor(discretizedBound);
                }
                uint64_t dimensionValue = storm::utility::convertNumber<uint64_t>(discretizedBound);
                STORM_LOG_THROW(epochManager.isValidDimensionValue(dimensionValue), storm::exceptions::NotSupportedException,
                                "The bound " << bound << " is too high for the considered number of dimensions.");
                dimensions[dim].maxValue = dimensionValue;
            }
        }
    }
}

template<typename ValueType, bool SingleObjectiveMode>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::translateLowerBoundInfinityDimensions(std::vector<Epoch>& epochSteps) {
    // Translate lowerBoundedByInfinity dimensions to finite bounds
    storm::storage::BitVector infLowerBoundedDimensions(dimensions.size(), false);
    storm::storage::BitVector upperBoundedDimensions(dimensions.size(), false);
    for (uint64_t dim = 0; dim < dimensions.size(); ++dim) {
        infLowerBoundedDimensions.set(dim, dimensions[dim].boundType == DimensionBoundType::LowerBoundInfinity);
        upperBoundedDimensions.set(dim, dimensions[dim].boundType == DimensionBoundType::UpperBound);
    }
    if (!infLowerBoundedDimensions.empty()) {
        // We can currently only handle this case for single maximizing bounded until probability objectives.
        // The approach is to erase all epochSteps that are not part of an end component and then change the reward bound to '> 0'.
        // Then, reaching a reward means reaching an end component where arbitrarily many reward can be collected.
        STORM_LOG_THROW(
            SingleObjectiveMode, storm::exceptions::NotSupportedException,
            "Letting lower bounds approach infinity is only supported in single objective mode.");  // It most likely also works with multiple objectives with
                                                                                                    // the same shape. However, we haven't checked this yet.
        STORM_LOG_THROW(objectives.front().formula->isProbabilityOperatorFormula(), storm::exceptions::NotSupportedException,
                        "Letting lower bounds approach infinity is only supported for probability operator formulas");
        auto const& probabilityOperatorFormula = objectives.front().formula->asProbabilityOperatorFormula();
        STORM_LOG_THROW(probabilityOperatorFormula.getSubformula().isBoundedUntilFormula(), storm::exceptions::NotSupportedException,
                        "Letting lower bounds approach infinity is only supported for bounded until probabilities.");
        STORM_LOG_THROW(!model.isNondeterministicModel() ||
                            (probabilityOperatorFormula.hasOptimalityType() && storm::solver::maximize(probabilityOperatorFormula.getOptimalityType())),
                        storm::exceptions::NotSupportedException,
                        "Letting lower bounds approach infinity is only supported for maximizing bounded until probabilities.");

        STORM_LOG_THROW(upperBoundedDimensions.empty() || !probabilityOperatorFormula.getSubformula().asBoundedUntilFormula().hasMultiDimensionalSubformulas(),
                        storm::exceptions::NotSupportedException,
                        "Letting lower bounds approach infinity is only supported if the formula has either only lower bounds or if it has a single goal "
                        "state.");  // This would fail because the upper bounded dimension(s) might be satisfied already. One should erase epoch steps in the
                                    // epoch model (after applying the goal-unfolding).
        storm::storage::BitVector choicesWithoutUpperBoundedStep(model.getNumberOfChoices(), true);
        if (!upperBoundedDimensions.empty()) {
            // To not invalidate upper-bounded dimensions, one needs to consider MECS where no reward for such a dimension is collected.
            for (uint64_t choiceIndex = 0; choiceIndex < model.getNumberOfChoices(); ++choiceIndex) {
                for (auto dim : upperBoundedDimensions) {
                    if (epochManager.getDimensionOfEpoch(epochSteps[choiceIndex], dim) != 0) {
                        choicesWithoutUpperBoundedStep.set(choiceIndex, false);
                        break;
                    }
                }
            }
        }
        storm::storage::MaximalEndComponentDecomposition<ValueType> mecDecomposition(model.getTransitionMatrix(), model.getBackwardTransitions(),
                                                                                     storm::storage::BitVector(model.getNumberOfStates(), true),
                                                                                     choicesWithoutUpperBoundedStep);
        storm::storage::BitVector nonMecChoices(model.getNumberOfChoices(), true);
        for (auto const& mec : mecDecomposition) {
            for (auto const& stateChoicesPair : mec) {
                for (auto const& choice : stateChoicesPair.second) {
                    nonMecChoices.set(choice, false);
                }
            }
        }
        for (auto choice : nonMecChoices) {
            for (auto dim : infLowerBoundedDimensions) {
                epochManager.setDimensionOfEpoch(epochSteps[choice], dim, 0);
            }
        }

        // Translate the dimension to '>0'
        for (auto dim : infLowerBoundedDimensions) {
            dimensions[dim].boundType = DimensionBoundType::LowerBound;
            dimensions[dim].maxValue = 0;
        }
    }
}

template<typename ValueType, bool SingleObjectiveMode>
typename MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::Epoch MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getStartEpoch(
    bool setUnknownDimsToBottom) {
    Epoch startEpoch = epochManager.getZeroEpoch();
    for (uint64_t dim = 0; dim < epochManager.getDimensionCount(); ++dim) {
        if (dimensions[dim].maxValue) {
            epochManager.setDimensionOfEpoch(startEpoch, dim, dimensions[dim].maxValue.get());
        } else {
            STORM_LOG_THROW(setUnknownDimsToBottom || dimensions[dim].boundType == DimensionBoundType::Unbounded, storm::exceptions::UnexpectedException,
                            "Tried to obtain the start epoch although no bound on dimension " << dim << " is known.");
            epochManager.setBottomDimension(startEpoch, dim);
        }
    }
    STORM_LOG_TRACE("Start epoch is " << epochManager.toString(startEpoch));
    return startEpoch;
}

template<typename ValueType, bool SingleObjectiveMode>
std::vector<typename MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::Epoch>
MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getEpochComputationOrder(Epoch const& startEpoch, bool stopAtComputedEpochs) {
    // Perform a DFS to find all the reachable epochs
    std::vector<Epoch> dfsStack;
    std::set<Epoch, std::function<bool(Epoch const&, Epoch const&)>> collectedEpochs(
        std::bind(&EpochManager::epochClassZigZagOrder, &epochManager, std::placeholders::_1, std::placeholders::_2));

    if (!stopAtComputedEpochs || epochSolutions.count(startEpoch) == 0) {
        collectedEpochs.insert(startEpoch);
        dfsStack.push_back(startEpoch);
    }
    while (!dfsStack.empty()) {
        Epoch currentEpoch = dfsStack.back();
        dfsStack.pop_back();
        for (auto const& step : possibleEpochSteps) {
            Epoch successorEpoch = epochManager.getSuccessorEpoch(currentEpoch, step);
            if (!stopAtComputedEpochs || epochSolutions.count(successorEpoch) == 0) {
                if (collectedEpochs.insert(successorEpoch).second) {
                    dfsStack.push_back(std::move(successorEpoch));
                }
            }
        }
    }
    return std::vector<Epoch>(collectedEpochs.begin(), collectedEpochs.end());
}

template<typename ValueType, bool SingleObjectiveMode>
EpochModel<ValueType, SingleObjectiveMode>& MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::setCurrentEpoch(Epoch const& epoch) {
    STORM_LOG_DEBUG("Setting model for epoch " << epochManager.toString(epoch));

    // Check if we need to update the current epoch class
    if (!currentEpoch || !epochManager.compareEpochClass(epoch, currentEpoch.get())) {
        setCurrentEpochClass(epoch);
        epochModel.epochMatrixChanged = true;
        if (storm::settings::getModule<storm::settings::modules::CoreSettings>().isShowStatisticsSet()) {
            if (storm::utility::graph::hasCycle(epochModel.epochMatrix)) {
                std::cout << "Epoch model for epoch " << epochManager.toString(epoch) << " is cyclic.\n";
            }
        }
    } else {
        epochModel.epochMatrixChanged = false;
    }

    bool containsLowerBoundedObjective = false;
    for (auto const& dimension : dimensions) {
        if (dimension.boundType == DimensionBoundType::LowerBound) {
            containsLowerBoundedObjective = true;
            break;
        }
    }
    std::map<Epoch, EpochSolution const*> subSolutions;
    for (auto const& step : possibleEpochSteps) {
        Epoch successorEpoch = epochManager.getSuccessorEpoch(epoch, step);
        if (successorEpoch != epoch) {
            auto successorSolIt = epochSolutions.find(successorEpoch);
            STORM_LOG_ASSERT(successorSolIt != epochSolutions.end(), "Solution for successor epoch does not exist (anymore).");
            subSolutions.emplace(successorEpoch, &successorSolIt->second);
        }
    }
    epochModel.stepSolutions.resize(epochModel.stepChoices.getNumberOfSetBits());
    auto stepSolIt = epochModel.stepSolutions.begin();
    for (auto reducedChoice : epochModel.stepChoices) {
        uint64_t productChoice = epochModelToProductChoiceMap[reducedChoice];
        uint64_t productState = productModel->getProductStateFromChoice(productChoice);
        auto const& memoryState = productModel->getMemoryState(productState);
        Epoch successorEpoch = epochManager.getSuccessorEpoch(epoch, productModel->getSteps()[productChoice]);
        EpochClass successorEpochClass = epochManager.getEpochClass(successorEpoch);
        // Find out whether objective reward is earned for the current choice
        // Objective reward is not earned if
        // a) there is an upper bounded subObjective that is __still_relevant__ but the corresponding reward bound is passed after taking the choice
        // b) there is a lower bounded subObjective and the corresponding reward bound is not passed yet.
        for (uint64_t objIndex = 0; objIndex < this->objectives.size(); ++objIndex) {
            bool rewardEarned = !storm::utility::isZero(epochModel.objectiveRewards[objIndex][reducedChoice]);
            if (rewardEarned) {
                for (auto dim : objectiveDimensions[objIndex]) {
                    if ((dimensions[dim].boundType == DimensionBoundType::UpperBound) == epochManager.isBottomDimension(successorEpoch, dim) &&
                        productModel->getMemoryStateManager().isRelevantDimension(memoryState, dim)) {
                        rewardEarned = false;
                        break;
                    }
                }
            }
            epochModel.objectiveRewardFilter[objIndex].set(reducedChoice, rewardEarned);
        }
        // compute the solution for the stepChoices
        // For optimization purposes, we distinguish the case where the memory state does not have to be transformed
        EpochSolution const& successorEpochSolution = getEpochSolution(subSolutions, successorEpoch);
        SolutionType choiceSolution;
        bool firstSuccessor = true;
        if (!containsLowerBoundedObjective && epochManager.compareEpochClass(epoch, successorEpoch)) {
            for (auto const& successor : productModel->getProduct().getTransitionMatrix().getRow(productChoice)) {
                if (firstSuccessor) {
                    choiceSolution = getScaledSolution(getStateSolution(successorEpochSolution, successor.getColumn()), successor.getValue());
                    firstSuccessor = false;
                } else {
                    addScaledSolution(choiceSolution, getStateSolution(successorEpochSolution, successor.getColumn()), successor.getValue());
                }
            }
        } else {
            for (auto const& successor : productModel->getProduct().getTransitionMatrix().getRow(productChoice)) {
                uint64_t successorProductState = productModel->transformProductState(successor.getColumn(), successorEpochClass, memoryState);
                SolutionType const& successorSolution = getStateSolution(successorEpochSolution, successorProductState);
                if (firstSuccessor) {
                    choiceSolution = getScaledSolution(successorSolution, successor.getValue());
                    firstSuccessor = false;
                } else {
                    addScaledSolution(choiceSolution, successorSolution, successor.getValue());
                }
            }
        }
        assert(!firstSuccessor);
        *stepSolIt = std::move(choiceSolution);
        ++stepSolIt;
    }

    assert(epochModel.objectiveRewards.size() == objectives.size());
    assert(epochModel.objectiveRewardFilter.size() == objectives.size());
    assert(epochModel.epochMatrix.getRowCount() == epochModel.stepChoices.size());
    assert(epochModel.stepChoices.size() == epochModel.objectiveRewards.front().size());
    assert(epochModel.objectiveRewards.front().size() == epochModel.objectiveRewards.back().size());
    assert(epochModel.objectiveRewards.front().size() == epochModel.objectiveRewardFilter.front().size());
    assert(epochModel.objectiveRewards.back().size() == epochModel.objectiveRewardFilter.back().size());
    assert(epochModel.stepChoices.getNumberOfSetBits() == epochModel.stepSolutions.size());

    currentEpoch = epoch;
    /*
    std::cout << "Epoch model for epoch " << storm::utility::vector::toString(epoch) << '\n';
    std::cout << "Matrix: \n" << epochModel.epochMatrix << '\n';
    std::cout << "ObjectiveRewards: " << storm::utility::vector::toString(epochModel.objectiveRewards[0]) << '\n';
    std::cout << "steps: " << epochModel.stepChoices << '\n';
    std::cout << "step solutions: ";
    for (int i = 0; i < epochModel.stepSolutions.size(); ++i) {
        std::cout << "   " << epochModel.stepSolutions[i].weightedValue;
    }
    std::cout << '\n';
    */
    return epochModel;
}

template<typename ValueType, bool SingleObjectiveMode>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::setCurrentEpochClass(Epoch const& epoch) {
    EpochClass epochClass = epochManager.getEpochClass(epoch);
    // std::cout << "Setting epoch class for epoch " << epochManager.toString(epoch) << '\n';
    auto productObjectiveRewards = productModel->computeObjectiveRewards(epochClass, objectives);

    storm::storage::BitVector stepChoices(productModel->getProduct().getTransitionMatrix().getRowCount(), false);
    uint64_t choice = 0;
    for (auto const& step : productModel->getSteps()) {
        if (!epochManager.isZeroEpoch(step) && epochManager.getSuccessorEpoch(epoch, step) != epoch) {
            stepChoices.set(choice, true);
        }
        ++choice;
    }
    epochModel.epochMatrix = productModel->getProduct().getTransitionMatrix().filterEntries(~stepChoices);
    // redirect transitions for the case where the lower reward bounds are not met yet
    storm::storage::BitVector violatedLowerBoundedDimensions(dimensions.size(), false);
    for (uint64_t dim = 0; dim < dimensions.size(); ++dim) {
        if (dimensions[dim].boundType == DimensionBoundType::LowerBound && !epochManager.isBottomDimensionEpochClass(epochClass, dim)) {
            violatedLowerBoundedDimensions.set(dim);
        }
    }
    if (!violatedLowerBoundedDimensions.empty()) {
        for (uint64_t state = 0; state < epochModel.epochMatrix.getRowGroupCount(); ++state) {
            auto const& memoryState = productModel->getMemoryState(state);
            for (auto& entry : epochModel.epochMatrix.getRowGroup(state)) {
                entry.setColumn(productModel->transformProductState(entry.getColumn(), epochClass, memoryState));
            }
        }
    }

    storm::storage::BitVector zeroObjRewardChoices(productModel->getProduct().getTransitionMatrix().getRowCount(), true);
    for (uint64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
        if (violatedLowerBoundedDimensions.isDisjointFrom(objectiveDimensions[objIndex])) {
            zeroObjRewardChoices &= storm::utility::vector::filterZero(productObjectiveRewards[objIndex]);
        }
    }
    storm::storage::BitVector allProductStates(productModel->getProduct().getNumberOfStates(), true);

    // Get the relevant states for this epoch.
    storm::storage::BitVector productInStates = productModel->getInStates(epochClass);
    // The epoch model only needs to consider the states that are reachable from a relevant state
    storm::storage::BitVector consideredStates =
        storm::utility::graph::getReachableStates(epochModel.epochMatrix, productInStates, allProductStates, ~allProductStates);

    // We assume that there is no end component in which objective reward is earned
    STORM_LOG_ASSERT(!storm::utility::graph::checkIfECWithChoiceExists(epochModel.epochMatrix, epochModel.epochMatrix.transpose(true), allProductStates,
                                                                       ~zeroObjRewardChoices & ~stepChoices),
                     "There is a scheduler that yields infinite reward for one objective. This case should be excluded");

    // Create the epoch model matrix
    std::vector<uint64_t> productToEpochModelStateMapping;
    if (model.isOfType(storm::models::ModelType::Dtmc)) {
        assert(zeroObjRewardChoices.size() == productModel->getProduct().getNumberOfStates());
        assert(stepChoices.size() == productModel->getProduct().getNumberOfStates());
        STORM_LOG_ASSERT(epochModel.equationSolverProblemFormat.is_initialized(), "Linear equation problem format was not set.");
        bool convertToEquationSystem = epochModel.equationSolverProblemFormat.get() == storm::solver::LinearEquationSolverProblemFormat::EquationSystem;
        // For DTMCs we consider the subsystem induced by the considered states.
        // The transitions for states with zero reward are filtered out to guarantee a unique solution of the eq-system.
        auto backwardTransitions = epochModel.epochMatrix.transpose(true);
        storm::storage::BitVector nonZeroRewardStates =
            storm::utility::graph::performProbGreater0(backwardTransitions, consideredStates, consideredStates & (~zeroObjRewardChoices | stepChoices));
        // If there is at least one considered state with reward zero, we have to add a 'zero-reward-state' to the epoch model.
        bool requiresZeroRewardState = nonZeroRewardStates != consideredStates;
        uint64_t numEpochModelStates = nonZeroRewardStates.getNumberOfSetBits();
        uint64_t zeroRewardInState = numEpochModelStates;
        if (requiresZeroRewardState) {
            ++numEpochModelStates;
        }
        storm::storage::SparseMatrixBuilder<ValueType> builder;
        if (!nonZeroRewardStates.empty()) {
            builder = storm::storage::SparseMatrixBuilder<ValueType>(
                epochModel.epochMatrix.getSubmatrix(true, nonZeroRewardStates, nonZeroRewardStates, convertToEquationSystem));
        }
        if (requiresZeroRewardState) {
            if (convertToEquationSystem) {
                // add a diagonal entry
                builder.addNextValue(zeroRewardInState, zeroRewardInState, storm::utility::zero<ValueType>());
            }
            epochModel.epochMatrix = builder.build(numEpochModelStates, numEpochModelStates);
        } else {
            assert(!nonZeroRewardStates.empty());
            epochModel.epochMatrix = builder.build();
        }
        if (convertToEquationSystem) {
            epochModel.epochMatrix.convertToEquationSystem();
        }

        epochModelToProductChoiceMap.clear();
        epochModelToProductChoiceMap.reserve(numEpochModelStates);
        productToEpochModelStateMapping.assign(nonZeroRewardStates.size(), zeroRewardInState);
        for (auto productState : nonZeroRewardStates) {
            productToEpochModelStateMapping[productState] = epochModelToProductChoiceMap.size();
            epochModelToProductChoiceMap.push_back(productState);
        }
        if (requiresZeroRewardState) {
            uint64_t zeroRewardProductState = (consideredStates & ~nonZeroRewardStates).getNextSetIndex(0);
            assert(zeroRewardProductState < consideredStates.size());
            epochModelToProductChoiceMap.push_back(zeroRewardProductState);
        }
    } else if (model.isOfType(storm::models::ModelType::Mdp)) {
        // Eliminate zero-reward end components
        auto ecElimResult = storm::transformer::EndComponentEliminator<ValueType>::transform(epochModel.epochMatrix, consideredStates,
                                                                                             zeroObjRewardChoices & ~stepChoices, consideredStates);
        epochModel.epochMatrix = std::move(ecElimResult.matrix);
        epochModelToProductChoiceMap = std::move(ecElimResult.newToOldRowMapping);
        productToEpochModelStateMapping = std::move(ecElimResult.oldToNewStateMapping);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unsupported model type.");
    }

    epochModel.stepChoices = storm::storage::BitVector(epochModel.epochMatrix.getRowCount(), false);
    for (uint64_t choice = 0; choice < epochModel.epochMatrix.getRowCount(); ++choice) {
        if (stepChoices.get(epochModelToProductChoiceMap[choice])) {
            epochModel.stepChoices.set(choice, true);
        }
    }

    epochModel.objectiveRewards.clear();
    for (uint64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
        std::vector<ValueType> const& productObjRew = productObjectiveRewards[objIndex];
        std::vector<ValueType> reducedModelObjRewards;
        reducedModelObjRewards.reserve(epochModel.epochMatrix.getRowCount());
        for (auto const& productChoice : epochModelToProductChoiceMap) {
            reducedModelObjRewards.push_back(productObjRew[productChoice]);
        }
        // Check if the objective is violated in the current epoch
        if (!violatedLowerBoundedDimensions.isDisjointFrom(objectiveDimensions[objIndex])) {
            storm::utility::vector::setVectorValues(reducedModelObjRewards, ~epochModel.stepChoices, storm::utility::zero<ValueType>());
        }
        epochModel.objectiveRewards.push_back(std::move(reducedModelObjRewards));
    }

    epochModel.epochInStates = storm::storage::BitVector(epochModel.epochMatrix.getRowGroupCount(), false);
    for (auto productState : productInStates) {
        STORM_LOG_ASSERT(productToEpochModelStateMapping[productState] < epochModel.epochMatrix.getRowGroupCount(),
                         "Selected product state does not exist in the epoch model.");
        epochModel.epochInStates.set(productToEpochModelStateMapping[productState], true);
    }

    std::vector<uint64_t> toEpochModelInStatesMap(productModel->getProduct().getNumberOfStates(), std::numeric_limits<uint64_t>::max());
    std::vector<uint64_t> epochModelStateToInStateMap = epochModel.epochInStates.getNumberOfSetBitsBeforeIndices();
    for (auto productState : productInStates) {
        toEpochModelInStatesMap[productState] = epochModelStateToInStateMap[productToEpochModelStateMapping[productState]];
    }
    productStateToEpochModelInStateMap = std::make_shared<std::vector<uint64_t> const>(std::move(toEpochModelInStatesMap));

    epochModel.objectiveRewardFilter.clear();
    for (auto const& objRewards : epochModel.objectiveRewards) {
        epochModel.objectiveRewardFilter.push_back(storm::utility::vector::filterZero(objRewards));
        epochModel.objectiveRewardFilter.back().complement();
    }
}

template<typename ValueType, bool SingleObjectiveMode>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::setEquationSystemFormatForEpochModel(
    storm::solver::LinearEquationSolverProblemFormat eqSysFormat) {
    STORM_LOG_ASSERT(model.isOfType(storm::models::ModelType::Dtmc), "Trying to set the equation problem format although the model is not deterministic.");
    epochModel.equationSolverProblemFormat = eqSysFormat;
}

template<typename ValueType, bool SingleObjectiveMode>
template<bool SO, typename std::enable_if<SO, int>::type>
typename MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::SolutionType
MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getScaledSolution(SolutionType const& solution, ValueType const& scalingFactor) const {
    return solution * scalingFactor;
}

template<typename ValueType, bool SingleObjectiveMode>
template<bool SO, typename std::enable_if<!SO, int>::type>
typename MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::SolutionType
MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getScaledSolution(SolutionType const& solution, ValueType const& scalingFactor) const {
    SolutionType res;
    res.reserve(solution.size());
    for (auto const& sol : solution) {
        res.push_back(sol * scalingFactor);
    }
    return res;
}

template<typename ValueType, bool SingleObjectiveMode>
template<bool SO, typename std::enable_if<SO, int>::type>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::addScaledSolution(SolutionType& solution, SolutionType const& solutionToAdd,
                                                                                        ValueType const& scalingFactor) const {
    solution += solutionToAdd * scalingFactor;
}

template<typename ValueType, bool SingleObjectiveMode>
template<bool SO, typename std::enable_if<!SO, int>::type>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::addScaledSolution(SolutionType& solution, SolutionType const& solutionToAdd,
                                                                                        ValueType const& scalingFactor) const {
    storm::utility::vector::addScaledVector(solution, solutionToAdd, scalingFactor);
}

template<typename ValueType, bool SingleObjectiveMode>
template<bool SO, typename std::enable_if<SO, int>::type>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::setSolutionEntry(SolutionType& solution, uint64_t objIndex,
                                                                                       ValueType const& value) const {
    STORM_LOG_ASSERT(objIndex == 0, "Invalid objective index in single objective mode");
    solution = value;
}

template<typename ValueType, bool SingleObjectiveMode>
template<bool SO, typename std::enable_if<!SO, int>::type>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::setSolutionEntry(SolutionType& solution, uint64_t objIndex,
                                                                                       ValueType const& value) const {
    STORM_LOG_ASSERT(objIndex < solution.size(), "Invalid objective index " << objIndex << ".");
    solution[objIndex] = value;
}

template<typename ValueType, bool SingleObjectiveMode>
template<bool SO, typename std::enable_if<SO, int>::type>
std::string MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::solutionToString(SolutionType const& solution) const {
    std::stringstream stringstream;
    stringstream << solution;
    return stringstream.str();
}

template<typename ValueType, bool SingleObjectiveMode>
template<bool SO, typename std::enable_if<!SO, int>::type>
std::string MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::solutionToString(SolutionType const& solution) const {
    std::stringstream stringstream;
    stringstream << "(";
    bool first = true;
    for (auto const& s : solution) {
        if (first) {
            first = false;
        } else {
            stringstream << ", ";
        }
        stringstream << s;
    }
    stringstream << ")";
    return stringstream.str();
}

template<typename ValueType, bool SingleObjectiveMode>
ValueType MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getRequiredEpochModelPrecision(Epoch const& startEpoch, ValueType const& precision) {
    return precision / storm::utility::convertNumber<ValueType>(epochManager.getSumOfDimensions(startEpoch) + 1);
}

template<typename ValueType, bool SingleObjectiveMode>
boost::optional<ValueType> MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getUpperObjectiveBound(uint64_t objectiveIndex) {
    auto& objective = this->objectives[objectiveIndex];
    if (!objective.upperResultBound) {
        if (objective.formula->isProbabilityOperatorFormula()) {
            objective.upperResultBound = storm::utility::one<ValueType>();
        } else if (objective.formula->isRewardOperatorFormula()) {
            auto const& rewModel = this->model.getRewardModel(objective.formula->asRewardOperatorFormula().getRewardModelName());
            auto actionRewards = rewModel.getTotalRewardVector(this->model.getTransitionMatrix());
            if (objective.formula->getSubformula().isCumulativeRewardFormula()) {
                // Try to get an upper bound by computing the maximal reward achievable within one epoch step
                auto const& cumulativeRewardFormula = objective.formula->getSubformula().asCumulativeRewardFormula();
                for (uint64_t objDim = 0; objDim < cumulativeRewardFormula.getDimension(); ++objDim) {
                    boost::optional<ValueType> resBound;
                    ValueType rewardBound = cumulativeRewardFormula.template getBound<ValueType>(objDim);
                    if (cumulativeRewardFormula.getTimeBoundReference(objDim).isRewardBound()) {
                        auto const& costModel = this->model.getRewardModel(cumulativeRewardFormula.getTimeBoundReference(objDim).getRewardName());
                        if (!costModel.hasTransitionRewards()) {
                            auto actionCosts = costModel.getTotalRewardVector(this->model.getTransitionMatrix());
                            ValueType largestRewardPerCost = storm::utility::zero<ValueType>();
                            bool isFinite = true;
                            for (auto rewIt = actionRewards.begin(), costIt = actionCosts.begin(); rewIt != actionRewards.end(); ++rewIt, ++costIt) {
                                if (!storm::utility::isZero(*rewIt)) {
                                    if (storm::utility::isZero(*costIt)) {
                                        isFinite = false;
                                        break;
                                    }
                                    ValueType rewardPerCost = *rewIt / *costIt;
                                    largestRewardPerCost = std::max(largestRewardPerCost, rewardPerCost);
                                }
                            }
                            if (isFinite) {
                                resBound = largestRewardPerCost * rewardBound;
                            }
                        }
                    } else {
                        resBound = (*std::max_element(actionRewards.begin(), actionRewards.end())) * rewardBound;
                    }
                    if (resBound && (!objective.upperResultBound || objective.upperResultBound.get() > resBound.get())) {
                        objective.upperResultBound = resBound;
                    }
                }

                // If we could not find an upper bound, try to get an upper bound for the unbounded case
                if (!objective.upperResultBound) {
                    storm::storage::BitVector allStates(model.getNumberOfStates(), true);
                    // Get the set of states from which reward is reachable
                    auto nonZeroRewardStates = rewModel.getStatesWithZeroReward(model.getTransitionMatrix());
                    nonZeroRewardStates.complement();
                    auto expRewGreater0EStates = storm::utility::graph::performProbGreater0E(model.getBackwardTransitions(), allStates, nonZeroRewardStates);
                    // Eliminate zero-reward ECs
                    auto zeroRewardChoices = rewModel.getChoicesWithZeroReward(model.getTransitionMatrix());
                    auto ecElimRes = storm::transformer::EndComponentEliminator<ValueType>::transform(model.getTransitionMatrix(), expRewGreater0EStates,
                                                                                                      zeroRewardChoices, ~allStates);
                    allStates.resize(ecElimRes.matrix.getRowGroupCount());
                    storm::storage::BitVector outStates(allStates.size(), false);
                    std::vector<ValueType> rew0StateProbs;
                    rew0StateProbs.reserve(ecElimRes.matrix.getRowCount());
                    for (uint64_t state = 0; state < allStates.size(); ++state) {
                        for (uint64_t choice = ecElimRes.matrix.getRowGroupIndices()[state]; choice < ecElimRes.matrix.getRowGroupIndices()[state + 1];
                             ++choice) {
                            // Check whether the choice lead to a state with expRew 0 in the original model
                            bool isOutChoice = false;
                            uint64_t originalModelChoice = ecElimRes.newToOldRowMapping[choice];
                            for (auto const& entry : model.getTransitionMatrix().getRow(originalModelChoice)) {
                                if (!expRewGreater0EStates.get(entry.getColumn())) {
                                    isOutChoice = true;
                                    outStates.set(state, true);
                                    rew0StateProbs.push_back(storm::utility::one<ValueType>() - ecElimRes.matrix.getRowSum(choice));
                                    assert(!storm::utility::isZero(rew0StateProbs.back()));
                                    break;
                                }
                            }
                            if (!isOutChoice) {
                                rew0StateProbs.push_back(storm::utility::zero<ValueType>());
                            }
                        }
                    }
                    // An upper reward bound can only be computed if it is below infinity
                    if (storm::utility::graph::performProb1A(ecElimRes.matrix, ecElimRes.matrix.getRowGroupIndices(), ecElimRes.matrix.transpose(true),
                                                             allStates, outStates)
                            .full()) {
                        std::vector<ValueType> rewards;
                        rewards.reserve(ecElimRes.matrix.getRowCount());
                        for (auto row : ecElimRes.newToOldRowMapping) {
                            rewards.push_back(actionRewards[row]);
                        }
                        storm::modelchecker::helper::BaierUpperRewardBoundsComputer<ValueType> baier(ecElimRes.matrix, rew0StateProbs);
                        objective.upperResultBound = baier.computeTotalRewardBounds(rewards).upper;
                    }
                }
            }
        }
    }
    return objective.upperResultBound;
}

template<typename ValueType, bool SingleObjectiveMode>
boost::optional<ValueType> MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getLowerObjectiveBound(uint64_t objectiveIndex) {
    auto& objective = this->objectives[objectiveIndex];
    if (!objective.lowerResultBound) {
        objective.lowerResultBound = storm::utility::zero<ValueType>();
    }
    return objective.lowerResultBound;
}

template<typename ValueType, bool SingleObjectiveMode>
void MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::setSolutionForCurrentEpoch(std::vector<SolutionType>&& inStateSolutions) {
    STORM_LOG_ASSERT(currentEpoch, "Tried to set a solution for the current epoch, but no epoch was specified before.");
    STORM_LOG_ASSERT(inStateSolutions.size() == epochModel.epochInStates.getNumberOfSetBits(), "Invalid number of solutions.");

    std::set<Epoch> predecessorEpochs, successorEpochs;
    for (auto const& step : possibleEpochSteps) {
        epochManager.gatherPredecessorEpochs(predecessorEpochs, currentEpoch.get(), step);
        successorEpochs.insert(epochManager.getSuccessorEpoch(currentEpoch.get(), step));
    }
    predecessorEpochs.erase(currentEpoch.get());
    successorEpochs.erase(currentEpoch.get());

    // clean up solutions that are not needed anymore
    for (auto const& successorEpoch : successorEpochs) {
        auto successorEpochSolutionIt = epochSolutions.find(successorEpoch);
        STORM_LOG_ASSERT(successorEpochSolutionIt != epochSolutions.end(), "Solution for successor epoch does not exist (anymore).");
        --successorEpochSolutionIt->second.count;
        if (successorEpochSolutionIt->second.count == 0) {
            epochSolutions.erase(successorEpochSolutionIt);
        }
    }

    // add the new solution
    EpochSolution solution;
    solution.count = predecessorEpochs.size();
    solution.productStateToSolutionVectorMap = productStateToEpochModelInStateMap;
    solution.solutions = std::move(inStateSolutions);
    epochSolutions[currentEpoch.get()] = std::move(solution);
}

template<typename ValueType, bool SingleObjectiveMode>
typename MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::SolutionType const&
MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getStateSolution(Epoch const& epoch, uint64_t const& productState) {
    auto epochSolutionIt = epochSolutions.find(epoch);
    STORM_LOG_ASSERT(epochSolutionIt != epochSolutions.end(), "Requested unexisting solution for epoch " << epochManager.toString(epoch) << ".");
    return getStateSolution(epochSolutionIt->second, productState);
}

template<typename ValueType, bool SingleObjectiveMode>
typename MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::EpochSolution const&
MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getEpochSolution(std::map<Epoch, EpochSolution const*> const& solutions, Epoch const& epoch) {
    auto epochSolutionIt = solutions.find(epoch);
    STORM_LOG_ASSERT(epochSolutionIt != solutions.end(), "Requested unexisting solution for epoch " << epochManager.toString(epoch) << ".");
    return *epochSolutionIt->second;
}

template<typename ValueType, bool SingleObjectiveMode>
typename MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::SolutionType const&
MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getStateSolution(EpochSolution const& epochSolution, uint64_t const& productState) {
    STORM_LOG_ASSERT(productState < epochSolution.productStateToSolutionVectorMap->size(), "Requested solution at an unexisting product state.");
    STORM_LOG_ASSERT((*epochSolution.productStateToSolutionVectorMap)[productState] < epochSolution.solutions.size(),
                     "Requested solution for epoch at product state " << productState << " for which no solution was stored.");
    return epochSolution.solutions[(*epochSolution.productStateToSolutionVectorMap)[productState]];
}

template<typename ValueType, bool SingleObjectiveMode>
typename MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::SolutionType
MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getInitialStateResult(Epoch const& epoch) {
    STORM_LOG_ASSERT(model.getInitialStates().getNumberOfSetBits() == 1, "The model has multiple initial states.");
    return getInitialStateResult(epoch, *model.getInitialStates().begin());
}

template<typename ValueType, bool SingleObjectiveMode>
typename MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::SolutionType
MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getInitialStateResult(Epoch const& epoch, uint64_t initialStateIndex) {
    STORM_LOG_ASSERT(model.getInitialStates().get(initialStateIndex), "The given model state is not an initial state.");

    auto result = getStateSolution(epoch, productModel->getInitialProductState(initialStateIndex, model.getInitialStates(), epochManager.getEpochClass(epoch)));
    for (uint64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
        if (productModel->getProb1InitialStates(objIndex) && productModel->getProb1InitialStates(objIndex)->get(initialStateIndex)) {
            // Check whether the objective can actually hold in this epoch
            bool objectiveHolds = true;
            for (auto dim : objectiveDimensions[objIndex]) {
                if (dimensions[dim].boundType == DimensionBoundType::LowerBound && !epochManager.isBottomDimension(epoch, dim)) {
                    objectiveHolds = false;
                } else if (dimensions[dim].boundType == DimensionBoundType::UpperBound && epochManager.isBottomDimension(epoch, dim)) {
                    objectiveHolds = false;
                }
                STORM_LOG_ASSERT(dimensions[dim].boundType != DimensionBoundType::LowerBoundInfinity, "Unexpected bound type at this point.");
            }
            if (objectiveHolds) {
                setSolutionEntry(result, objIndex, storm::utility::one<ValueType>());
            }
        }
    }
    return result;
}

template<typename ValueType, bool SingleObjectiveMode>
EpochManager const& MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getEpochManager() const {
    return epochManager;
}

template<typename ValueType, bool SingleObjectiveMode>
Dimension<ValueType> const& MultiDimensionalRewardUnfolding<ValueType, SingleObjectiveMode>::getDimension(uint64_t dim) const {
    return dimensions.at(dim);
}

template class MultiDimensionalRewardUnfolding<double, true>;
template class MultiDimensionalRewardUnfolding<double, false>;
template class MultiDimensionalRewardUnfolding<storm::RationalNumber, true>;
template class MultiDimensionalRewardUnfolding<storm::RationalNumber, false>;
}  // namespace rewardbounded
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
