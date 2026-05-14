#pragma once

#include <memory>
#include "storm/models/sparse/ModelForward.h"
#include "storm/storage/umb/import/ImportOptions.h"
#include "storm/storage/umb/model/UmbModelForward.h"

namespace storm {

namespace models {
enum class ModelType;
class ModelBase;
}  // namespace models

namespace umb {

/*!
 * Derives the model type from the given UMB model index.
 */
storm::models::ModelType deriveModelType(storm::umb::ModelIndex const& index);

/*!
 * Returns true iff the given umb model with the given options should have ValueType as its ValueType.
 * Currently, this can be either double, storm::RationalNumber, storm::Interval, or storm::RationalInterval.
 * @note this does not check whether the model can actually be represented with the derived ValueType (e.g., interval to double is not possible).
 */
template<typename ValueType>
bool deriveValueType(storm::umb::ModelIndex const& index, ImportOptions const& options = {});

/*!
 * Constructs a sparse model from the given UMB model.
 * The derived value type is enforced, which might trigger conversions e.g. from/to rationals.
 * Throws an exception if such a conversion is not possible (e.g. interval to ValueType=double).
 */
template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> sparseModelFromUmb(storm::umb::UmbModel const& umbModel, ImportOptions const& options = {});

/*!
 * Constructs a sparse model from the given UMB model.
 * @note deriveValueType can be used to determine the right ValueType for downward casting the returned ModelBase.
 */
std::shared_ptr<storm::models::ModelBase> sparseModelFromUmb(storm::umb::UmbModel const& umbModel, ImportOptions const& options = {});

}  // namespace umb
}  // namespace storm