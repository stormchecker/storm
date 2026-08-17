#pragma once

#include "storm/models/sparse/Model.h"
#include "storm/storage/StateActionPair.h"

namespace storm {

// Forward declare Scheduler class.
namespace storage {
template<typename ValueType>
class Scheduler;
}

namespace models {
namespace sparse {

/*!
 * The base class of sparse nondeterministic models.
 */
template<class ValueType, typename RewardModelType = StandardRewardModel<ValueType>>
class NondeterministicModel : public Model<ValueType, RewardModelType> {
   public:
    /*!
     * Constructs a model from the given data.
     *
     * @param modelType the type of this model
     * @param components The components for this model.
     */
    NondeterministicModel(ModelType modelType, storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components);
    NondeterministicModel(ModelType modelType, storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components);

    virtual ~NondeterministicModel() = default;

    /*!
     * Retrieves the vector indicating which matrix rows represent non-deterministic choices of a certain state.
     *
     * @return The vector indicating which matrix rows represent non-deterministic choices of a certain state.
     */
    std::vector<uint_fast64_t> const& getNondeterministicChoiceIndices() const;

    using Model<ValueType, RewardModelType>::getNumberOfChoices;

    /*!
     * @param state State for which we want to know how many choices it has
     *
     * @return The number of non-deterministic choices for the given state
     */
    uint_fast64_t getNumberOfChoices(uint_fast64_t state) const;

    virtual void reduceToStateBasedRewards() override;

    /*!
     *  For a state/action pair, get the choice index referring to the state-action pair.
     */
    uint_fast64_t getChoiceIndex(storm::storage::StateActionPair const& stateactPair) const;
    /*!
     * Applies the given scheduler to this model.
     * @param scheduler the considered scheduler.
     * @param dropUnreachableStates if set, the resulting model only considers the states that are reachable from an initial state
     * @param preserveModelType if set, the resulting model has the same type as the original (this) model,
     *                          even when all nondeterminism is resolved and independent of the scheduler. Otherwise, the model type may differ.
     */
    std::shared_ptr<storm::models::sparse::Model<ValueType, RewardModelType>> applyScheduler(storm::storage::Scheduler<ValueType> const& scheduler,
                                                                                             bool dropUnreachableStates = true,
                                                                                             bool preserveModelType = false) const;

    virtual void printModelInformationToStream(std::ostream& out) const override;

    virtual void writeDotToStream(std::ostream& outStream, size_t maxWidthLabel = 30, bool includeLabeling = true,
                                  storm::storage::BitVector const* subsystem = nullptr, std::vector<ValueType> const* firstValue = nullptr,
                                  std::vector<ValueType> const* secondValue = nullptr, std::vector<uint_fast64_t> const* stateColoring = nullptr,
                                  std::vector<std::string> const* colors = nullptr, std::vector<uint_fast64_t>* scheduler = nullptr,
                                  bool finalizeOutput = true) const override;
};

}  // namespace sparse
}  // namespace models
}  // namespace storm
