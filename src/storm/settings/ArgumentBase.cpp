#include "storm/settings/ArgumentBase.h"

#include <boost/algorithm/string.hpp>
#include <iomanip>
#include <sstream>

namespace storm {
namespace settings {
std::ostream& operator<<(std::ostream& out, ArgumentBase const& argument) {
    argument.printToStream(out);
    return out;
}

template<typename TargetType>
TargetType ArgumentBase::convertFromString(std::string const& valueAsString, bool& conversionSuccessful) {
    std::istringstream stream(valueAsString);
    TargetType t;
    conversionSuccessful = (stream >> t) && (stream >> std::ws).eof();
    return t;
}

template<>
std::string ArgumentBase::convertFromString(std::string const& valueAsString, bool& conversionSuccessful) {
    conversionSuccessful = true;
    return valueAsString;
}

template<>
bool ArgumentBase::convertFromString<bool>(std::string const& s, bool& ok) {
    static const std::string lowerTrueString = "true";
    static const std::string lowerFalseString = "false";
    static const std::string lowerYesString = "yes";
    static const std::string lowerNoString = "no";

    std::string lowerInput = boost::algorithm::to_lower_copy(s);

    if (lowerInput.compare(lowerTrueString) == 0 || lowerInput.compare(lowerYesString) == 0) {
        ok = true;
        return true;
    } else if (lowerInput.compare(lowerFalseString) == 0 || lowerInput.compare(lowerNoString) == 0) {
        ok = true;
        return false;
    }

    std::istringstream stream(s);
    bool t;
    ok = (stream >> t) && (stream >> std::ws).eof();
    return t;
}

template<typename ValueType>
std::string ArgumentBase::convertToString(ValueType const& value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

// Explicitly instantiate the templates.
template int_fast64_t ArgumentBase::convertFromString(std::string const& valueAsString, bool& conversionSuccessful);
template uint_fast64_t ArgumentBase::convertFromString(std::string const& valueAsString, bool& conversionSuccessful);
template double ArgumentBase::convertFromString(std::string const& valueAsString, bool& conversionSuccessful);

template std::string ArgumentBase::convertToString(std::string const& value);
template std::string ArgumentBase::convertToString(int_fast64_t const& value);
template std::string ArgumentBase::convertToString(uint_fast64_t const& value);
template std::string ArgumentBase::convertToString(double const& value);
template std::string ArgumentBase::convertToString(bool const& value);
}  // namespace settings
}  // namespace storm
