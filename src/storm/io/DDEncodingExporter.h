#pragma once

#include "storm/models/symbolic/Model.h"

namespace storm {
namespace io {

/*!
 * Exports a sparse model into the explicit drdd format.
 *
 * @param filename       File path
 * @param symbolicModel  Model to export
 */
template<storm::dd::DdType Type, typename ValueType>
void explicitExportSymbolicModel(std::string const& filename, std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> symbolicModel);

}  // namespace io
}  // namespace storm