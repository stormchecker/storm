#include "storm/modelchecker/prctl/helper/rewardbounded/ProductModel.h"

#include "storm/logic/CloneVisitor.h"
#include "storm/logic/Formulas.h"
#include "storm/storage/memorystructure/MemoryStructureBuilder.h"
#include "storm/storage/memorystructure/SparseModelMemoryProduct.h"
#include "storm/utility/macros.h"

#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace rewardbounded {

template<typename ValueType>
ProductModel<ValueType>::ProductModel(storm::models::sparse::Model<ValueType> const& model,
                                      std::vector<storm::modelchecker::multiobjective::Objective<ValueType>> const& objectives,
                                      std::vector<Dimension<ValueType>> const& dimensions, std::vector<storm::storage::BitVector> const& objectiveDimensions,
                                      EpochManager const& epochManager, std::vector<Epoch> const& originalModelSteps)
    : dimensions(dimensions),
      objectiveDimensions(objectiveDimensions),
      epochManager(epochManager),
      memoryStateManager(dimensions.size()),
      prob1InitialStates(objectives.size(), boost::none) {
    for (uint64_t dim = 0; dim < dimensions.size(); ++dim) {
        if (!dimensions[dim].memoryLabel) {
            memoryStateManager.setDimensionWithoutMemory(dim);
        }
    }

    storm::storage::MemoryStructure memory = computeMemoryStructure(model, objectives);
    STORM_LOG_ASSERT(memoryStateManager.getMemoryStateCount() == memory.getNumberOfStates(), "Memory state count mismatch.");
    std::vector<MemoryState> memoryStateMap = computeMemoryStateMap(memory);

    storm::storage::SparseModelMemoryProduct<ValueType> productBuilder(memory.product(model));

    setReachableProductStates(productBuilder, originalModelSteps, memoryStateMap);
    product = productBuilder.build();

    uint64_t numModelStates = productBuilder.getOriginalModel().getNumberOfStates();
    MemoryState upperMemStateBound = memoryStateManager.getUpperMemoryStateBound();
    uint64_t numMemoryStates = memoryStateManager.getMemoryStateCount();
    uint64_t numProductStates = getProduct().getNumberOfStates();

    // Compute a mappings from product states to model/memory states and back
    modelMemoryToProductStateMap.resize(upperMemStateBound * numModelStates, std::numeric_limits<uint64_t>::max());
    productToModelStateMap.resize(numProductStates, std::numeric_limits<uint64_t>::max());
    productToMemoryStateMap.resize(numProductStates, std::numeric_limits<uint64_t>::max());
    for (uint64_t modelState = 0; modelState < numModelStates; ++modelState) {
        for (uint64_t memoryStateIndex = 0; memoryStateIndex < numMemoryStates; ++memoryStateIndex) {
            if (productBuilder.isStateReachable(modelState, memoryStateIndex)) {
                uint64_t productState = productBuilder.getResultState(modelState, memoryStateIndex);
                modelMemoryToProductStateMap[modelState * upperMemStateBound + memoryStateMap[memoryStateIndex]] = productState;
                productToModelStateMap[productState] = modelState;
                productToMemoryStateMap[productState] = memoryStateMap[memoryStateIndex];
            }
        }
    }

    // Map choice indices of the product to the state where it origins
    choiceToStateMap.reserve(getProduct().getTransitionMatrix().getRowCount());
    for (uint64_t productState = 0; productState < numProductStates; ++productState) {
        uint64_t groupSize = getProduct().getTransitionMatrix().getRowGroupSize(productState);
        for (uint64_t i = 0; i < groupSize; ++i) {
            choiceToStateMap.push_back(productState);
        }
    }

    // Compute the epoch steps for the product
    steps.resize(getProduct().getTransitionMatrix().getRowCount(), 0);
    for (uint64_t modelState = 0; modelState < numModelStates; ++modelState) {
        uint64_t numChoices = productBuilder.getOriginalModel().getTransitionMatrix().getRowGroupSize(modelState);
        uint64_t firstChoice = productBuilder.getOriginalModel().getTransitionMatrix().getRowGroupIndices()[modelState];
        for (uint64_t choiceOffset = 0; choiceOffset < numChoices; ++choiceOffset) {
            Epoch const& step = originalModelSteps[firstChoice + choiceOffset];
            if (step != 0) {
                for (MemoryState const& memoryState : memoryStateMap) {
                    if (productStateExists(modelState, memoryState)) {
                        uint64_t productState = getProductState(modelState, memoryState);
                        uint64_t productChoice = getProduct().getTransitionMatrix().getRowGroupIndices()[productState] + choiceOffset;
                        STORM_LOG_ASSERT(productChoice < getProduct().getTransitionMatrix().getRowGroupIndices()[productState + 1],
                                         "Product choice out of range.");
                        steps[productChoice] = step;
                    }
                }
            }
        }
    }

    //  getProduct().writeDotToStream(std::cout);

    computeReachableStatesInEpochClasses();
}

template<typename ValueType>
storm::storage::MemoryStructure ProductModel<ValueType>::computeMemoryStructure(
    storm::models::sparse::Model<ValueType> const& model, std::vector<storm::modelchecker::multiobjective::Objective<ValueType>> const& objectives) {
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<ValueType>> mc(model);

    // Create a memory structure that remembers whether (sub)objectives are satisfied
    storm::storage::MemoryStructure memory = storm::storage::MemoryStructureBuilder<ValueType>::buildTrivialMemoryStructure(model);
    for (uint64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
        if (!objectives[objIndex].formula->isProbabilityOperatorFormula()) {
            continue;
        }

        std::vector<uint64_t> dimensionIndexMap;
        for (auto globalDimensionIndex : objectiveDimensions[objIndex]) {
            dimensionIndexMap.push_back(globalDimensionIndex);
        }

        // collect the memory states for this objective
        std::vector<storm::storage::BitVector> objMemStates;
        storm::storage::BitVector m(dimensionIndexMap.size(), false);
        for (; !m.full(); m.increment()) {
            objMemStates.push_back(~m);
        }
        objMemStates.push_back(~m);
        STORM_LOG_ASSERT(objMemStates.size() == 1ull << dimensionIndexMap.size(), "Memory states size mismatch.");

        // build objective memory
        auto objMemoryBuilder = storm::storage::MemoryStructureBuilder<ValueType>(objMemStates.size(), model);

        // Get the set of states that for all subobjectives satisfy either the left or the right subformula
        storm::storage::BitVector constraintStates(model.getNumberOfStates(), true);
        for (auto dim : objectiveDimensions[objIndex]) {
            auto const& dimension = dimensions[dim];
            STORM_LOG_ASSERT(dimension.formula->isBoundedUntilFormula(), "Unexpected Formula type.");
            constraintStates &= (mc.check(dimension.formula->asBoundedUntilFormula().getLeftSubformula())
                                     ->template asExplicitQualitativeCheckResult<ValueType>()
                                     .getTruthValuesVector() |
                                 mc.check(dimension.formula->asBoundedUntilFormula().getRightSubformula())
                                     ->template asExplicitQualitativeCheckResult<ValueType>()
                                     .getTruthValuesVector());
        }

        // Build the transitions between the memory states
        for (uint64_t memState = 0; memState < objMemStates.size(); ++memState) {
            auto const& memStateBV = objMemStates[memState];
            for (uint64_t memStatePrime = 0; memStatePrime < objMemStates.size(); ++memStatePrime) {
                auto const& memStatePrimeBV = objMemStates[memStatePrime];
                if (memStatePrimeBV.isSubsetOf(memStateBV)) {
                    std::shared_ptr<storm::logic::Formula const> transitionFormula = storm::logic::Formula::getTrueFormula();
                    for (auto subObjIndex : memStateBV) {
                        std::shared_ptr<storm::logic::Formula const> subObjFormula =
                            dimensions[dimensionIndexMap[subObjIndex]].formula->asBoundedUntilFormula().getRightSubformula().asSharedPointer();
                        if (memStatePrimeBV.get(subObjIndex)) {
                            subObjFormula = std::make_shared<storm::logic::UnaryBooleanStateFormula>(storm::logic::UnaryBooleanStateFormula::OperatorType::Not,
                                                                                                     subObjFormula);
                        }
                        transitionFormula = std::make_shared<storm::logic::BinaryBooleanStateFormula>(
                            storm::logic::BinaryBooleanStateFormula::OperatorType::And, transitionFormula, subObjFormula);
                    }

                    storm::storage::BitVector transitionStates =
                        mc.check(*transitionFormula)->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
                    if (memStatePrimeBV.empty()) {
                        transitionStates |= ~constraintStates;
                    } else {
                        transitionStates &= constraintStates;
                    }
                    objMemoryBuilder.setTransition(memState, memStatePrime, transitionStates);

                    // Set the initial states
                    if (memStateBV.full()) {
                        storm::storage::BitVector initialTransitionStates = model.getInitialStates() & transitionStates;
                        // At this point we can check whether there is an initial state that already satisfies all subObjectives.
                        // Such a situation can not be reduced (easily) to an expected reward computation and thus requires special treatment
                        if (memStatePrimeBV.empty() && !initialTransitionStates.empty()) {
                            prob1InitialStates[objIndex] = initialTransitionStates;
                        }

                        for (auto initState : initialTransitionStates) {
                            objMemoryBuilder.setInitialMemoryState(initState, memStatePrime);
                        }
                    }
                }
            }
        }

        // Build the memory labels
        for (uint64_t memState = 0; memState < objMemStates.size(); ++memState) {
            auto const& memStateBV = objMemStates[memState];
            for (auto subObjIndex : memStateBV) {
                objMemoryBuilder.setLabel(memState, dimensions[dimensionIndexMap[subObjIndex]].memoryLabel.get());
            }
        }
        auto objMemory = objMemoryBuilder.build();
        memory = memory.product(objMemory);
    }
    return memory;
}

template<typename ValueType>
std::vector<typename ProductModel<ValueType>::MemoryState> ProductModel<ValueType>::computeMemoryStateMap(storm::storage::MemoryStructure const& memory) const {
    // Compute a mapping between the different representations of memory states
    std::vector<MemoryState> result;
    result.reserve(memory.getNumberOfStates());
    for (uint64_t memStateIndex = 0; memStateIndex < memory.getNumberOfStates(); ++memStateIndex) {
        MemoryState memState = memoryStateManager.getInitialMemoryState();
        std::set<std::string> stateLabels = memory.getStateLabeling().getLabelsOfState(memStateIndex);
        for (uint64_t dim = 0; dim < epochManager.getDimensionCount(); ++dim) {
            if (dimensions[dim].memoryLabel) {
                if (stateLabels.find(dimensions[dim].memoryLabel.get()) != stateLabels.end()) {
                    memoryStateManager.setRelevantDimension(memState, dim, true);
                } else {
                    memoryStateManager.setRelevantDimension(memState, dim, false);
                }
            }
        }
        result.push_back(std::move(memState));
    }
    return result;
}

template<typename ValueType>
void ProductModel<ValueType>::setReachableProductStates(storm::storage::SparseModelMemoryProduct<ValueType>& productBuilder,
                                                        std::vector<Epoch> const& originalModelSteps, std::vector<MemoryState> const& memoryStateMap) const {
    std::vector<uint64_t> inverseMemoryStateMap(memoryStateManager.getUpperMemoryStateBound(), std::numeric_limits<uint64_t>::max());
    for (uint64_t memStateIndex = 0; memStateIndex < memoryStateMap.size(); ++memStateIndex) {
        inverseMemoryStateMap[memoryStateMap[memStateIndex]] = memStateIndex;
    }

    auto const& memory = productBuilder.getMemory();
    auto const& model = productBuilder.getOriginalModel();
    auto const& modelTransitions = model.getTransitionMatrix();

    std::vector<storm::storage::BitVector> reachableProductStates(memoryStateManager.getUpperMemoryStateBound());
    for (auto const& memState : memoryStateMap) {
        reachableProductStates[memState] = storm::storage::BitVector(model.getNumberOfStates(), false);
    }

    // Initialize the reachable states with the initial states
    // If the bound is not known (e.g., when computing quantiles) we don't know the initial epoch class. Hence, we consider all possibilities.
    std::vector<EpochClass> initEpochClasses;
    initEpochClasses.push_back(epochManager.getEpochClass(epochManager.getZeroEpoch()));
    for (uint64_t dim = 0; dim < dimensions.size(); ++dim) {
        Dimension<ValueType> const& dimension = dimensions[dim];
        if (dimension.boundType == DimensionBoundType::Unbounded) {
            // For unbounded dimensions we are only interested in the bottom class.
            for (auto& ec : initEpochClasses) {
                epochManager.setDimensionOfEpochClass(ec, dim, true);
            }
        } else if (!dimension.maxValue) {
            // If no max value is known, we have to consider all possibilities
            std::vector<EpochClass> newEcs = initEpochClasses;
            for (auto& ec : newEcs) {
                epochManager.setDimensionOfEpochClass(ec, dim, true);
            }
            initEpochClasses.insert(initEpochClasses.end(), newEcs.begin(), newEcs.end());
        }
    }
    for (auto const& initEpochClass : initEpochClasses) {
        auto memStateIt = memory.getInitialMemoryStates().begin();
        for (auto initState : model.getInitialStates()) {
            uint64_t transformedMemoryState = transformMemoryState(memoryStateMap[*memStateIt], initEpochClass, memoryStateManager.getInitialMemoryState());
            reachableProductStates[transformedMemoryState].set(initState, true);
            ++memStateIt;
        }
        STORM_LOG_ASSERT(memStateIt == memory.getInitialMemoryStates().end(), "Memory state iterator not at end.");
    }

    // Find the reachable epoch classes
    std::set<Epoch> possibleSteps(originalModelSteps.begin(), originalModelSteps.end());
    std::set<EpochClass, std::function<bool(EpochClass const&, EpochClass const&)>> reachableEpochClasses(
        std::bind(&EpochManager::epochClassOrder, &epochManager, std::placeholders::_1, std::placeholders::_2));
    collectReachableEpochClasses(reachableEpochClasses, possibleSteps);

    // Iterate over all epoch classes starting from the initial one (i.e., no bottom dimension).
    for (auto epochClassIt = reachableEpochClasses.rbegin(); epochClassIt != reachableEpochClasses.rend(); ++epochClassIt) {
        auto const& epochClass = *epochClassIt;

        // Find the remaining set of reachable states via DFS.
        std::vector<std::pair<uint64_t, MemoryState>> dfsStack;
        for (MemoryState const& memState : memoryStateMap) {
            for (auto modelState : reachableProductStates[memState]) {
                dfsStack.emplace_back(modelState, memState);
            }
        }

        while (!dfsStack.empty()) {
            uint64_t currentModelState = dfsStack.back().first;
            MemoryState currentMemoryState = dfsStack.back().second;
            uint64_t currentMemoryStateIndex = inverseMemoryStateMap[currentMemoryState];
            dfsStack.pop_back();

            for (uint64_t choice = modelTransitions.getRowGroupIndices()[currentModelState];
                 choice != modelTransitions.getRowGroupIndices()[currentModelState + 1]; ++choice) {
                for (auto transitionIt = modelTransitions.getRow(choice).begin(); transitionIt < modelTransitions.getRow(choice).end(); ++transitionIt) {
                    MemoryState successorMemoryState =
                        memoryStateMap[memory.getSuccessorMemoryState(currentMemoryStateIndex, transitionIt - modelTransitions.begin())];
                    successorMemoryState = transformMemoryState(successorMemoryState, epochClass, currentMemoryState);
                    if (!reachableProductStates[successorMemoryState].get(transitionIt->getColumn())) {
                        reachableProductStates[successorMemoryState].set(transitionIt->getColumn(), true);
                        dfsStack.emplace_back(transitionIt->getColumn(), successorMemoryState);
                    }
                }
            }
        }
    }

    for (uint64_t memStateIndex = 0; memStateIndex < memoryStateManager.getMemoryStateCount(); ++memStateIndex) {
        for (auto modelState : reachableProductStates[memoryStateMap[memStateIndex]]) {
            productBuilder.addReachableState(modelState, memStateIndex);
        }
    }
}

template<typename ValueType>
storm::models::sparse::Model<ValueType> const& ProductModel<ValueType>::getProduct() const {
    return *product;
}

template<typename ValueType>
std::vector<typename ProductModel<ValueType>::Epoch> const& ProductModel<ValueType>::getSteps() const {
    return steps;
}

template<typename ValueType>
bool ProductModel<ValueType>::productStateExists(uint64_t const& modelState, MemoryState const& memoryState) const {
    return modelMemoryToProductStateMap[modelState * memoryStateManager.getUpperMemoryStateBound() + memoryState] < getProduct().getNumberOfStates();
}

template<typename ValueType>
uint64_t ProductModel<ValueType>::getProductState(uint64_t const& modelState, MemoryState const& memoryState) const {
    STORM_LOG_ASSERT(productStateExists(modelState, memoryState), "Tried to obtain state (" << modelState << ", " << memoryStateManager.toString(memoryState)
                                                                                            << ") in the model-memory-product which does not exist");
    return modelMemoryToProductStateMap[modelState * memoryStateManager.getUpperMemoryStateBound() + memoryState];
}

template<typename ValueType>
uint64_t ProductModel<ValueType>::getInitialProductState(uint64_t const& initialModelState, storm::storage::BitVector const& initialModelStates,
                                                         EpochClass const& epochClass) const {
    auto productInitStateIt = getProduct().getInitialStates().begin();
    productInitStateIt += initialModelStates.getNumberOfSetBitsBeforeIndex(initialModelState);
    STORM_LOG_ASSERT(getModelState(*productInitStateIt) == initialModelState, "Could not find the corresponding initial state in the product model.");
    return transformProductState(*productInitStateIt, epochClass, memoryStateManager.getInitialMemoryState());
}

template<typename ValueType>
uint64_t ProductModel<ValueType>::getModelState(uint64_t const& productState) const {
    return productToModelStateMap[productState];
}

template<typename ValueType>
typename ProductModel<ValueType>::MemoryState ProductModel<ValueType>::getMemoryState(uint64_t const& productState) const {
    return productToMemoryStateMap[productState];
}

template<typename ValueType>
MemoryStateManager const& ProductModel<ValueType>::getMemoryStateManager() const {
    return memoryStateManager;
}

template<typename ValueType>
uint64_t ProductModel<ValueType>::getProductStateFromChoice(uint64_t const& productChoice) const {
    return choiceToStateMap[productChoice];
}

template<typename ValueType>
std::vector<std::vector<ValueType>> ProductModel<ValueType>::computeObjectiveRewards(
    EpochClass const& epochClass, std::vector<storm::modelchecker::multiobjective::Objective<ValueType>> const& objectives) const {
    std::vector<std::vector<ValueType>> objectiveRewards;
    objectiveRewards.reserve(objectives.size());

    for (uint64_t objIndex = 0; objIndex < objectives.size(); ++objIndex) {
        auto const& formula = *objectives[objIndex].formula;
        if (formula.isProbabilityOperatorFormula()) {
            storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<ValueType>> mc(getProduct());
            std::vector<uint64_t> dimensionIndexMap;
            for (auto globalDimensionIndex : objectiveDimensions[objIndex]) {
                dimensionIndexMap.push_back(globalDimensionIndex);
            }

            std::shared_ptr<storm::logic::Formula const> sinkStatesFormula;
            for (auto dim : objectiveDimensions[objIndex]) {
                auto memLabelFormula = std::make_shared<storm::logic::AtomicLabelFormula>(dimensions[dim].memoryLabel.get());
                if (sinkStatesFormula) {
                    sinkStatesFormula = std::make_shared<storm::logic::BinaryBooleanStateFormula>(storm::logic::BinaryBooleanStateFormula::OperatorType::Or,
                                                                                                  sinkStatesFormula, memLabelFormula);
                } else {
                    sinkStatesFormula = memLabelFormula;
                }
            }
            sinkStatesFormula =
                std::make_shared<storm::logic::UnaryBooleanStateFormula>(storm::logic::UnaryBooleanStateFormula::OperatorType::Not, sinkStatesFormula);

            std::vector<ValueType> objRew(getProduct().getTransitionMatrix().getRowCount(), storm::utility::zero<ValueType>());
            storm::storage::BitVector relevantObjectives(objectiveDimensions[objIndex].getNumberOfSetBits());

            while (!relevantObjectives.full()) {
                relevantObjectives.increment();

                // find out whether objective reward should be earned within this epoch class
                bool collectRewardInEpoch = true;
                for (auto subObjIndex : relevantObjectives) {
                    if (dimensions[dimensionIndexMap[subObjIndex]].boundType == DimensionBoundType::UpperBound &&
                        epochManager.isBottomDimensionEpochClass(epochClass, dimensionIndexMap[subObjIndex])) {
                        collectRewardInEpoch = false;
                        break;
                    }
                }

                if (collectRewardInEpoch) {
                    std::shared_ptr<storm::logic::Formula const> relevantStatesFormula;
                    std::shared_ptr<storm::logic::Formula const> goalStatesFormula = storm::logic::CloneVisitor().clone(*sinkStatesFormula);
                    for (uint64_t subObjIndex = 0; subObjIndex < dimensionIndexMap.size(); ++subObjIndex) {
                        std::shared_ptr<storm::logic::Formula> memLabelFormula =
                            std::make_shared<storm::logic::AtomicLabelFormula>(dimensions[dimensionIndexMap[subObjIndex]].memoryLabel.get());
                        if (relevantObjectives.get(subObjIndex)) {
                            auto rightSubFormula =
                                dimensions[dimensionIndexMap[subObjIndex]].formula->asBoundedUntilFormula().getRightSubformula().asSharedPointer();
                            goalStatesFormula = std::make_shared<storm::logic::BinaryBooleanStateFormula>(
                                storm::logic::BinaryBooleanStateFormula::OperatorType::And, goalStatesFormula, rightSubFormula);
                        } else {
                            memLabelFormula = std::make_shared<storm::logic::UnaryBooleanStateFormula>(
                                storm::logic::UnaryBooleanStateFormula::OperatorType::Not, memLabelFormula);
                        }
                        if (relevantStatesFormula) {
                            relevantStatesFormula = std::make_shared<storm::logic::BinaryBooleanStateFormula>(
                                storm::logic::BinaryBooleanStateFormula::OperatorType::And, relevantStatesFormula, memLabelFormula);
                        } else {
                            relevantStatesFormula = memLabelFormula;
                        }
                    }

                    storm::storage::BitVector relevantStates =
                        mc.check(*relevantStatesFormula)->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
                    storm::storage::BitVector relevantChoices = getProduct().getTransitionMatrix().getRowFilter(relevantStates);
                    storm::storage::BitVector goalStates =
                        mc.check(*goalStatesFormula)->template asExplicitQualitativeCheckResult<ValueType>().getTruthValuesVector();
                    for (auto choice : relevantChoices) {
                        objRew[choice] += getProduct().getTransitionMatrix().getConstrainedRowSum(choice, goalStates);
                    }
                }
            }

            objectiveRewards.push_back(std::move(objRew));

        } else if (formula.isRewardOperatorFormula()) {
            auto const& rewModel = getProduct().getRewardModel(formula.asRewardOperatorFormula().getRewardModelName());
            STORM_LOG_THROW(!rewModel.hasTransitionRewards(), storm::exceptions::NotSupportedException,
                            "Reward model has transition rewards which is not expected.");
            bool rewardCollectedInEpoch = true;
            if (formula.getSubformula().isCumulativeRewardFormula()) {
                for (auto dim : objectiveDimensions[objIndex]) {
                    if (epochManager.isBottomDimensionEpochClass(epochClass, dim)) {
                        rewardCollectedInEpoch = false;
                        break;
                    }
                }
            } else {
                STORM_LOG_THROW(formula.getSubformula().isTotalRewardFormula(), storm::exceptions::UnexpectedException,
                                "Unexpected type of formula " << formula << ".");
            }
            if (rewardCollectedInEpoch) {
                objectiveRewards.push_back(rewModel.getTotalRewardVector(getProduct().getTransitionMatrix()));
            } else {
                objectiveRewards.emplace_back(getProduct().getTransitionMatrix().getRowCount(), storm::utility::zero<ValueType>());
            }
        } else {
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type of formula " << formula << ".");
        }
    }

    return objectiveRewards;
}

template<typename ValueType>
storm::storage::BitVector const& ProductModel<ValueType>::getInStates(EpochClass const& epochClass) const {
    STORM_LOG_ASSERT(inStates.find(epochClass) != inStates.end(), "Could not find InStates for the given epoch class.");
    return inStates.find(epochClass)->second;
}

template<typename ValueType>
void ProductModel<ValueType>::computeReachableStatesInEpochClasses() {
    std::set<Epoch> possibleSteps(steps.begin(), steps.end());
    std::set<EpochClass, std::function<bool(EpochClass const&, EpochClass const&)>> reachableEpochClasses(
        std::bind(&EpochManager::epochClassOrder, &epochManager, std::placeholders::_1, std::placeholders::_2));

    collectReachableEpochClasses(reachableEpochClasses, possibleSteps);

    for (auto epochClassIt = reachableEpochClasses.rbegin(); epochClassIt != reachableEpochClasses.rend(); ++epochClassIt) {
        std::vector<EpochClass> predecessors;
        for (auto predecessorIt = reachableEpochClasses.rbegin(); predecessorIt != epochClassIt; ++predecessorIt) {
            if (epochManager.isPredecessorEpochClass(*predecessorIt, *epochClassIt)) {
                predecessors.push_back(*predecessorIt);
            }
        }
        computeReachableStates(*epochClassIt, predecessors);
    }
}

template<typename ValueType>
void ProductModel<ValueType>::collectReachableEpochClasses(
    std::set<EpochClass, std::function<bool(EpochClass const&, EpochClass const&)>>& reachableEpochClasses, std::set<Epoch> const& possibleSteps) const {
    // Get the start epoch according to the given bounds.
    // For dimensions for which no bound (aka. maxValue) is known, we will later overapproximate the set of reachable classes.
    Epoch startEpoch = epochManager.getZeroEpoch();
    for (uint64_t dim = 0; dim < epochManager.getDimensionCount(); ++dim) {
        if (dimensions[dim].maxValue) {
            epochManager.setDimensionOfEpoch(startEpoch, dim, dimensions[dim].maxValue.get());
        } else {
            epochManager.setBottomDimension(startEpoch, dim);
        }
    }

    std::set<Epoch> seenEpochs({startEpoch});
    std::vector<Epoch> dfsStack({startEpoch});

    reachableEpochClasses.insert(epochManager.getEpochClass(startEpoch));

    // Perform a DFS to find all the reachable epochs
    while (!dfsStack.empty()) {
        Epoch currentEpoch = dfsStack.back();
        dfsStack.pop_back();
        for (auto const& step : possibleSteps) {
            Epoch successorEpoch = epochManager.getSuccessorEpoch(currentEpoch, step);
            if (seenEpochs.insert(successorEpoch).second) {
                reachableEpochClasses.insert(epochManager.getEpochClass(successorEpoch));
                dfsStack.push_back(std::move(successorEpoch));
            }
        }
    }

    // Also treat dimensions without a priori bound. Unbounded dimensions need no further treatment as for these only the 'bottom' class is relevant.
    for (uint64_t dim = 0; dim < epochManager.getDimensionCount(); ++dim) {
        if (dimensions[dim].boundType != DimensionBoundType::Unbounded && !dimensions[dim].maxValue) {
            std::vector<EpochClass> newClasses;
            for (auto const& c : reachableEpochClasses) {
                auto newClass = c;
                epochManager.setDimensionOfEpochClass(newClass, dim, false);
                newClasses.push_back(newClass);
            }
            for (auto const& c : newClasses) {
                reachableEpochClasses.insert(c);
            }
        }
    }
}

template<typename ValueType>
void ProductModel<ValueType>::computeReachableStates(EpochClass const& epochClass, std::vector<EpochClass> const& predecessors) {
    storm::storage::BitVector bottomDimensions(epochManager.getDimensionCount(), false);
    bool considerInitialStates = true;
    for (uint64_t dim = 0; dim < epochManager.getDimensionCount(); ++dim) {
        if (epochManager.isBottomDimensionEpochClass(epochClass, dim)) {
            bottomDimensions.set(dim, true);
            if (dimensions[dim].boundType != DimensionBoundType::Unbounded && dimensions[dim].maxValue) {
                considerInitialStates = false;
            }
        }
    }
    storm::storage::BitVector nonBottomDimensions = ~bottomDimensions;

    storm::storage::BitVector ecInStates(getProduct().getNumberOfStates(), false);
    if (considerInitialStates) {
        for (auto initState : getProduct().getInitialStates()) {
            uint64_t transformedInitState = transformProductState(initState, epochClass, memoryStateManager.getInitialMemoryState());
            ecInStates.set(transformedInitState, true);
        }
    }
    for (auto const& predecessor : predecessors) {
        storm::storage::BitVector positiveStepDimensions(epochManager.getDimensionCount(), false);
        for (uint64_t dim = 0; dim < epochManager.getDimensionCount(); ++dim) {
            if (!epochManager.isBottomDimensionEpochClass(predecessor, dim) && bottomDimensions.get(dim)) {
                positiveStepDimensions.set(dim, true);
            }
        }
        STORM_LOG_ASSERT(reachableStates.find(predecessor) != reachableStates.end(), "Could not find reachable states of predecessor epoch class.");
        storm::storage::BitVector predecessorStates = reachableStates.find(predecessor)->second;
        for (auto predecessorState : predecessorStates) {
            uint64_t predecessorMemoryState = getMemoryState(predecessorState);
            for (uint64_t choice = getProduct().getTransitionMatrix().getRowGroupIndices()[predecessorState];
                 choice < getProduct().getTransitionMatrix().getRowGroupIndices()[predecessorState + 1]; ++choice) {
                bool choiceLeadsToThisClass = false;
                Epoch const& choiceStep = getSteps()[choice];
                for (auto dim : positiveStepDimensions) {
                    if (epochManager.getDimensionOfEpoch(choiceStep, dim) > 0) {
                        choiceLeadsToThisClass = true;
                    }
                }

                if (choiceLeadsToThisClass) {
                    for (auto const& transition : getProduct().getTransitionMatrix().getRow(choice)) {
                        uint64_t successorState = transformProductState(transition.getColumn(), epochClass, predecessorMemoryState);

                        ecInStates.set(successorState, true);
                    }
                }
            }
        }
    }

    // Find all states reachable from an InState via DFS.
    storm::storage::BitVector ecReachableStates = ecInStates;
    std::vector<uint64_t> dfsStack(ecReachableStates.begin(), ecReachableStates.end());

    while (!dfsStack.empty()) {
        uint64_t currentState = dfsStack.back();
        uint64_t currentMemoryState = getMemoryState(currentState);
        dfsStack.pop_back();

        for (uint64_t choice = getProduct().getTransitionMatrix().getRowGroupIndices()[currentState];
             choice != getProduct().getTransitionMatrix().getRowGroupIndices()[currentState + 1]; ++choice) {
            bool choiceLeadsOutsideOfEpoch = false;
            Epoch const& choiceStep = getSteps()[choice];
            for (auto dim : nonBottomDimensions) {
                if (epochManager.getDimensionOfEpoch(choiceStep, dim) > 0) {
                    choiceLeadsOutsideOfEpoch = true;
                    break;
                }
            }

            for (auto const& transition : getProduct().getTransitionMatrix().getRow(choice)) {
                uint64_t successorState = transformProductState(transition.getColumn(), epochClass, currentMemoryState);
                if (choiceLeadsOutsideOfEpoch) {
                    ecInStates.set(successorState, true);
                }
                if (!ecReachableStates.get(successorState)) {
                    ecReachableStates.set(successorState, true);
                    dfsStack.push_back(successorState);
                }
            }
        }
    }

    reachableStates[epochClass] = std::move(ecReachableStates);

    inStates[epochClass] = std::move(ecInStates);
}

template<typename ValueType>
typename ProductModel<ValueType>::MemoryState ProductModel<ValueType>::transformMemoryState(MemoryState const& memoryState, EpochClass const& epochClass,
                                                                                            MemoryState const& predecessorMemoryState) const {
    MemoryState memoryStatePrime = memoryState;

    for (auto const& objDimensions : objectiveDimensions) {
        for (auto dim : objDimensions) {
            auto const& dimension = dimensions[dim];
            if (dimension.memoryLabel) {
                bool dimUpperBounded = dimension.boundType == DimensionBoundType::UpperBound;
                bool dimBottom = epochManager.isBottomDimensionEpochClass(epochClass, dim);
                if (dimUpperBounded && dimBottom && memoryStateManager.isRelevantDimension(predecessorMemoryState, dim)) {
                    STORM_LOG_ASSERT(objDimensions == dimension.dependentDimensions, "Unexpected set of dependent dimensions.");
                    memoryStateManager.setRelevantDimensions(memoryStatePrime, objDimensions, false);
                    break;
                } else if (!dimUpperBounded && !dimBottom && memoryStateManager.isRelevantDimension(predecessorMemoryState, dim)) {
                    memoryStateManager.setRelevantDimensions(memoryStatePrime, dimension.dependentDimensions, true);
                }
            }
        }
    }

    //  std::cout << "Transformed memory state " << memoryStateManager.toString(memoryState) << " at epoch class " << epochClass << " with predecessor " <<
    //  memoryStateManager.toString(predecessorMemoryState) << " to " << memoryStateManager.toString(memoryStatePrime) << '\n';

    return memoryStatePrime;
}

template<typename ValueType>
uint64_t ProductModel<ValueType>::transformProductState(uint64_t const& productState, EpochClass const& epochClass,
                                                        MemoryState const& predecessorMemoryState) const {
    return getProductState(getModelState(productState), transformMemoryState(getMemoryState(productState), epochClass, predecessorMemoryState));
}

template<typename ValueType>
boost::optional<storm::storage::BitVector> const& ProductModel<ValueType>::getProb1InitialStates(uint64_t objectiveIndex) const {
    return prob1InitialStates[objectiveIndex];
}

template class ProductModel<double>;
template class ProductModel<storm::RationalNumber>;
}  // namespace rewardbounded
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm
