#include "storm-dft/builder/ExplicitDFTModelBuilder.h"

#include <map>

#include "storm-dft/settings/modules/FaultTreeSettings.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/transformer/NonMarkovianChainTransformer.h"
#include "storm/utility/ProgressMeasurement.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/bitoperations.h"
#include "storm/utility/constants.h"
#include "storm/utility/vector.h"

namespace storm::dft {
namespace builder {

template<typename ValueType, typename StateType>
ExplicitDFTModelBuilder<ValueType, StateType>::ModelComponents::ModelComponents()
    : transitionMatrix(), stateLabeling(), markovianStates(), exitRates(), choiceLabeling() {
    // Intentionally left empty.
}

template<typename ValueType, typename StateType>
ExplicitDFTModelBuilder<ValueType, StateType>::MatrixBuilder::MatrixBuilder(bool canHaveNondeterminism)
    : mappingOffset(0), stateRemapping(), currentRowGroup(0), currentRow(0), canHaveNondeterminism((canHaveNondeterminism)) {
    // Create matrix builder
    builder = storm::storage::SparseMatrixBuilder<ValueType>(0, 0, 0, false, canHaveNondeterminism, 0);
}

template<typename ValueType, typename StateType>
ExplicitDFTModelBuilder<ValueType, StateType>::ExplicitDFTModelBuilder(storm::dft::storage::DFT<ValueType> const& dft,
                                                                       storm::dft::storage::DftSymmetries const& symmetries)
    : dft(dft),
      stateGenerationInfo(std::make_shared<storm::dft::storage::DFTStateGenerationInfo>(dft.buildStateGenerationInfo(symmetries))),
      generator(dft, *stateGenerationInfo),
      matrixBuilder(!generator.isDeterministicModel()),
      stateStorage(dft.stateBitVectorSize()),
      explorationQueue(1, 0, 0.9, false) {
    // Set relevant events
    STORM_LOG_DEBUG("Relevant events: " << this->dft.getRelevantEventsString());
    if (dft.getRelevantEvents().size() <= 1) {
        STORM_LOG_ASSERT(dft.getRelevantEvents()[0] == dft.getTopLevelIndex(), "TLE is not relevant.");
        // Only interested in top level event -> introduce unique failed state
        this->uniqueFailedState = true;
        STORM_LOG_DEBUG("Using unique failed state with id 0.");
    }

    // Compute independent subtrees
    if (dft.getTopLevelType() == storm::dft::storage::elements::DFTElementType::OR) {
        // We only support this for approximation with top level element OR
        for (auto const& child : dft.getGate(dft.getTopLevelIndex())->children()) {
            // Consider all children of the top level gate
            std::vector<size_t> isubdft;
            if (child->nrParents() > 1 || child->hasOutgoingDependencies()) {
                STORM_LOG_TRACE("child " << child->name() << " does not allow modularisation.");
                isubdft.clear();
            } else if (dft.isGate(child->id())) {
                isubdft = dft.getGate(child->id())->independentSubDft(false);
            } else {
                STORM_LOG_ASSERT(dft.isBasicElement(child->id()), "Child is no BE.");
                if (dft.getBasicElement(child->id())->hasIngoingDependencies()) {
                    STORM_LOG_TRACE("child " << child->name() << " does not allow modularisation.");
                    isubdft.clear();
                } else {
                    isubdft = {child->id()};
                }
            }
            if (isubdft.empty()) {
                subtreeBEs.clear();
                break;
            } else {
                // Add new independent subtree
                std::vector<size_t> subtree;
                for (size_t id : isubdft) {
                    if (dft.isBasicElement(id)) {
                        subtree.push_back(id);
                    }
                }
                subtreeBEs.push_back(subtree);
            }
        }
    }
    if (subtreeBEs.empty()) {
        // Consider complete tree
        std::vector<size_t> subtree;
        for (size_t id = 0; id < dft.nrElements(); ++id) {
            if (dft.isBasicElement(id)) {
                subtree.push_back(id);
            }
        }
        subtreeBEs.push_back(subtree);
    }

    for (auto tree : subtreeBEs) {
        std::stringstream stream;
        stream << "Subtree: ";
        for (size_t id : tree) {
            stream << id << ", ";
        }
        STORM_LOG_TRACE(stream.str());
    }
}

template<typename ValueType, typename StateType>
void ExplicitDFTModelBuilder<ValueType, StateType>::buildModel(size_t iteration, double approximationThreshold,
                                                               storm::dft::builder::ApproximationHeuristic approximationHeuristic) {
    STORM_LOG_TRACE("Generating DFT state space");
    usedHeuristic = approximationHeuristic;

    if (approximationThreshold > 0 && !this->uniqueFailedState) {
        // Approximation requires unique failed states
        // TODO lift this restriction
        STORM_LOG_WARN("Approximation requires unique failed state. Forcing use of unique failed state.");
        this->uniqueFailedState = true;
    }

    if (iteration < 1) {
        // Initialize
        switch (usedHeuristic) {
            case storm::dft::builder::ApproximationHeuristic::DEPTH:
                explorationQueue = storm::dft::storage::BucketPriorityQueue<ExplorationHeuristic>(dft.nrElements() + 1, 0, 0.9, false);
                break;
            case storm::dft::builder::ApproximationHeuristic::PROBABILITY:
                explorationQueue = storm::dft::storage::BucketPriorityQueue<ExplorationHeuristic>(200, 0, 0.9, true);
                break;
            case storm::dft::builder::ApproximationHeuristic::BOUNDDIFFERENCE:
                explorationQueue = storm::dft::storage::BucketPriorityQueue<ExplorationHeuristic>(200, 0, 0.9, true);
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Heuristic not known.");
        }
        modelComponents.markovianStates = storm::storage::BitVector(INITIAL_BITVECTOR_SIZE);

        if (this->uniqueFailedState) {
            // Introduce explicit fail state
            storm::generator::StateBehavior<ValueType, StateType> behavior = generator.createMergeFailedState([this](DFTStatePointer const& state) {
                size_t failedStateId = newIndex++;
                STORM_LOG_ASSERT(failedStateId == 0, "Unique failed id is not 0.");
                matrixBuilder.stateRemapping.push_back(0);
                return failedStateId;
            });

            matrixBuilder.setRemapping(0);
            STORM_LOG_ASSERT(!behavior.empty(), "Behavior is empty.");
            matrixBuilder.newRowGroup();
            setMarkovian(behavior.begin()->isMarkovian());

            // Now add self loop.
            // TODO: maybe use general method.
            STORM_LOG_ASSERT(behavior.getNumberOfChoices() == 1, "Wrong number of choices for failed state.");
            STORM_LOG_ASSERT(behavior.begin()->size() == 1, "Wrong number of transitions for failed state.");
            std::pair<StateType, ValueType> stateProbabilityPair = *(behavior.begin()->begin());
            STORM_LOG_ASSERT(stateProbabilityPair.first == 0, "No self loop for failed state.");
            STORM_LOG_ASSERT(storm::utility::isOne<ValueType>(stateProbabilityPair.second), "Probability for failed state != 1.");
            matrixBuilder.addTransition(stateProbabilityPair.first, stateProbabilityPair.second);
            matrixBuilder.finishRow();
        }

        // Build initial state
        this->stateStorage.initialStateIndices =
            generator.getInitialStates(std::bind(&ExplicitDFTModelBuilder::getOrAddStateIndex, this, std::placeholders::_1));
        STORM_LOG_ASSERT(stateStorage.initialStateIndices.size() == 1, "Only one initial state assumed.");
        initialStateIndex = stateStorage.initialStateIndices[0];
        STORM_LOG_TRACE("Initial state: " << initialStateIndex);

        // DFT may be instantly failed due to a constant failure
        // in this case a model only consisting of the uniqueFailedState suffices
        if (initialStateIndex == 0 && this->uniqueFailedState) {
            modelComponents.markovianStates.resize(1);
            modelComponents.deterministicModel = generator.isDeterministicModel();

            STORM_LOG_TRACE("Markovian states: " << modelComponents.markovianStates);
            STORM_LOG_DEBUG("Model has 1 state");
            STORM_LOG_DEBUG("Model is " << (generator.isDeterministicModel() ? "deterministic" : "non-deterministic"));

            // Build transition matrix
            modelComponents.transitionMatrix = matrixBuilder.builder.build(1, 1);
            STORM_LOG_TRACE("Transition matrix: \n" << modelComponents.transitionMatrix);

            buildLabeling();
            return;
        }

        // Initialize heuristic values for inital state
        STORM_LOG_ASSERT(!statesNotExplored.at(initialStateIndex).second, "Heuristic for initial state is already initialized.");
        ExplorationHeuristicPointer heuristic;
        switch (usedHeuristic) {
            case storm::dft::builder::ApproximationHeuristic::DEPTH:
                heuristic = std::make_shared<DFTExplorationHeuristicDepth<ValueType>>(initialStateIndex);
                break;
            case storm::dft::builder::ApproximationHeuristic::PROBABILITY:
                heuristic = std::make_shared<DFTExplorationHeuristicProbability<ValueType>>(initialStateIndex);
                break;
            case storm::dft::builder::ApproximationHeuristic::BOUNDDIFFERENCE:
                heuristic = std::make_shared<DFTExplorationHeuristicBoundDifference<ValueType>>(initialStateIndex);
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Heuristic not known.");
        }
        heuristic->markExpand();
        statesNotExplored[initialStateIndex].second = heuristic;
        explorationQueue.push(heuristic);
    } else {
        initializeNextIteration();
    }

    if (approximationThreshold > 0.0) {
        switch (usedHeuristic) {
            case storm::dft::builder::ApproximationHeuristic::DEPTH:
                approximationThreshold = iteration + 1;
                break;
            case storm::dft::builder::ApproximationHeuristic::PROBABILITY:
            case storm::dft::builder::ApproximationHeuristic::BOUNDDIFFERENCE:
                approximationThreshold = std::pow(2, -(double)iteration);  // Need conversion to avoid overflow when negating
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Heuristic not known.");
        }
    }

    auto ftSettings = storm::settings::getModule<storm::dft::settings::modules::FaultTreeSettings>();
    if (ftSettings.isMaxDepthSet()) {
        STORM_LOG_ASSERT(usedHeuristic == storm::dft::builder::ApproximationHeuristic::DEPTH, "MaxDepth requires 'depth' exploration heuristic.");
        approximationThreshold = ftSettings.getMaxDepth();
    }

    exploreStateSpace(approximationThreshold);

    size_t stateSize = stateStorage.getNumberOfStates() + (this->uniqueFailedState ? 1 : 0);
    modelComponents.markovianStates.resize(stateSize);
    modelComponents.deterministicModel = generator.isDeterministicModel();

    // Fix the entries in the transition matrix according to the mapping of ids to row group indices
    STORM_LOG_ASSERT(matrixBuilder.getRemapping(initialStateIndex) == initialStateIndex, "Initial state should not be remapped.");
    // TODO: do not consider all rows?
    STORM_LOG_TRACE("Remap matrix: " << matrixBuilder.stateRemapping << ", offset: " << matrixBuilder.mappingOffset);
    matrixBuilder.remap();

    STORM_LOG_TRACE("State remapping: " << matrixBuilder.stateRemapping);
    STORM_LOG_TRACE("Markovian states: " << modelComponents.markovianStates);
    STORM_LOG_DEBUG("Model has " << stateSize << " states");
    STORM_LOG_DEBUG("Model is " << (generator.isDeterministicModel() ? "deterministic" : "non-deterministic"));

    // Build transition matrix
    modelComponents.transitionMatrix = matrixBuilder.builder.build(stateSize, stateSize);
    if (stateSize <= 15) {
        STORM_LOG_TRACE("Transition matrix: \n" << modelComponents.transitionMatrix);
    } else {
        STORM_LOG_TRACE("Transition matrix: too big to print");
    }

    buildLabeling();
}

template<typename ValueType, typename StateType>
void ExplicitDFTModelBuilder<ValueType, StateType>::initializeNextIteration() {
    STORM_LOG_TRACE("Refining DFT state space");

    // TODO: should be easier now as skipped states all are at the end of the matrix
    // Push skipped states to explore queue
    // TODO: remove
    for (auto const& skippedState : skippedStates) {
        statesNotExplored[skippedState.second.first->getId()] = skippedState.second;
        explorationQueue.push(skippedState.second.second);
    }

    // Initialize matrix builder again
    // TODO: avoid copy
    std::vector<uint_fast64_t> copyRemapping = matrixBuilder.stateRemapping;
    matrixBuilder = MatrixBuilder(!generator.isDeterministicModel());
    matrixBuilder.stateRemapping = copyRemapping;
    StateType nrStates = modelComponents.transitionMatrix.getRowGroupCount();
    STORM_LOG_ASSERT(nrStates == matrixBuilder.stateRemapping.size(), "No. of states does not coincide with mapping size.");

    // Start by creating a remapping from the old indices to the new indices
    std::vector<StateType> indexRemapping = std::vector<StateType>(nrStates, 0);
    auto iterSkipped = skippedStates.begin();
    size_t skippedBefore = 0;
    for (size_t i = 0; i < indexRemapping.size(); ++i) {
        while (iterSkipped != skippedStates.end() && iterSkipped->first <= i) {
            STORM_LOG_ASSERT(iterSkipped->first < indexRemapping.size(), "Skipped is too high.");
            ++skippedBefore;
            ++iterSkipped;
        }
        indexRemapping[i] = i - skippedBefore;
    }

    // Set remapping
    size_t nrExpandedStates = nrStates - skippedBefore;
    matrixBuilder.mappingOffset = nrStates;
    STORM_LOG_TRACE("# expanded states: " << nrExpandedStates);
    StateType skippedIndex = nrExpandedStates;
    std::map<StateType, std::pair<DFTStatePointer, ExplorationHeuristicPointer>> skippedStatesNew;
    for (size_t id = 0; id < matrixBuilder.stateRemapping.size(); ++id) {
        StateType index = matrixBuilder.getRemapping(id);
        auto itFind = skippedStates.find(index);
        if (itFind != skippedStates.end()) {
            // Set new mapping for skipped state
            matrixBuilder.stateRemapping[id] = skippedIndex;
            skippedStatesNew[skippedIndex] = itFind->second;
            indexRemapping[index] = skippedIndex;
            ++skippedIndex;
        } else {
            // Set new mapping for expanded state
            matrixBuilder.stateRemapping[id] = indexRemapping[index];
        }
    }
    STORM_LOG_TRACE("New state remapping: " << matrixBuilder.stateRemapping);
    std::stringstream ss;
    ss << "Index remapping:\n";
    for (auto tmp : indexRemapping) {
        ss << tmp << " ";
    }
    STORM_LOG_TRACE(ss.str());

    // Remap markovian states
    storm::storage::BitVector markovianStatesNew = storm::storage::BitVector(modelComponents.markovianStates.size(), true);
    // Iterate over all not set bits
    modelComponents.markovianStates.complement();
    size_t index = modelComponents.markovianStates.getNextSetIndex(0);
    while (index < modelComponents.markovianStates.size()) {
        markovianStatesNew.set(indexRemapping[index], false);
        index = modelComponents.markovianStates.getNextSetIndex(index + 1);
    }
    STORM_LOG_ASSERT(modelComponents.markovianStates.size() - modelComponents.markovianStates.getNumberOfSetBits() == markovianStatesNew.getNumberOfSetBits(),
                     "Remapping of markovian states is wrong.");
    STORM_LOG_ASSERT(markovianStatesNew.size() == nrStates, "No. of states does not coincide with markovian size.");
    modelComponents.markovianStates = markovianStatesNew;

    // Build submatrix for expanded states
    // TODO: only use row groups when necessary
    for (StateType oldRowGroup = 0; oldRowGroup < modelComponents.transitionMatrix.getRowGroupCount(); ++oldRowGroup) {
        if (indexRemapping[oldRowGroup] < nrExpandedStates) {
            // State is expanded -> copy to new matrix
            matrixBuilder.newRowGroup();
            for (StateType oldRow = modelComponents.transitionMatrix.getRowGroupIndices()[oldRowGroup];
                 oldRow < modelComponents.transitionMatrix.getRowGroupIndices()[oldRowGroup + 1]; ++oldRow) {
                for (typename storm::storage::SparseMatrix<ValueType>::const_iterator itEntry = modelComponents.transitionMatrix.begin(oldRow);
                     itEntry != modelComponents.transitionMatrix.end(oldRow); ++itEntry) {
                    auto itFind = skippedStates.find(itEntry->getColumn());
                    if (itFind != skippedStates.end()) {
                        // Set id for skipped states as we remap it later
                        matrixBuilder.addTransition(matrixBuilder.mappingOffset + itFind->second.first->getId(), itEntry->getValue());
                    } else {
                        // Set newly remapped index for expanded states
                        matrixBuilder.addTransition(indexRemapping[itEntry->getColumn()], itEntry->getValue());
                    }
                }
                matrixBuilder.finishRow();
            }
        }
    }

    skippedStates = skippedStatesNew;

    STORM_LOG_ASSERT(matrixBuilder.getCurrentRowGroup() == nrExpandedStates, "Row group size does not match.");
    skippedStates.clear();
}

template<typename ValueType, typename StateType>
void ExplicitDFTModelBuilder<ValueType, StateType>::exploreStateSpace(double approximationThreshold) {
    size_t nrExpandedStates = 0;
    size_t nrSkippedStates = 0;
    storm::utility::ProgressMeasurement progress("explored states",
                                                 storm::settings::getModule<storm::settings::modules::GeneralSettings>().getShowProgressDelay());
    progress.startNewMeasurement(0);
    // TODO: do not empty queue every time but break before
    while (!explorationQueue.empty()) {
        // Get the first state in the queue
        ExplorationHeuristicPointer currentExplorationHeuristic = explorationQueue.pop();
        StateType currentId = currentExplorationHeuristic->getId();
        auto itFind = statesNotExplored.find(currentId);
        STORM_LOG_ASSERT(itFind != statesNotExplored.end(), "Id " << currentId << " not found");
        DFTStatePointer currentState = itFind->second.first;
        STORM_LOG_ASSERT(currentExplorationHeuristic == itFind->second.second, "Exploration heuristics do not match.");
        STORM_LOG_ASSERT(currentState->getId() == currentId, "Ids do not match.");
        // Remove it from the list of not explored states
        statesNotExplored.erase(itFind);
        STORM_LOG_ASSERT(stateStorage.stateToId.contains(currentState->status()), "State is not contained in state storage.");
        STORM_LOG_ASSERT(stateStorage.stateToId.getValue(currentState->status()) == currentId, "Ids of states do not coincide.");

        // Get concrete state if necessary
        if (currentState->isPseudoState()) {
            // Create concrete state from pseudo state
            currentState->construct();
        }
        STORM_LOG_ASSERT(!currentState->isPseudoState(), "State is pseudo state.");

        // Remember that the current row group was actually filled with the transitions of a different state
        matrixBuilder.setRemapping(currentId);

        matrixBuilder.newRowGroup();

        // Try to explore the next state
        generator.load(currentState);

        // if (approximationThreshold > 0.0 && nrExpandedStates > approximationThreshold && !currentExplorationHeuristic->isExpand()) {
        if (approximationThreshold > 0.0 && currentExplorationHeuristic->isSkip(approximationThreshold)) {
            // Skip the current state
            ++nrSkippedStates;
            STORM_LOG_TRACE("Skip expansion of state: " << dft.getStateString(currentState));
            setMarkovian(true);
            // Add transition to target state with temporary value 0
            // TODO: what to do when there is no unique target state?
            // STORM_LOG_ASSERT(this->uniqueFailedState, "Approximation only works with unique failed state.");
            matrixBuilder.addTransition(0, storm::utility::zero<ValueType>());
            // Remember skipped state
            skippedStates[matrixBuilder.getCurrentRowGroup() - 1] = std::make_pair(currentState, currentExplorationHeuristic);
            matrixBuilder.finishRow();
        } else {
            // Explore the current state
            ++nrExpandedStates;
            storm::generator::StateBehavior<ValueType, StateType> behavior =
                generator.expand(std::bind(&ExplicitDFTModelBuilder::getOrAddStateIndex, this, std::placeholders::_1));
            STORM_LOG_ASSERT(!behavior.empty(), "Behavior is empty.");
            setMarkovian(behavior.begin()->isMarkovian());

            // Now add all choices.
            for (auto const& choice : behavior) {
                // Add the probabilistic behavior to the matrix.
                for (auto const& stateProbabilityPair : choice) {
                    STORM_LOG_ASSERT(!storm::utility::isZero(stateProbabilityPair.second), "Probability zero.");
                    // Set transition to state id + offset. This helps in only remapping all previously skipped states.
                    matrixBuilder.addTransition(matrixBuilder.mappingOffset + stateProbabilityPair.first, stateProbabilityPair.second);
                    // Set heuristic values for reached states
                    auto iter = statesNotExplored.find(stateProbabilityPair.first);
                    if (iter != statesNotExplored.end()) {
                        // Update heuristic values
                        DFTStatePointer state = iter->second.first;
                        if (!iter->second.second) {
                            // Initialize heuristic values
                            ExplorationHeuristicPointer heuristic;
                            switch (usedHeuristic) {
                                case storm::dft::builder::ApproximationHeuristic::DEPTH:
                                    heuristic =
                                        std::make_shared<DFTExplorationHeuristicDepth<ValueType>>(stateProbabilityPair.first, *currentExplorationHeuristic);
                                    break;
                                case storm::dft::builder::ApproximationHeuristic::PROBABILITY:
                                    heuristic = std::make_shared<DFTExplorationHeuristicProbability<ValueType>>(
                                        stateProbabilityPair.first, *currentExplorationHeuristic, stateProbabilityPair.second, choice.getTotalMass());
                                    break;
                                case storm::dft::builder::ApproximationHeuristic::BOUNDDIFFERENCE:
                                    heuristic = std::make_shared<DFTExplorationHeuristicBoundDifference<ValueType>>(
                                        stateProbabilityPair.first, *currentExplorationHeuristic, stateProbabilityPair.second, choice.getTotalMass());
                                    break;
                                default:
                                    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Heuristic not known.");
                            }

                            iter->second.second = heuristic;
                            // if (state->hasFailed(dft.getTopLevelIndex()) || state->isFailsafe(dft.getTopLevelIndex()) ||
                            // state->getFailableElements().hasDependencies() || (!state->getFailableElements().hasDependencies() &&
                            // !state->getFailableElements().hasBEs())) {
                            if (state->getFailableElements().hasDependencies() ||
                                (!state->getFailableElements().hasDependencies() && !state->getFailableElements().hasBEs())) {
                                // Do not skip absorbing state or if reached by dependencies
                                iter->second.second->markExpand();
                            }
                            if (usedHeuristic == storm::dft::builder::ApproximationHeuristic::BOUNDDIFFERENCE) {
                                // Compute bounds for heuristic now
                                if (state->isPseudoState()) {
                                    // Create concrete state from pseudo state
                                    state->construct();
                                }
                                STORM_LOG_ASSERT(!currentState->isPseudoState(), "State is pseudo state.");

                                // Initialize bounds
                                // TODO: avoid hack
                                ValueType lowerBound = getLowerBound(state);
                                ValueType upperBound = getUpperBound(state);
                                heuristic->setBounds(lowerBound, upperBound);
                            }

                            explorationQueue.push(heuristic);
                        } else if (!iter->second.second->isExpand()) {
                            bool changedPriority = false;
                            double oldPriority = iter->second.second->getPriority();
                            switch (usedHeuristic) {
                                case storm::dft::builder::ApproximationHeuristic::DEPTH:
                                    changedPriority = iter->second.second->updateHeuristicValues(*currentExplorationHeuristic,
                                                                                                 /* next values are irrelevant */ stateProbabilityPair.second,
                                                                                                 stateProbabilityPair.second);
                                    break;
                                case storm::dft::builder::ApproximationHeuristic::PROBABILITY:
                                    changedPriority = iter->second.second->updateHeuristicValues(*currentExplorationHeuristic, stateProbabilityPair.second,
                                                                                                 choice.getTotalMass());
                                    break;
                                case storm::dft::builder::ApproximationHeuristic::BOUNDDIFFERENCE:
                                    changedPriority = iter->second.second->updateHeuristicValues(*currentExplorationHeuristic, stateProbabilityPair.second,
                                                                                                 choice.getTotalMass());
                                    break;
                                default:
                                    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentException, "Heuristic not known.");
                            }
                            if (changedPriority) {
                                // Update priority queue
                                explorationQueue.update(iter->second.second, oldPriority);
                            }
                        }
                    }
                }
                matrixBuilder.finishRow();
            }
        }
        if (storm::utility::resources::isTerminate()) {
            break;
        }
        // Output number of currently explored states
        if (nrExpandedStates % 100 == 0) {
            progress.updateProgress(nrExpandedStates);
        }
    }  // end exploration

    STORM_LOG_INFO("Expanded " << nrExpandedStates << " states");
    STORM_LOG_INFO("Skipped " << nrSkippedStates << " states");
    STORM_LOG_ASSERT(nrSkippedStates == skippedStates.size(), "Nr skipped states is wrong.");
}

template<typename ValueType, typename StateType>
void ExplicitDFTModelBuilder<ValueType, StateType>::buildLabeling() {
    bool isAddLabelsClaiming = storm::settings::getModule<storm::dft::settings::modules::FaultTreeSettings>().isAddLabelsClaiming();

    // Build state labeling
    modelComponents.stateLabeling = storm::models::sparse::StateLabeling(modelComponents.transitionMatrix.getRowGroupCount());
    // Initial state
    modelComponents.stateLabeling.addLabel("init");
    STORM_LOG_ASSERT(matrixBuilder.getRemapping(initialStateIndex) == initialStateIndex, "Initial state should not be remapped.");
    modelComponents.stateLabeling.addLabelToState("init", initialStateIndex);
    // System failure
    modelComponents.stateLabeling.addLabel("failed");

    // Label all states corresponding to their status (failed, don't care BE)
    // Collect labels for all necessary elements
    for (size_t id = 0; id < dft.nrElements(); ++id) {
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(id);
        if (element->isRelevant()) {
            modelComponents.stateLabeling.addLabel(element->name() + "_failed");
            modelComponents.stateLabeling.addLabel(element->name() + "_dc");
        }
    }
    std::vector<std::shared_ptr<storm::dft::storage::elements::DFTGate<ValueType> const>> spares;  // Only filled if needed
    if (isAddLabelsClaiming) {
        // Collect labels for claiming
        for (size_t spareId : dft.getSpareIndices()) {
            auto const& spare = dft.getGate(spareId);
            spares.push_back(spare);
            for (auto const& child : spare->children()) {
                modelComponents.stateLabeling.addLabel(spare->name() + "_claimed_" + child->name());
            }
        }
    }

    // Set labels to states
    if (this->uniqueFailedState) {
        // Unique failed state has label 0
        modelComponents.stateLabeling.addLabelToState("failed", 0);
        std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(dft.getTopLevelIndex());
        STORM_LOG_ASSERT(element->isRelevant(), "TLE should be relevant if unique failed state is used.");
        modelComponents.stateLabeling.addLabelToState(element->name() + "_failed", 0);
    }
    for (auto const& stateIdPair : stateStorage.stateToId) {
        storm::storage::BitVector state = stateIdPair.first;
        size_t stateId = matrixBuilder.getRemapping(stateIdPair.second);
        if (dft.hasFailed(state, *stateGenerationInfo)) {
            modelComponents.stateLabeling.addLabelToState("failed", stateId);
        }
        // Set failed/don't care status for each necessary element
        for (size_t id = 0; id < dft.nrElements(); ++id) {
            std::shared_ptr<storm::dft::storage::elements::DFTElement<ValueType> const> element = dft.getElement(id);
            if (element->isRelevant()) {
                storm::dft::storage::DFTElementState elementState =
                    storm::dft::storage::DFTState<ValueType>::getElementState(state, *stateGenerationInfo, element->id());
                switch (elementState) {
                    case storm::dft::storage::DFTElementState::Failed:
                        modelComponents.stateLabeling.addLabelToState(element->name() + "_failed", stateId);
                        break;
                    case storm::dft::storage::DFTElementState::DontCare:
                        modelComponents.stateLabeling.addLabelToState(element->name() + "_dc", stateId);
                        break;
                    case storm::dft::storage::DFTElementState::Operational:
                    case storm::dft::storage::DFTElementState::Failsafe:
                        // do nothing
                        break;
                    default:
                        STORM_LOG_ASSERT(false, "Unknown element state " << elementState);
                }
            }
        }
        if (isAddLabelsClaiming) {
            for (auto const& spare : spares) {
                size_t claimedChildId = dft.uses(state, *stateGenerationInfo, spare->id());
                if (claimedChildId != spare->id()) {
                    modelComponents.stateLabeling.addLabelToState(spare->name() + "_claimed_" + dft.getElement(claimedChildId)->name(), stateId);
                }
            }
        }
    }

    STORM_LOG_TRACE(modelComponents.stateLabeling);
}

template<typename ValueType, typename StateType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> ExplicitDFTModelBuilder<ValueType, StateType>::getModel() {
    if (storm::settings::getModule<storm::dft::settings::modules::FaultTreeSettings>().isMaxDepthSet() && skippedStates.size() > 0) {
        // Give skipped states separate label "skipped"
        modelComponents.stateLabeling.addLabel("skipped");
        for (auto it = skippedStates.begin(); it != skippedStates.end(); ++it) {
            modelComponents.stateLabeling.addLabelToState("skipped", it->first);
        }
    } else {
        STORM_LOG_ASSERT(skippedStates.size() == 0, "Concrete model has skipped states.");
    }

    return createModel(false);
}

template<typename ValueType, typename StateType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> ExplicitDFTModelBuilder<ValueType, StateType>::getModelApproximation(bool lowerBound,
                                                                                                                              bool expectedTime) {
    if (expectedTime) {
        // TODO: handle case with no skipped states
        changeMatrixBound(modelComponents.transitionMatrix, lowerBound);
        return createModel(true);
    } else {
        // Change model for probabilities
        // TODO: make nicer
        storm::storage::SparseMatrix<ValueType> matrix = modelComponents.transitionMatrix;
        storm::models::sparse::StateLabeling labeling = modelComponents.stateLabeling;
        if (lowerBound) {
            // Set self loop for lower bound
            for (auto it = skippedStates.begin(); it != skippedStates.end(); ++it) {
                auto matrixEntry = matrix.getRow(it->first, 0).begin();
                STORM_LOG_ASSERT(matrixEntry->getColumn() == 0, "Transition has wrong target state.");
                STORM_LOG_ASSERT(!it->second.first->isPseudoState(), "State is still pseudo state.");
                matrixEntry->setValue(storm::utility::one<ValueType>());
                matrixEntry->setColumn(it->first);
            }
        } else {
            // Make skipped states failed states for upper bound
            storm::storage::BitVector failedStates = modelComponents.stateLabeling.getStates("failed");
            for (auto it = skippedStates.begin(); it != skippedStates.end(); ++it) {
                failedStates.set(it->first);
            }
            labeling.setStates("failed", failedStates);
        }

        std::shared_ptr<storm::models::sparse::Model<ValueType>> model;
        if (modelComponents.deterministicModel) {
            model = std::make_shared<storm::models::sparse::Ctmc<ValueType>>(std::move(matrix), std::move(labeling));
        } else {
            // Build MA
            // Compute exit rates
            // TODO: avoid computing multiple times
            modelComponents.exitRates = std::vector<ValueType>(modelComponents.markovianStates.size());
            std::vector<typename storm::storage::SparseMatrix<ValueType>::index_type> indices = matrix.getRowGroupIndices();
            for (StateType stateIndex = 0; stateIndex < modelComponents.markovianStates.size(); ++stateIndex) {
                if (modelComponents.markovianStates[stateIndex]) {
                    modelComponents.exitRates[stateIndex] = matrix.getRowSum(indices[stateIndex]);
                } else {
                    modelComponents.exitRates[stateIndex] = storm::utility::zero<ValueType>();
                }
            }
            STORM_LOG_TRACE("Exit rates: " << modelComponents.exitRates);

            storm::storage::sparse::ModelComponents<ValueType> maComponents(std::move(matrix), std::move(labeling));
            maComponents.rateTransitions = true;
            maComponents.markovianStates = modelComponents.markovianStates;
            maComponents.exitRates = modelComponents.exitRates;
            std::shared_ptr<storm::models::sparse::MarkovAutomaton<ValueType>> ma =
                std::make_shared<storm::models::sparse::MarkovAutomaton<ValueType>>(std::move(maComponents));
            if (ma->hasOnlyTrivialNondeterminism()) {
                // Markov automaton can be converted into CTMC
                model = storm::transformer::NonMarkovianChainTransformer<ValueType>::eliminateNonmarkovianStates(
                    ma, storm::transformer::EliminationLabelBehavior::ExtendLabels);
            } else {
                model = ma;
            }
        }

        if (model->getNumberOfStates() <= 15) {
            STORM_LOG_TRACE("Transition matrix: \n" << model->getTransitionMatrix());
        } else {
            STORM_LOG_TRACE("Transition matrix: too big to print");
        }
        return model;
    }
}

template<typename ValueType, typename StateType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> ExplicitDFTModelBuilder<ValueType, StateType>::createModel(bool copy) {
    std::shared_ptr<storm::models::sparse::Model<ValueType>> model;

    if (modelComponents.deterministicModel) {
        // Build CTMC
        if (copy) {
            model = std::make_shared<storm::models::sparse::Ctmc<ValueType>>(modelComponents.transitionMatrix, modelComponents.stateLabeling);
        } else {
            model =
                std::make_shared<storm::models::sparse::Ctmc<ValueType>>(std::move(modelComponents.transitionMatrix), std::move(modelComponents.stateLabeling));
        }
    } else {
        // Build MA
        // Compute exit rates
        // TODO: avoid computing multiple times
        modelComponents.exitRates = std::vector<ValueType>(modelComponents.markovianStates.size());
        std::vector<typename storm::storage::SparseMatrix<ValueType>::index_type> indices = modelComponents.transitionMatrix.getRowGroupIndices();
        for (StateType stateIndex = 0; stateIndex < modelComponents.markovianStates.size(); ++stateIndex) {
            if (modelComponents.markovianStates[stateIndex]) {
                modelComponents.exitRates[stateIndex] = modelComponents.transitionMatrix.getRowSum(indices[stateIndex]);
            } else {
                modelComponents.exitRates[stateIndex] = storm::utility::zero<ValueType>();
            }
        }
        STORM_LOG_TRACE("Exit rates: " << modelComponents.exitRates);

        std::shared_ptr<storm::models::sparse::MarkovAutomaton<ValueType>> ma;
        if (copy) {
            storm::storage::sparse::ModelComponents<ValueType> maComponents(modelComponents.transitionMatrix, modelComponents.stateLabeling);
            maComponents.rateTransitions = true;
            maComponents.markovianStates = modelComponents.markovianStates;
            maComponents.exitRates = modelComponents.exitRates;
            ma = std::make_shared<storm::models::sparse::MarkovAutomaton<ValueType>>(std::move(maComponents));
        } else {
            storm::storage::sparse::ModelComponents<ValueType> maComponents(std::move(modelComponents.transitionMatrix),
                                                                            std::move(modelComponents.stateLabeling));
            maComponents.rateTransitions = true;
            maComponents.markovianStates = std::move(modelComponents.markovianStates);
            maComponents.exitRates = std::move(modelComponents.exitRates);
            ma = std::make_shared<storm::models::sparse::MarkovAutomaton<ValueType>>(std::move(maComponents));
        }
        if (ma->hasOnlyTrivialNondeterminism()) {
            // Markov automaton can be converted into CTMC
            model = storm::transformer::NonMarkovianChainTransformer<ValueType>::eliminateNonmarkovianStates(
                ma, storm::transformer::EliminationLabelBehavior::ExtendLabels);
        } else {
            model = ma;
        }
    }

    if (model->getNumberOfStates() <= 15) {
        STORM_LOG_TRACE("Transition matrix: \n" << model->getTransitionMatrix());
    } else {
        STORM_LOG_TRACE("Transition matrix: too big to print");
    }
    return model;
}

template<typename ValueType, typename StateType>
void ExplicitDFTModelBuilder<ValueType, StateType>::changeMatrixBound(storm::storage::SparseMatrix<ValueType>& matrix, bool lowerBound) const {
    // Set lower bound for skipped states
    for (auto it = skippedStates.begin(); it != skippedStates.end(); ++it) {
        auto matrixEntry = matrix.getRow(it->first, 0).begin();
        STORM_LOG_ASSERT(matrixEntry->getColumn() == 0, "Transition has wrong target state.");
        STORM_LOG_ASSERT(!it->second.first->isPseudoState(), "State is still pseudo state.");

        // Change bound
        // TODO: cache values inbetween iterations
        if (lowerBound) {
            matrixEntry->setValue(getLowerBound(it->second.first));
        } else {
            matrixEntry->setValue(getUpperBound(it->second.first));
        }
    }
}

template<typename ValueType, typename StateType>
ValueType ExplicitDFTModelBuilder<ValueType, StateType>::getLowerBound(DFTStatePointer const& state) const {
    // Get the lower bound by considering the failure of all possible BEs
    ValueType lowerBound = storm::utility::zero<ValueType>();
    STORM_LOG_ASSERT(!state->getFailableElements().hasDependencies(), "Lower bound should only be computed if dependencies were already handled.");
    for (auto it = state->getFailableElements().begin(); it != state->getFailableElements().end(); ++it) {
        lowerBound += state->getBERate(*it);
    }
    STORM_LOG_TRACE("Lower bound is " << lowerBound << " for state " << state->getId());
    return lowerBound;
}

template<typename ValueType, typename StateType>
ValueType ExplicitDFTModelBuilder<ValueType, StateType>::getUpperBound(DFTStatePointer const& state) const {
    if (state->hasFailed(dft.getTopLevelIndex())) {
        return storm::utility::zero<ValueType>();
    }

    // Get the upper bound by considering the failure of all BEs
    ValueType upperBound = storm::utility::one<ValueType>();
    ValueType rateSum = storm::utility::zero<ValueType>();

    // Compute for each independent subtree
    for (std::vector<size_t> const& subtree : subtreeBEs) {
        // Get all possible rates
        std::vector<ValueType> rates;
        storm::storage::BitVector coldBEs(subtree.size(), false);
        for (size_t i = 0; i < subtree.size(); ++i) {
            size_t id = subtree[i];
            // Consider only still operational BEs
            if (state->isOperational(id)) {
                auto be = dft.getBasicElement(id);
                switch (be->beType()) {
                    case storm::dft::storage::elements::BEType::CONSTANT:
                        // Ignore BE which cannot fail
                        continue;
                    case storm::dft::storage::elements::BEType::EXPONENTIAL: {
                        // Get BE rate
                        ValueType rate = state->getBERate(id);
                        if (storm::utility::isZero<ValueType>(rate)) {
                            // Get active failure rate for cold BE
                            auto beExp = std::static_pointer_cast<storm::dft::storage::elements::BEExponential<ValueType> const>(be);
                            rate = beExp->activeFailureRate();
                            STORM_LOG_ASSERT(!storm::utility::isZero<ValueType>(rate), "Failure rate should not be zero.");
                            // Mark BE as cold
                            coldBEs.set(i, true);
                        }
                        rates.push_back(rate);
                        rateSum += rate;
                        break;
                    }
                    default:
                        STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "BE of type '" << be->beType() << "' is not known.");
                        break;
                }
            }
        }
        STORM_LOG_ASSERT(rates.size() > 0, "No rates failable.");

        // Sort rates
        std::sort(rates.begin(), rates.end());
        std::vector<size_t> rateCount(rates.size(), 0);
        size_t lastIndex = 0;
        for (size_t i = 0; i < rates.size(); ++i) {
            if (rates[lastIndex] != rates[i]) {
                lastIndex = i;
            }
            ++rateCount[lastIndex];
        }

        for (size_t i = 0; i < rates.size(); ++i) {
            // Cold BEs cannot fail in the first step
            if (!coldBEs.get(i) && rateCount[i] > 0) {
                std::iter_swap(rates.begin() + i, rates.end() - 1);
                // Compute AND MTTF of subtree without current rate and scale with current rate
                upperBound += rates.back() * rateCount[i] * computeMTTFAnd(rates, rates.size() - 1);
                // Swap back
                // TODO try to avoid swapping back
                std::iter_swap(rates.begin() + i, rates.end() - 1);
            }
        }
    }

    STORM_LOG_TRACE("Upper bound is " << (rateSum / upperBound) << " for state " << state->getId());
    STORM_LOG_ASSERT(!storm::utility::isZero(upperBound), "Upper bound is 0.");
    STORM_LOG_ASSERT(!storm::utility::isZero(rateSum), "State is absorbing.");
    return rateSum / upperBound;
}

template<typename ValueType, typename StateType>
ValueType ExplicitDFTModelBuilder<ValueType, StateType>::computeMTTFAnd(std::vector<ValueType> const& rates, size_t size) const {
    ValueType result = storm::utility::zero<ValueType>();
    if (size == 0) {
        return result;
    }

    // TODO: works only for <64 BEs!
    // WARNING: this code produces wrong results for more than 32 BEs
    /*for (size_t i = 1; i < 4 && i <= rates.size(); ++i) {
        size_t permutation = smallestIntWithNBitsSet(static_cast<size_t>(i));
        ValueType sum = storm::utility::zero<ValueType>();
        do {
            ValueType permResult = storm::utility::zero<ValueType>();
            for(size_t j = 0; j < rates.size(); ++j) {
                if(permutation & static_cast<size_t>(1 << static_cast<size_t>(j))) {
                    // WARNING: if the first bit is set, it also recognizes the 32nd bit as set
                    // TODO: fix
                    permResult += rates[j];
                }
            }
            permutation = nextBitPermutation(permutation);
            STORM_LOG_ASSERT(!storm::utility::isZero(permResult), "PermResult is 0.");
            sum += storm::utility::one<ValueType>() / permResult;
        } while(permutation < (static_cast<size_t>(1) << rates.size()) && permutation != 0);
        if (i % 2 == 0) {
            result -= sum;
        } else {
            result += sum;
        }
    }*/

    // Compute result with permutations of size <= 3
    ValueType one = storm::utility::one<ValueType>();
    for (size_t i1 = 0; i1 < size; ++i1) {
        // + 1/a
        ValueType sum = rates[i1];
        result += one / sum;
        for (size_t i2 = 0; i2 < i1; ++i2) {
            // - 1/(a+b)
            ValueType sum2 = sum + rates[i2];
            result -= one / sum2;
            for (size_t i3 = 0; i3 < i2; ++i3) {
                // + 1/(a+b+c)
                result += one / (sum2 + rates[i3]);
            }
        }
    }

    STORM_LOG_ASSERT(!storm::utility::isZero(result), "UpperBound is 0.");
    return result;
}

template<typename ValueType, typename StateType>
StateType ExplicitDFTModelBuilder<ValueType, StateType>::getOrAddStateIndex(DFTStatePointer const& state) {
    StateType stateId;
    bool changed = false;

    if (stateGenerationInfo->hasSymmetries()) {
        // Order state by symmetry
        STORM_LOG_TRACE("Check for symmetry: " << dft.getStateString(state));
        changed = state->orderBySymmetry();
        STORM_LOG_TRACE("State " << (changed ? "changed to " : "did not change") << (changed ? dft.getStateString(state) : ""));
    }

    if (stateStorage.stateToId.contains(state->status())) {
        // State already exists
        stateId = stateStorage.stateToId.getValue(state->status());
        STORM_LOG_TRACE("State " << dft.getStateString(state) << " with id " << stateId << " already exists");
        if (!changed) {
            // Check if state is pseudo state
            // If state is explored already the possible pseudo state was already constructed
            auto iter = statesNotExplored.find(stateId);
            if (iter != statesNotExplored.end() && iter->second.first->isPseudoState()) {
                // Create pseudo state now
                STORM_LOG_ASSERT(iter->second.first->getId() == stateId, "Ids do not match.");
                STORM_LOG_ASSERT(iter->second.first->status() == state->status(), "Pseudo states do not coincide.");
                state->setId(stateId);
                // Update mapping to map to concrete state now
                // TODO: just change pointer?
                statesNotExplored[stateId] = std::make_pair(state, iter->second.second);
                // We do not push the new state on the exploration queue as the pseudo state was already pushed
                STORM_LOG_TRACE("Created pseudo state " << dft.getStateString(state));
            }
        }
    } else {
        // State does not exist yet
        STORM_LOG_ASSERT(state->isPseudoState() == changed, "State type (pseudo/concrete) wrong.");
        // Create new state
        state->setId(newIndex++);
        stateId = stateStorage.stateToId.findOrAdd(state->status(), state->getId());
        STORM_LOG_ASSERT(stateId == state->getId(), "Ids do not match.");
        // Insert state as not yet explored
        ExplorationHeuristicPointer nullHeuristic;
        statesNotExplored[stateId] = std::make_pair(state, nullHeuristic);
        // Reserve one slot for the new state in the remapping
        matrixBuilder.stateRemapping.push_back(0);
        STORM_LOG_TRACE("New " << (state->isPseudoState() ? "pseudo" : "concrete") << " state: " << dft.getStateString(state));
    }
    return stateId;
}

template<typename ValueType, typename StateType>
void ExplicitDFTModelBuilder<ValueType, StateType>::setMarkovian(bool markovian) {
    if (matrixBuilder.getCurrentRowGroup() > modelComponents.markovianStates.size()) {
        // Resize BitVector
        modelComponents.markovianStates.resize(modelComponents.markovianStates.size() + INITIAL_BITVECTOR_SIZE);
    }
    modelComponents.markovianStates.set(matrixBuilder.getCurrentRowGroup() - 1, markovian);
}

template<typename ValueType, typename StateType>
void ExplicitDFTModelBuilder<ValueType, StateType>::printNotExplored() const {
    std::cout << "states not explored:\n";
    for (auto it : statesNotExplored) {
        std::cout << it.first << " -> " << dft.getStateString(it.second.first) << '\n';
    }
}

// Explicitly instantiate the class.
template class ExplicitDFTModelBuilder<double>;
template class ExplicitDFTModelBuilder<storm::RationalFunction>;

}  // namespace builder
}  // namespace storm::dft
