#pragma once

#include <memory>

#include "storm/models/sparse/Model.h"

namespace storm::transformer {

template<typename InputValueType, typename OutputValueType>
/** Converts the numeric values of a sparse model while preserving its model-specific metadata. */
class SparseModelValueTypeTransformer {
   public:
    explicit SparseModelValueTypeTransformer() = default;

    /**
     * Returns a model equivalent to @p inputModel with all probabilities, rates, and rewards converted to OutputValueType.
     *
     * @pre inputModel is not null.
     */
    std::shared_ptr<storm::models::sparse::Model<OutputValueType>> transformModel(std::shared_ptr<models::sparse::Model<InputValueType>> const& inputModel);
};

}  // namespace storm::transformer
