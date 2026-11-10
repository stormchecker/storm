#pragma once

#include <string>
#include <vector>

#include "storm/adapters/JsonForward.h"
#include "storm/modelchecker/certificates/RankingType.h"

namespace storm::storage {
class BitVector;
}

namespace storm::modelchecker::detail {

/*!
 * Converts the given values to a json array.
 * Infinite values are written as the strings "inf" and "-inf".
 * Values of an exact type (i.e., rational numbers) are written as strings of the form "numerator/denominator" since json numbers cannot represent them.
 * @tparam ValueType the value type of the json object
 * @tparam ElementType Either ValueType or the type that extends ValueType with infinity
 */
template<typename ValueType, typename ElementType>
storm::json<ValueType> valuesToJson(std::vector<ElementType> const& values);

/*!
 * Converts the given ranks to a json array. Infinite ranks are written as the string "inf".
 */
template<typename ValueType>
storm::json<ValueType> ranksToJson(std::vector<RankingType> const& ranks);

/*!
 * Converts the given set of states to a json object of the form {"label": <label>, "states": [<state indices>]}.
 */
template<typename ValueType>
storm::json<ValueType> stateSetToJson(std::string const& label, storm::storage::BitVector const& states);

}  // namespace storm::modelchecker::detail
