#pragma once

#include <cstdint>
#include <string>

#include "storm/exceptions/InternalTypeErrorException.h"
#include "storm/settings/ArgumentType.h"
#include "storm/utility/macros.h"

namespace storm {
namespace settings {
/*!
 * This function infers the type in our enum of possible types from the template parameter.
 *
 * @return The argument type that has been inferred.
 */
template<typename T>
ArgumentType inferToEnumType();

// Specialized function templates that allow casting the given value to the correct type. If the conversion
// fails, an exception is thrown.
template<typename T>
std::string const& inferToString(ArgumentType const& argumentType, T const& value);

template<>
std::string const& inferToString(ArgumentType const& argumentType, std::string const& value);

template<typename T>
int_fast64_t inferToInteger(ArgumentType const& argumentType, T const& value);

template<>
int_fast64_t inferToInteger(ArgumentType const& argumentType, int_fast64_t const& value);

template<typename T>
uint_fast64_t inferToUnsignedInteger(ArgumentType const& argumentType, T const& value);

template<>
uint_fast64_t inferToUnsignedInteger(ArgumentType const& argumentType, uint_fast64_t const& value);

template<typename T>
double inferToDouble(ArgumentType const& argumentType, T const& value);

template<>
double inferToDouble(ArgumentType const& argumentType, double const& value);

template<typename T>
bool inferToBoolean(ArgumentType const& argumentType, T const& value);

template<>
bool inferToBoolean(ArgumentType const& argumentType, bool const& value);
}  // namespace settings
}  // namespace storm
