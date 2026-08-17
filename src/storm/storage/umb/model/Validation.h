#pragma once

#include <iosfwd>

#include "storm/storage/umb/model/Type.h"
#include "storm/storage/umb/model/UmbModelForward.h"

namespace storm::umb {
/*!
 * Validates the given UMB model and writes potential errors to the given output stream.
 * @return true if the UMB model is valid.
 * @note This only validates the umbModel against the UMB specification. It does not check whether Storm supports all features used in the model.
 * @note The validation checks are incomplete. In particular, this does not apply any checks that would require iterating over dynamic-sized structures.
 */
bool validate(storm::umb::UmbModel const& umbModel, std::ostream& errors);

/*!
 * Validates the given UMB model. If it is invalid, an exception is thrown.
 * @note: This only validates the umbModel against the UMB specification. It does not check whether Storm supports all features used in the model.
 * @note The validation checks are incomplete. In particular, this does not apply any checks that would require iterating over dynamic-sized structures.
 */
void validateOrThrow(storm::umb::UmbModel const& umbModel);

namespace validation {
/*!
 * Validates a single type declaration against the UMB specification, writing potential errors to the given
 * output stream.
 * @param type The type declaration to validate.
 * @param requireStandardSize For types that are only occasionally required to be their default size (e.g.
 *        Bool, Int, Uint), enforces the default size when set. Types that must *always* be their default
 *        size (Double, DoubleInterval, String) are checked regardless of this flag.
 * @return true if the type declaration is valid.
 */
bool validateTypeDeclaration(storm::umb::SizedType const& type, bool requireStandardSize, std::ostream& err);
}  // namespace validation

}  // namespace storm::umb
