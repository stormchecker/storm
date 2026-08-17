#pragma once
#include <filesystem>
#include <memory>

#include "storm/storage/umb/export/ExportOptions.h"
#include "storm/storage/umb/import/ImportOptions.h"

#include "storm/models/sparse/ModelForward.h"

namespace storm {

namespace models {
enum class ModelType;
class ModelBase;
}  // namespace models

/*!
 * Import and export of umb files.
 * @see https://pmc-tools.github.io/umb/spec
 * @see https://arxiv.org/abs/2606.17811
 */
namespace umb {

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> buildModelFromUmb(std::filesystem::path const& umbLocation, ImportOptions const& options = {});

std::shared_ptr<storm::models::ModelBase> buildModelFromUmb(std::filesystem::path const& umbLocation, ImportOptions const& options = {});

template<typename ValueType>
void exportModelToUmb(storm::models::sparse::Model<ValueType> const& model, std::filesystem::path const& targetLocation, ExportOptions const& options = {});

}  // namespace umb
}  // namespace storm