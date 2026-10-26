#pragma once

#include <memory>

#include "storm/io/umb/export/ExportOptions.h"
#include "storm/languages/umb/UmbModelForward.h"
#include "storm/models/sparse/ModelForward.h"

namespace storm::umb {
template<typename ValueType>
storm::umb::UmbModel sparseModelToUmb(storm::models::sparse::Model<ValueType> const& model, ExportOptions const& options);

}  // namespace storm::umb