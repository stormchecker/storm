#include "KeyValueParser.h"

#include <boost/algorithm/string.hpp>
#include <vector>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

std::unordered_map<std::string, std::string> parseKeyValueString(std::string const& keyValueString) {
    std::unordered_map<std::string, std::string> keyValueMap;
    std::vector<std::string> definitions;
    boost::split(definitions, keyValueString, boost::is_any_of(","));
    for (auto& definition : definitions) {
        boost::trim(definition);

        // Check whether the token could be a legal constant definition.
        std::size_t positionOfAssignmentOperator = definition.find('=');
        STORM_LOG_THROW(positionOfAssignmentOperator != std::string::npos, storm::exceptions::InvalidArgumentException,
                        "Illegal key value string: syntax error.");

        // Now extract the variable name and the value from the string.
        std::string key = definition.substr(0, positionOfAssignmentOperator);
        boost::trim(key);
        std::string value = definition.substr(positionOfAssignmentOperator + 1);
        boost::trim(value);
        keyValueMap.emplace(key, value);
    }
    return keyValueMap;
}
}  // namespace parser
}  // namespace storm
