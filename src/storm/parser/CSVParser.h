#pragma once

#include <string>
#include <vector>

namespace storm {
namespace parser {
/*!
 * Given a string separated by commas, returns the values.
 */
std::vector<std::string> parseCommaSeperatedValues(std::string const& input);

}  // namespace parser
}  // namespace storm
