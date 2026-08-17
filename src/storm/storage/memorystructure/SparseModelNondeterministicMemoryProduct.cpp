#include "storm/storage/memorystructure/SparseModelNondeterministicMemoryProduct.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/sparse/ModelComponents.h"
#include "storm/utility/graph.h"

namespace storm {
namespace storage {

template<typename SparseModelType>
SparseModelNondeterministicMemoryProduct<SparseModelType>::SparseModelNondeterministicMemoryProduct(
    SparseModelType const& model, storm::storage::NondeterministicMemoryStructure const& memory)
    : model(model), memory(memory) {
    // intentionally left empty
}

template<typename SparseModelType>
std::shared_ptr<SparseModelType> SparseModelNondeterministicMemoryProduct<SparseModelType>::build() const {
    // For Markov automata, we can not introduce nondeterminism at Markovian states.
    // We could introduce new intermediate probabilistic states, however that is not supported yet.
    STORM_LOG_THROW(!model.isOfType(storm::models::ModelType::MarkovAutomaton), storm::exceptions::NotSupportedException,
                    "Nondeterministic memory product not supported for Markov automata as this would introduce nondeterminism at Markovian states.");
    // For simplicity we first build the 'full' product of model and memory (with model.numStates * memory.numStates states).
    storm::storage::sparse::ModelComponents<ValueType> components;
    components.transitionMatrix = buildTransitions();
    components.stateLabeling = buildStateLabeling();

    // Now delete unreachable states.
    storm::storage::BitVector allStates(components.transitionMatrix.getRowGroupCount(), true);
    auto reachableStates =
        storm::utility::graph::getReachableStates(components.transitionMatrix, components.stateLabeling.getStates("init"), allStates, ~allStates);
    components.transitionMatrix = components.transitionMatrix.getSubmatrix(true, reachableStates, reachableStates);
    components.stateLabeling = components.stateLabeling.getSubLabeling(reachableStates);

    // build the remaining components
    for (auto const& rewModel : model.getRewardModels()) {
        components.rewardModels.emplace(rewModel.first, buildRewardModel(rewModel.second, reachableStates));
    }
    /* Markov automata are not supported (This code would introduce nondeterminism at Markovian states...)
    if (model.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        STORM_LOG_ASSERT((std::is_same<SparseModelType, storm::models::sparse::MarkovAutomaton<ValueType>>::value), "Model has unexpected type.");
        auto ma = model.template as<storm::models::sparse::MarkovAutomaton<ValueType>>();
        components.exitRates = buildExitRateVector(*ma, reachableStates);
        components.markovianStates = buildMarkovianStateLabeling(*ma, reachableStates);
    } */
    return std::make_shared<SparseModelType>(std::move(components));
}

template<typename SparseModelType>
storm::storage::SparseMatrix<typename SparseModelNondeterministicMemoryProduct<SparseModelType>::ValueType>
SparseModelNondeterministicMemoryProduct<SparseModelType>::buildTransitions() const {
    storm::storage::SparseMatrix<ValueType> const& origTransitions = model.getTransitionMatrix();
    uint64_t numRows = 0;
    uint64_t numEntries = 0;
    for (uint64_t modelState = 0; modelState < model.getNumberOfStates(); ++modelState) {
        for (uint64_t memState = 0; memState < memory.getNumberOfStates(); ++memState) {
            numRows += origTransitions.getRowGroupSize(modelState) * memory.getNumberOfOutgoingTransitions(memState);
            numEntries += origTransitions.getRowGroup(modelState).getNumberOfEntries() * memory.getNumberOfOutgoingTransitions(memState);
        }
    }
    storm::storage::SparseMatrixBuilder<ValueType> builder(numRows, model.getNumberOfStates() * memory.getNumberOfStates(), numEntries, true, true,
                                                           model.getNumberOfStates() * memory.getNumberOfStates());

    uint64_t row = 0;
    for (uint64_t modelState = 0; modelState < model.getNumberOfStates(); ++modelState) {
        for (uint64_t memState = 0; memState < memory.getNumberOfStates(); ++memState) {
            builder.newRowGroup(row);
            for (uint64_t origRow = origTransitions.getRowGroupIndices()[modelState]; origRow < origTransitions.getRowGroupIndices()[modelState + 1];
                 ++origRow) {
                for (auto memStatePrime : memory.getTransitions(memState)) {
                    for (auto const& entry : origTransitions.getRow(origRow)) {
                        builder.addNextValue(row, getProductState(entry.getColumn(), memStatePrime), entry.getValue());
                    }
                    ++row;
                }
            }
        }
    }
    return builder.build();
}

template<typename SparseModelType>
storm::models::sparse::StateLabeling SparseModelNondeterministicMemoryProduct<SparseModelType>::buildStateLabeling() const {
    storm::models::sparse::StateLabeling labeling(model.getNumberOfStates() * memory.getNumberOfStates());
    for (auto const& labelName : model.getStateLabeling().getLabels()) {
        storm::storage::BitVector newStates(model.getNumberOfStates() * memory.getNumberOfStates(), false);

        // The init label is only assigned to Product states with the initial memory state
        if (labelName == "init") {
            for (auto modelState : model.getStateLabeling().getStates(labelName)) {
                newStates.set(getProductState(modelState, memory.getInitialState()));
            }
        } else {
            for (auto modelState : model.getStateLabeling().getStates(labelName)) {
                for (uint64_t memState = 0; memState < memory.getNumberOfStates(); ++memState) {
                    newStates.set(getProductState(modelState, memState));
                }
            }
        }
        labeling.addLabel(labelName, std::move(newStates));
    }
    return labeling;
}

template<typename SparseModelType>
storm::models::sparse::StandardRewardModel<typename SparseModelNondeterministicMemoryProduct<SparseModelType>::ValueType>
SparseModelNondeterministicMemoryProduct<SparseModelType>::buildRewardModel(storm::models::sparse::StandardRewardModel<ValueType> const& rewardModel,
                                                                            storm::storage::BitVector const& reachableStates) const {
    std::optional<std::vector<ValueType>> stateRewards, actionRewards;
    if (rewardModel.hasStateRewards()) {
        stateRewards = std::vector<ValueType>();
        stateRewards->reserve(model.getNumberOfStates() * memory.getNumberOfStates());
        for (uint64_t modelState = 0; modelState < model.getNumberOfStates(); ++modelState) {
            for (uint64_t memState = 0; memState < memory.getNumberOfStates(); ++memState) {
                if (reachableStates.get(getProductState(modelState, memState))) {
                    stateRewards->push_back(rewardModel.getStateReward(modelState));
                }
            }
        }
    }
    if (rewardModel.hasStateActionRewards()) {
        actionRewards = std::vector<ValueType>();
        for (uint64_t modelState = 0; modelState < model.getNumberOfStates(); ++modelState) {
            for (uint64_t memState = 0; memState < memory.getNumberOfStates(); ++memState) {
                if (reachableStates.get(getProductState(modelState, memState))) {
                    for (uint64_t origRow = model.getTransitionMatrix().getRowGroupIndices()[modelState];
                         origRow < model.getTransitionMatrix().getRowGroupIndices()[modelState + 1]; ++origRow) {
                        ValueType const& actionReward = rewardModel.getStateActionReward(origRow);
                        actionRewards->insert(actionRewards->end(), memory.getNumberOfOutgoingTransitions(memState), actionReward);
                    }
                }
            }
        }
    }
    STORM_LOG_THROW(!rewardModel.hasTransitionRewards(), storm::exceptions::NotSupportedException, "Transition rewards are currently not supported.");
    return storm::models::sparse::StandardRewardModel<ValueType>(std::move(stateRewards), std::move(actionRewards));
}

/* Markov Automata currently not supported.
template<typename SparseModelType>
std::vector<typename SparseModelNondeterministicMemoryProduct<SparseModelType>::ValueType>
SparseModelNondeterministicMemoryProduct<SparseModelType>::buildExitRateVector(storm::models::sparse::MarkovAutomaton<ValueType> const& modelAsMA,
storm::storage::BitVector const& reachableStates) const { std::vector<ValueType> res; res.reserve(model.getNumberOfStates() * memory.getNumberOfStates()); for
(uint64_t modelState = 0; modelState < model.getNumberOfStates(); ++modelState) { for (uint64_t memState = 0; memState < memory.getNumberOfStates(); ++memState)
{ if (reachableStates.get(getProductState(modelState, memState))) { res.push_back(modelAsMA.getExitRate(modelState));
            }
        }
    }
    return res;
}

template<typename SparseModelType>
storm::storage::BitVector
SparseModelNondeterministicMemoryProduct<SparseModelType>::buildMarkovianStateLabeling(storm::models::sparse::MarkovAutomaton<ValueType> const& modelAsMA,
storm::storage::BitVector const& reachableStates) const { storm::storage::BitVector res(reachableStates.getNumberOfSetBits(), false); uint64_t
currReachableState = 0; for (uint64_t modelState = 0; modelState < model.getNumberOfStates(); ++modelState) { for (uint64_t memState = 0; memState <
memory.getNumberOfStates(); ++memState) { if (reachableStates.get(getProductState(modelState, memState))) { if (modelAsMA.isMarkovianState(modelState)) {
                    res.set(currReachableState, true);
                }
                ++currReachableState;
            }
        }
    }
    STORM_LOG_ASSERT(currReachableState == reachableStates.getNumberOfSetBits(), "Reachable state count mismatch.");
    return res;
}*/

template<typename SparseModelType>
uint64_t SparseModelNondeterministicMemoryProduct<SparseModelType>::getProductState(uint64_t modelState, uint64_t memoryState) const {
    return modelState * memory.getNumberOfStates() + memoryState;
}

template<typename SparseModelType>
uint64_t SparseModelNondeterministicMemoryProduct<SparseModelType>::getModelState(uint64_t productState) const {
    return productState / memory.getNumberOfStates();
}

template<typename SparseModelType>
uint64_t SparseModelNondeterministicMemoryProduct<SparseModelType>::getMemoryState(uint64_t productState) const {
    return productState % memory.getNumberOfStates();
}

template class SparseModelNondeterministicMemoryProduct<storm::models::sparse::Mdp<double>>;
template class SparseModelNondeterministicMemoryProduct<storm::models::sparse::Mdp<storm::RationalNumber>>;
template class SparseModelNondeterministicMemoryProduct<storm::models::sparse::MarkovAutomaton<double>>;
template class SparseModelNondeterministicMemoryProduct<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>;
}  // namespace storage
}  // namespace storm