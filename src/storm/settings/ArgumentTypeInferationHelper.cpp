#include "ArgumentTypeInferationHelper.h"

namespace storm {
namespace settings {

template<typename T>
ArgumentType inferToEnumType() {
    STORM_LOG_THROW(false, storm::exceptions::InternalTypeErrorException, "Unable to infer type of argument.");
}

template<>
ArgumentType inferToEnumType<std::string>() {
    return ArgumentType::String;
}

template<>
ArgumentType inferToEnumType<int_fast64_t>() {
    return ArgumentType::Integer;
}

template<>
ArgumentType inferToEnumType<uint_fast64_t>() {
    return ArgumentType::UnsignedInteger;
}

template<>
ArgumentType inferToEnumType<double>() {
    return ArgumentType::Double;
}

template<>
ArgumentType inferToEnumType<bool>() {
    return ArgumentType::Boolean;
}

template<typename T>
std::string const& inferToString(ArgumentType const&, T const&) {
    STORM_LOG_THROW(false, storm::exceptions::InternalTypeErrorException, "Unable to infer string from non-string argument value.");
}

template<>
std::string const& inferToString<std::string>(ArgumentType const& argumentType, std::string const& value) {
    STORM_LOG_THROW(argumentType == ArgumentType::String, storm::exceptions::InternalTypeErrorException, "Unable to infer string from non-string argument.");
    return value;
}

template<typename T>
int_fast64_t inferToInteger(ArgumentType const&, T const&) {
    STORM_LOG_THROW(false, storm::exceptions::InternalTypeErrorException, "Unable to infer integer from non-integer argument value.");
}

template<>
int_fast64_t inferToInteger<int_fast64_t>(ArgumentType const& argumentType, int_fast64_t const& value) {
    STORM_LOG_THROW(argumentType == ArgumentType::Integer, storm::exceptions::InternalTypeErrorException, "Unable to infer integer from non-integer argument.");
    return value;
}

template<typename T>
uint_fast64_t inferToUnsignedInteger(ArgumentType const&, T const&) {
    STORM_LOG_THROW(false, storm::exceptions::InternalTypeErrorException, "Unable to infer unsigned integer from non-unsigned argument value.");
}

template<>
uint_fast64_t inferToUnsignedInteger<uint_fast64_t>(ArgumentType const& argumentType, uint_fast64_t const& value) {
    STORM_LOG_THROW(argumentType == ArgumentType::UnsignedInteger, storm::exceptions::InternalTypeErrorException,
                    "Unable to infer integer from non-integer argument.");
    return value;
}

template<typename T>
double inferToDouble(ArgumentType const&, T const&) {
    STORM_LOG_THROW(false, storm::exceptions::InternalTypeErrorException, "Unable to infer double from non-double argument value.");
}

template<>
double inferToDouble<double>(ArgumentType const& argumentType, double const& value) {
    STORM_LOG_THROW(argumentType == ArgumentType::Double, storm::exceptions::InternalTypeErrorException, "Unable to infer double from non-double argument.");
    return value;
}

template<typename T>
bool inferToBoolean(ArgumentType const&, T const&) {
    STORM_LOG_THROW(false, storm::exceptions::InternalTypeErrorException, "Unable to infer boolean from non-boolean argument value.");
}

template<>
bool inferToBoolean<bool>(ArgumentType const& argumentType, bool const& value) {
    STORM_LOG_THROW(argumentType == ArgumentType::Boolean, storm::exceptions::InternalTypeErrorException, "Unable to infer boolean from non-boolean argument.");
    return value;
}

// Explicitly instantiate the templates.
template std::string const& inferToString<int_fast64_t>(ArgumentType const& argumentType, int_fast64_t const& value);
template std::string const& inferToString<uint_fast64_t>(ArgumentType const& argumentType, uint_fast64_t const& value);
template std::string const& inferToString<double>(ArgumentType const& argumentType, double const& value);
template std::string const& inferToString<bool>(ArgumentType const& argumentType, bool const& value);

template int_fast64_t inferToInteger<std::string>(ArgumentType const& argumentType, std::string const& value);
template int_fast64_t inferToInteger<uint_fast64_t>(ArgumentType const& argumentType, uint_fast64_t const& value);
template int_fast64_t inferToInteger<double>(ArgumentType const& argumentType, double const& value);
template int_fast64_t inferToInteger<bool>(ArgumentType const& argumentType, bool const& value);

template uint_fast64_t inferToUnsignedInteger<std::string>(ArgumentType const& argumentType, std::string const& value);
template uint_fast64_t inferToUnsignedInteger<int_fast64_t>(ArgumentType const& argumentType, int_fast64_t const& value);
template uint_fast64_t inferToUnsignedInteger<double>(ArgumentType const& argumentType, double const& value);
template uint_fast64_t inferToUnsignedInteger<bool>(ArgumentType const& argumentType, bool const& value);

template double inferToDouble<std::string>(ArgumentType const& argumentType, std::string const& value);
template double inferToDouble<int_fast64_t>(ArgumentType const& argumentType, int_fast64_t const& value);
template double inferToDouble<uint_fast64_t>(ArgumentType const& argumentType, uint_fast64_t const& value);
template double inferToDouble<bool>(ArgumentType const& argumentType, bool const& value);

template bool inferToBoolean<std::string>(ArgumentType const& argumentType, std::string const& value);
template bool inferToBoolean<int_fast64_t>(ArgumentType const& argumentType, int_fast64_t const& value);
template bool inferToBoolean<uint_fast64_t>(ArgumentType const& argumentType, uint_fast64_t const& value);
template bool inferToBoolean<double>(ArgumentType const& argumentType, double const& value);
}  // namespace settings
}  // namespace storm
