#include "storm/storage/bisimulation/BisimulationDecomposition.h"

#include <chrono>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/AbortException.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/InvalidOptionException.h"
#include "storm/logic/FormulaInformation.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/storage/bisimulation/DeterministicBlockData.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/macros.h"

namespace storm {
namespace storage {

using namespace bisimulation;

template<typename ModelType, typename BlockDataType>
BisimulationDecomposition<ModelType, BlockDataType>::Options::Options(ModelType const& model, storm::logic::Formula const& formula) : Options() {
    this->preserveSingleFormula(model, formula);
}

template<typename ModelType, typename BlockDataType>
BisimulationDecomposition<ModelType, BlockDataType>::Options::Options(ModelType const& model,
                                                                      std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas)
    : Options() {
    if (formulas.empty()) {
        this->respectedAtomicPropositions = model.getStateLabeling().getLabels();
        this->keepRewards = true;
    }
    if (formulas.size() == 1) {
        this->preserveSingleFormula(model, *formulas.front());
    } else {
        for (auto const& formula : formulas) {
            preserveFormula(*formula);
        }
    }
}

template<typename ModelType, typename BlockDataType>
BisimulationDecomposition<ModelType, BlockDataType>::Options::Options()
    : measureDrivenInitialPartition(false),
      phiStates(),
      psiStates(),
      respectedAtomicPropositions(),
      buildQuotient(true),
      keepRewards(false),
      type(BisimulationType::Strong),
      bounded(false),
      discounted(false) {
    // Intentionally left empty.
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::Options::preserveFormula(storm::logic::Formula const& formula) {
    // Disable the measure driven initial partition.
    measureDrivenInitialPartition = false;
    phiStates.reset();
    psiStates.reset();

    // Retrieve information about formula.
    storm::logic::FormulaInformation info = formula.info();

    // Preserve rewards if necessary.
    keepRewards = keepRewards || info.containsRewardOperator() || info.containsRewardBoundedFormula();

    // Preserve bounded properties if necessary.
    bounded = bounded || (info.containsBoundedUntilFormula() || info.containsNextFormula() || info.containsCumulativeRewardFormula());

    // Preserve discounted properties if necessary.
    discounted = discounted || info.containsDiscountFormula();

    // Compute the relevant labels and expressions.
    this->addToRespectedAtomicPropositions(formula.getAtomicExpressionFormulas(), formula.getAtomicLabelFormulas());
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::Options::preserveSingleFormula(ModelType const& model, storm::logic::Formula const& formula) {
    // Retrieve information about formula.
    storm::logic::FormulaInformation info = formula.info();

    keepRewards = info.containsRewardOperator() || info.containsRewardBoundedFormula();

    // We need to preserve bounded properties iff the formula contains a bounded until or a next subformula.
    bounded = info.containsBoundedUntilFormula() || info.containsNextFormula() || info.containsCumulativeRewardFormula();

    // We need to preserve discounting iff the formula contains a discounted subformula
    discounted = info.containsDiscountFormula();

    // Compute the relevant labels and expressions.
    this->addToRespectedAtomicPropositions(formula.getAtomicExpressionFormulas(), formula.getAtomicLabelFormulas());

    // Check whether measure driven initial partition is possible and, if so, set it.
    this->checkAndSetMeasureDrivenInitialPartition(model, formula);
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::Options::checkAndSetMeasureDrivenInitialPartition(ModelType const& model,
                                                                                                            storm::logic::Formula const& formula) {
    std::shared_ptr<storm::logic::Formula const> newFormula = formula.asSharedPointer();

    if (formula.isProbabilityOperatorFormula()) {
        if (formula.asProbabilityOperatorFormula().hasOptimalityType()) {
            optimalityType = formula.asProbabilityOperatorFormula().getOptimalityType();
        } else if (formula.asProbabilityOperatorFormula().hasBound()) {
            storm::logic::ComparisonType comparisonType = formula.asProbabilityOperatorFormula().getComparisonType();
            if (comparisonType == storm::logic::ComparisonType::Less || comparisonType == storm::logic::ComparisonType::LessEqual) {
                optimalityType = OptimizationDirection::Maximize;
            } else {
                optimalityType = OptimizationDirection::Minimize;
            }
        }
        newFormula = formula.asProbabilityOperatorFormula().getSubformula().asSharedPointer();
    } else if (formula.isRewardOperatorFormula()) {
        if (formula.asRewardOperatorFormula().hasOptimalityType()) {
            optimalityType = formula.asRewardOperatorFormula().getOptimalityType();
        } else if (formula.asRewardOperatorFormula().hasBound()) {
            storm::logic::ComparisonType comparisonType = formula.asRewardOperatorFormula().getComparisonType();
            if (comparisonType == storm::logic::ComparisonType::Less || comparisonType == storm::logic::ComparisonType::LessEqual) {
                optimalityType = OptimizationDirection::Maximize;
            } else {
                optimalityType = OptimizationDirection::Minimize;
            }
        }
        newFormula = formula.asRewardOperatorFormula().getSubformula().asSharedPointer();
    }

    std::shared_ptr<storm::logic::Formula const> leftSubformula = std::make_shared<storm::logic::BooleanLiteralFormula>(true);
    std::shared_ptr<storm::logic::Formula const> rightSubformula;
    if (newFormula->isUntilFormula()) {
        leftSubformula = newFormula->asUntilFormula().getLeftSubformula().asSharedPointer();
        rightSubformula = newFormula->asUntilFormula().getRightSubformula().asSharedPointer();
        if (leftSubformula->isInFragment(storm::logic::propositional()) && rightSubformula->isInFragment(storm::logic::propositional())) {
            measureDrivenInitialPartition = true;
        }
    } else if (newFormula->isEventuallyFormula()) {
        rightSubformula = newFormula->asEventuallyFormula().getSubformula().asSharedPointer();
        if (rightSubformula->isInFragment(storm::logic::propositional())) {
            measureDrivenInitialPartition = true;
        }
    }

    if (measureDrivenInitialPartition) {
        storm::modelchecker::SparsePropositionalModelChecker<ModelType> checker(model);
        std::unique_ptr<storm::modelchecker::CheckResult> phiStatesCheckResult = checker.check(*leftSubformula);
        std::unique_ptr<storm::modelchecker::CheckResult> psiStatesCheckResult = checker.check(*rightSubformula);

        using SolutionType = storm::IntervalBaseType<ValueType>;
        phiStates = phiStatesCheckResult->template asExplicitQualitativeCheckResult<SolutionType>().getTruthValuesVector();
        psiStates = psiStatesCheckResult->template asExplicitQualitativeCheckResult<SolutionType>().getTruthValuesVector();
    } else {
        optimalityType.reset();
    }
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::Options::addToRespectedAtomicPropositions(
    std::vector<std::shared_ptr<storm::logic::AtomicExpressionFormula const>> const& expressions,
    std::vector<std::shared_ptr<storm::logic::AtomicLabelFormula const>> const& labels) {
    std::set<std::string> labelsToRespect;
    for (auto const& labelFormula : labels) {
        labelsToRespect.insert(labelFormula->getLabel());
    }
    for (auto const& expressionFormula : expressions) {
        labelsToRespect.insert(expressionFormula->toString());
    }
    if (!respectedAtomicPropositions) {
        respectedAtomicPropositions = labelsToRespect;
    } else {
        respectedAtomicPropositions.value().insert(labelsToRespect.begin(), labelsToRespect.end());
    }
}

template<typename ModelType, typename BlockDataType>
BisimulationDecomposition<ModelType, BlockDataType>::BisimulationDecomposition(ModelType const& model, Options const& options)
    : BisimulationDecomposition(model, model.getBackwardTransitions(), options) {
    // Intentionally left empty.
}

template<typename ModelType, typename BlockDataType>
BisimulationDecomposition<ModelType, BlockDataType>::BisimulationDecomposition(ModelType const& model,
                                                                               storm::storage::SparseMatrix<ValueType> const& backwardTransitions,
                                                                               Options const& options)
    : model(model), backwardTransitions(backwardTransitions), options(options), partition(), comparator(options.getTolerance()), quotient(nullptr) {
    STORM_LOG_THROW(!options.getKeepRewards() || !model.hasRewardModel() || model.hasUniqueRewardModel(), storm::exceptions::IllegalFunctionCallException,
                    "Bisimulation currently only supports models with at most one reward model.");
    STORM_LOG_THROW(!options.getKeepRewards() || !model.hasRewardModel() || !model.getUniqueRewardModel().hasTransitionRewards(),
                    storm::exceptions::IllegalFunctionCallException,
                    "Bisimulation is currently supported for models with state or action rewards only. Consider converting the transition rewards to state "
                    "rewards (via suitable function calls).");
    STORM_LOG_THROW(options.getType() != BisimulationType::Weak || !options.getBounded(), storm::exceptions::IllegalFunctionCallException,
                    "Weak bisimulation cannot preserve bounded properties.");
    STORM_LOG_THROW(options.getType() != BisimulationType::Weak || !options.getDiscounted(), storm::exceptions::IllegalFunctionCallException,
                    "Weak bisimulation cannot preserve discounted properties.");

    // Fix the respected atomic propositions if they were not explicitly given.
    if (!this->options.respectedAtomicPropositions) {
        this->options.respectedAtomicPropositions = model.getStateLabeling().getLabels();
    }
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::computeBisimulationDecomposition() {
    std::chrono::high_resolution_clock::time_point totalStart = std::chrono::high_resolution_clock::now();

    std::chrono::high_resolution_clock::time_point initialPartitionStart = std::chrono::high_resolution_clock::now();
    // initialize the initial partition.
    if (options.measureDrivenInitialPartition) {
        STORM_LOG_THROW(options.phiStates, storm::exceptions::InvalidOptionException, "Unable to compute measure-driven initial partition without phi states.");
        STORM_LOG_THROW(options.psiStates, storm::exceptions::InvalidOptionException, "Unable to compute measure-driven initial partition without psi states.");
        this->initializeMeasureDrivenPartition();
    } else {
        this->initializeLabelBasedPartition();
    }
    STORM_LOG_WARN_COND(partition.size() > 1, "Initial partition consists only of a single block.");
    std::chrono::high_resolution_clock::duration initialPartitionTime = std::chrono::high_resolution_clock::now() - initialPartitionStart;

    this->initialize();

    std::chrono::high_resolution_clock::time_point refinementStart = std::chrono::high_resolution_clock::now();
    this->performPartitionRefinement();
    std::chrono::high_resolution_clock::duration refinementTime = std::chrono::high_resolution_clock::now() - refinementStart;

    std::chrono::high_resolution_clock::time_point extractionStart = std::chrono::high_resolution_clock::now();
    this->extractDecompositionBlocks();
    std::chrono::high_resolution_clock::duration extractionTime = std::chrono::high_resolution_clock::now() - extractionStart;

    std::chrono::high_resolution_clock::time_point quotientBuildStart = std::chrono::high_resolution_clock::now();
    if (options.buildQuotient) {
        this->buildQuotient();
    }
    std::chrono::high_resolution_clock::duration quotientBuildTime = std::chrono::high_resolution_clock::now() - quotientBuildStart;

    std::chrono::high_resolution_clock::duration totalTime = std::chrono::high_resolution_clock::now() - totalStart;

    if (storm::settings::getModule<storm::settings::modules::CoreSettings>().isShowStatisticsSet()) {
        std::chrono::milliseconds initialPartitionTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(initialPartitionTime);
        std::chrono::milliseconds refinementTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(refinementTime);
        std::chrono::milliseconds extractionTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(extractionTime);
        std::chrono::milliseconds quotientBuildTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(quotientBuildTime);
        std::chrono::milliseconds totalTimeInMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(totalTime);
        std::cout << "\nTime breakdown:\n";
        std::cout << "    * time for initial partition: " << initialPartitionTimeInMilliseconds.count() << "ms\n";
        std::cout << "    * time for partitioning: " << refinementTimeInMilliseconds.count() << "ms\n";
        std::cout << "    * time for extraction: " << extractionTimeInMilliseconds.count() << "ms\n";
        std::cout << "    * time for building quotient: " << quotientBuildTimeInMilliseconds.count() << "ms\n";
        std::cout << "------------------------------------------\n";
        std::cout << "    * total time: " << totalTimeInMilliseconds.count() << "ms\n\n";
    }
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::performPartitionRefinement() {
    // Insert all blocks into the splitter queue as a (potential) splitter.
    std::vector<Block<BlockDataType>*> splitterQueue;
    std::for_each(partition.getBlocks().begin(), partition.getBlocks().end(), [&](std::unique_ptr<Block<BlockDataType>> const& block) {
        block->data().setSplitter();
        splitterQueue.push_back(block.get());
    });

    // Then perform the actual splitting until there are no more splitters.
    uint_fast64_t iterations = 0;
    while (!splitterQueue.empty()) {
        ++iterations;

        // Get and prepare the next splitter.
        // Sort the splitters according to their sizes to prefer small splitters. That is just a heuristic, but
        // tends to work well.
        std::sort(splitterQueue.begin(), splitterQueue.end(),
                  [](Block<BlockDataType> const* b1, Block<BlockDataType> const* b2) { return b1->getNumberOfStates() > b2->getNumberOfStates(); });
        Block<BlockDataType>* splitter = splitterQueue.back();
        splitterQueue.pop_back();
        splitter->data().setSplitter(false);

        // Now refine the partition using the current splitter.
        refinePartitionBasedOnSplitter(*splitter, splitterQueue);

        if (storm::utility::resources::isTerminate()) {
            std::cout << "Performed " << iterations << " iterations of partition refinement before abort.\n";
            STORM_LOG_THROW(false, storm::exceptions::AbortException, "Aborted in bisimulation computation.");
            break;
        }
    }
}

template<typename ModelType, typename BlockDataType>
std::shared_ptr<ModelType> BisimulationDecomposition<ModelType, BlockDataType>::getQuotient() const {
    STORM_LOG_THROW(this->quotient != nullptr, storm::exceptions::IllegalFunctionCallException,
                    "Unable to retrieve quotient model from bisimulation decomposition, because it was not built.");
    return this->quotient;
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::splitInitialPartitionBasedOnRewards() {
    auto const& rewardModel = model.getUniqueRewardModel();
    if (rewardModel.hasStateRewards()) {
        this->splitInitialPartitionBasedOnRewards(rewardModel.getStateRewardVector());
    }
    if (rewardModel.hasStateActionRewards()) {
        if (model.isNondeterministicModel()) {
            std::vector<std::set<ValueType>> actionRewards;
            actionRewards.reserve(model.getNumberOfStates());
            for (storm::storage::sparse::state_type state = 0; state < model.getNumberOfStates(); ++state) {
                std::set<ValueType> rewardsAtState;
                for (auto choice = model.getTransitionMatrix().getRowGroupIndices()[state];
                     choice < model.getTransitionMatrix().getRowGroupIndices()[state + 1]; ++choice) {
                    rewardsAtState.insert(rewardModel.getStateActionReward(choice));
                }
                actionRewards.push_back(std::move(rewardsAtState));
            }
            this->splitInitialPartitionBasedOnActionRewards(actionRewards);
        } else {
            this->splitInitialPartitionBasedOnRewards(rewardModel.getStateActionRewardVector());
        }
    }
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::splitInitialPartitionBasedOnRewards(std::vector<ValueType> const& rewardVector) {
    partition.split([&rewardVector](storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b) {
        return rewardVector[a] < rewardVector[b];
    });
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::splitInitialPartitionBasedOnActionRewards(std::vector<std::set<ValueType>> const& actionRewards) {
    partition.split([&actionRewards](storm::storage::sparse::state_type const& a, storm::storage::sparse::state_type const& b) {
        return actionRewards[a] < actionRewards[b];
    });
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::initializeLabelBasedPartition() {
    partition = storm::storage::bisimulation::Partition<BlockDataType>(model.getNumberOfStates());

    for (auto const& label : options.respectedAtomicPropositions.value()) {
        if (label == "init") {
            continue;
        }
        partition.splitStates(model.getStates(label));
    }

    // If the model has state rewards, we need to consider them, because otherwise reward properties are not
    // preserved.
    if (options.getKeepRewards() && model.hasRewardModel()) {
        this->splitInitialPartitionBasedOnRewards();
    }
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::initializeMeasureDrivenPartition() {
    std::pair<storm::storage::BitVector, storm::storage::BitVector> statesWithProbability01 = this->getStatesWithProbability01();

    std::optional<storm::storage::sparse::state_type> representativePsiState;
    if (!options.psiStates.value().empty()) {
        representativePsiState = *options.psiStates.value().begin();
    }

    partition = storm::storage::bisimulation::Partition<BlockDataType>(
        model.getNumberOfStates(), statesWithProbability01.first,
        options.getBounded() || options.getKeepRewards() ? options.psiStates.value() : statesWithProbability01.second, representativePsiState);

    // If the model has state rewards, we need to consider them, because otherwise reward properties are not
    // preserved.
    if (options.getKeepRewards() && model.hasRewardModel()) {
        this->splitInitialPartitionBasedOnRewards();
    }
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::initialize() {
    // Intentionally left empty.
}

template<typename ModelType, typename BlockDataType>
void BisimulationDecomposition<ModelType, BlockDataType>::extractDecompositionBlocks() {
    // Now move the states from the internal partition into their final place in the decomposition. We do so in
    // a way that maintains the block IDs as indices.
    this->blocks.resize(partition.size());
    for (auto const& blockPtr : partition.getBlocks()) {
        // We need to sort the states to allow for rapid construction of the blocks.
        partition.sortBlock(*blockPtr);

        // Convert the state-value-pairs to states only.
        this->blocks[blockPtr->getId()] = block_type(partition.begin(*blockPtr), partition.end(*blockPtr), true);
    }
}

template class BisimulationDecomposition<storm::models::sparse::Dtmc<double>, bisimulation::DeterministicBlockData>;
template class BisimulationDecomposition<storm::models::sparse::Ctmc<double>, bisimulation::DeterministicBlockData>;
template class BisimulationDecomposition<storm::models::sparse::Mdp<double>, bisimulation::DeterministicBlockData>;

template class BisimulationDecomposition<storm::models::sparse::Dtmc<storm::RationalNumber>, bisimulation::DeterministicBlockData>;
template class BisimulationDecomposition<storm::models::sparse::Ctmc<storm::RationalNumber>, bisimulation::DeterministicBlockData>;
template class BisimulationDecomposition<storm::models::sparse::Mdp<storm::RationalNumber>, bisimulation::DeterministicBlockData>;

template class BisimulationDecomposition<storm::models::sparse::Dtmc<storm::RationalFunction>, bisimulation::DeterministicBlockData>;
template class BisimulationDecomposition<storm::models::sparse::Ctmc<storm::RationalFunction>, bisimulation::DeterministicBlockData>;
template class BisimulationDecomposition<storm::models::sparse::Mdp<storm::RationalFunction>, bisimulation::DeterministicBlockData>;
}  // namespace storage
}  // namespace storm
