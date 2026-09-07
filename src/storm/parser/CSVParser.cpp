#include "CSVParser.h"

#include <boost/algorithm/string.hpp>
#include <boost/any.hpp>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

std::vector<std::string> parseCommaSeperatedValues(std::string const& input) {
    std::vector<std::string> values;
    std::vector<std::string> definitions;
    boost::split(definitions, input, boost::is_any_of(","));
    for (auto& definition : definitions) {
        boost::trim(definition);
        values.push_back(definition);
    }
    return values;
}
}  // namespace parser
}  // namespace storm
