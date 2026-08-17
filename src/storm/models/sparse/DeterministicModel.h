#pragma once

#include "storm/models/sparse/Model.h"

namespace storm {
namespace models {
namespace sparse {

/*!
 * The base class of all sparse deterministic models.
 */
template<class ValueType, typename RewardModelType = StandardRewardModel<ValueType>>
class DeterministicModel : public Model<ValueType, RewardModelType> {
   public:
    /*!
     * Constructs a model from the given data.
     *
     * @param modelType the type of this model
     * @param components The components for this model.
     */
    DeterministicModel(ModelType modelType, storm::storage::sparse::ModelComponents<ValueType, RewardModelType> const& components);
    DeterministicModel(ModelType modelType, storm::storage::sparse::ModelComponents<ValueType, RewardModelType>&& components);

    DeterministicModel(DeterministicModel<ValueType, RewardModelType> const& other) = default;
    DeterministicModel& operator=(DeterministicModel<ValueType, RewardModelType> const& other) = default;

    DeterministicModel(DeterministicModel<ValueType, RewardModelType>&& other) = default;
    DeterministicModel<ValueType, RewardModelType>& operator=(DeterministicModel<ValueType, RewardModelType>&& model) = default;

    virtual ~DeterministicModel() = default;

    virtual void writeDotToStream(std::ostream& outStream, size_t maxWidthLabel = 30, bool includeLabeling = true,
                                  storm::storage::BitVector const* subsystem = nullptr, std::vector<ValueType> const* firstValue = nullptr,
                                  std::vector<ValueType> const* secondValue = nullptr, std::vector<uint_fast64_t> const* stateColoring = nullptr,
                                  std::vector<std::string> const* colors = nullptr, std::vector<uint_fast64_t>* scheduler = nullptr,
                                  bool finalizeOutput = true) const override;
};

}  // namespace sparse
}  // namespace models
}  // namespace storm
