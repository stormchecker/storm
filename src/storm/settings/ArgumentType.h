#pragma once

#include <iostream>

namespace storm {
namespace settings {

/*!
 * This enum captures all possible types for arguments.
 */
enum class ArgumentType { String, Integer, UnsignedInteger, Double, Boolean };

std::ostream& operator<<(std::ostream& out, ArgumentType& argumentType);

}  // namespace settings
}  // namespace storm
