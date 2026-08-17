#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/memorystructure/MemoryStructure.h"
#include "storm/storage/memorystructure/SparseModelMemoryProductReverseData.h"

namespace storm {
namespace storage {
/*!
 * This class builds the product of the given sparse model and the given memory structure.
 * This is similar to the well-known product of a model with a deterministic rabin automaton.
 * The product contains only the reachable states of the product
 *
 * The states of the resulting sparse model will have the original state labels plus the labels of this
 * memory structure.
 * An exception is thrown if the state labelings are not disjoint.
 */
template<typename ValueType, typename RewardModelType = storm::models::sparse::StandardRewardModel<ValueType>>
class SparseModelMemoryProduct {
   public:
    // Constructs the product w.r.t. the given model and the given memory structure
    SparseModelMemoryProduct(storm::models::sparse::Model<ValueType, RewardModelType> const& sparseModel,
                             storm::storage::MemoryStructure const& memoryStructure);

    // Constructs the product w.r.t. the given model and the given (finite memory) scheduler.
    SparseModelMemoryProduct(storm::models::sparse::Model<ValueType, RewardModelType> const& sparseModel,
                             storm::storage::Scheduler<ValueType> const& scheduler);

    // Enforces that the given model and memory state as well as the successor(s) are considered reachable -- even if they are not reachable from an initial
    // state.
    void addReachableState(uint64_t const& modelState, uint64_t const& memoryState);

    // Enforces that every state is considered reachable. If this is set, the result has size #modelStates * #memoryStates
    void setBuildFullProduct();

    // Returns true iff the given model and memory state is reachable in the product
    bool isStateReachable(uint64_t const& modelState, uint64_t const& memoryState);

    // Retrieves the state of the resulting model that represents the given memory and model state.
    // This method should only be called if the given state is reachable.
    uint64_t const& getResultState(uint64_t const& modelState, uint64_t const& memoryState);

    /*!
     * Invokes the building of the product under the specified scheduler (if given).
     * @param preserveModelType if set, the resulting model has the same type as the original model.
     * Otherwise, the model type might differ, e.g., MDPs are changed to DTMCs in case a complete scheduler is given.
     */
    std::shared_ptr<storm::models::sparse::Model<ValueType, RewardModelType>> build(bool preserveModelType = false);

    storm::models::sparse::Model<ValueType, RewardModelType> const& getOriginalModel() const;
    storm::storage::MemoryStructure const& getMemory() const;

    /*!
     * Extracts the reverse data that can be used to apply results for the product model back to the original input model.
     * @note This method should only be called after the product has been built.
     */
    SparseModelMemoryProductReverseData computeReverseData() const;

   private:
    // Initializes auxiliary data for building the product
    void initialize();

    // Computes for each pair of memory state and model transition index the successor memory state
    // The resulting vector maps (modelTransition * memoryStateCount) + memoryState to the corresponding successor state of the memory structure
    void computeMemorySuccessors();

    // Computes the reachable states of the resulting model
    void computeReachableStates(storm::storage::BitVector const& initialStates);

    // Methods that build the model components
    // Matrix for deterministic models
    storm::storage::SparseMatrix<ValueType> buildDeterministicTransitionMatrix();
    // Matrix for nondeterministic models
    storm::storage::SparseMatrix<ValueType> buildNondeterministicTransitionMatrix();
    // Matrix for models that consider a scheduler
    storm::storage::SparseMatrix<ValueType> buildTransitionMatrixForScheduler();
    // State labeling.
    storm::models::sparse::StateLabeling buildStateLabeling(storm::storage::SparseMatrix<ValueType> const& resultTransitionMatrix);
    // Reward models
    std::unordered_map<std::string, RewardModelType> buildRewardModels(storm::storage::SparseMatrix<ValueType> const& resultTransitionMatrix);

    // Builds the resulting model
    std::shared_ptr<storm::models::sparse::Model<ValueType, RewardModelType>> buildResult(storm::storage::SparseMatrix<ValueType>&& matrix,
                                                                                          storm::models::sparse::StateLabeling&& labeling,
                                                                                          std::unordered_map<std::string, RewardModelType>&& rewardModels,
                                                                                          bool preserveModelType);

    // Stores whether this builder has already been initialized.
    bool isInitialized;

    // stores the successor memory states for each transition in the product
    std::vector<uint64_t> memorySuccessors;

    // Maps (modelState * memoryStateCount) + memoryState to the state in the result that represents (memoryState,modelState)
    std::vector<uint64_t> toResultStateMapping;

    // Indicates which states are considered reachable. (s, m) is reachable if this BitVector is true at (s * memoryStateCount) + m
    storm::storage::BitVector reachableStates;

    uint64_t const memoryStateCount;

    storm::models::sparse::Model<ValueType, RewardModelType> const& model;
    boost::optional<storm::storage::MemoryStructure> localMemory;
    storm::storage::MemoryStructure const& memory;
    boost::optional<storm::storage::Scheduler<ValueType> const&> scheduler;
};
}  // namespace storage
}  // namespace storm
