#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "nlohmann/json_fwd.hpp"

namespace storm {

template<typename ValueType>
using json = nlohmann::basic_json<std::map, std::vector, std::string, bool, int64_t, uint64_t, ValueType>;

/*!
 * @pre j.is_number_float() must be true
 * @param j json object, must be of float type
 * @return true iff
 */
template<typename ValueType>
bool isJsonNumberExportAccurate(storm::json<ValueType> const& j);

/*!
 * Dumps the given json object, producing a String.
 * If the ValueType is exact, a warning is printed if one or more number values can not be exported (dumped) with full accuracy (e.g. there is no float for 1/3)
 * @param j The JSON object
 * @param compact indicates whether the export should be done in compact mode (no unnecessary whitespace)
 */
template<typename ValueType>
std::string dumpJson(storm::json<ValueType> const& j, bool compact = false);

}  // namespace storm