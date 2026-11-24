#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/JsonSerializationAdapter.h"
namespace storm::umb {

enum class Type { Bool, Int, IntInterval, Uint, UintInterval, Double, DoubleInterval, Rational, RationalInterval, String };
struct TypeDeclaration {
    using Values = Type;
    auto static constexpr Keys = {"bool",  "int", "int-interval", "uint", "uint-interval", "double", "double-interval", "rational", "rational-interval",
                                  "String"};
};

/*!
 * @return true if the type is Boolean.
 */
bool isBooleanType(Type const type);

/*!
 * @return true if the type is discrete (uint, int, or interval of (u)ints).
 */
bool isDiscreteNumericType(Type const type);

/*!
 * @return true if the type is continuous (double, rational, or interval of doubles/rationals)
 */
bool isContinuousNumericType(Type const type);

/*!
 * @return true if the type is numeric (discrete or continuous).
 */
bool isNumericType(Type const type);

/*!
 * @return true if the type is an interval type.
 */
bool isIntervalType(Type const type);

/*!
 * @return true if the type is an interval type.
 */
bool isStringType(Type const type);

/*!
 * Returns the default size (in bits) of a type, if available
 */
uint64_t defaultBitSize(Type const type);

/*!
 * Returns a string representation of the type.
 */
std::string toString(Type const type);

struct SizedType {
    storm::SerializedEnum<storm::umb::TypeDeclaration> type;
    std::optional<uint64_t> size;
    auto static constexpr JsonKeys = {"type", "size"};
    using JsonSerialization = storm::JsonSerialization;

    uint64_t bitSize() const;
    std::string toString() const;
};

}  // namespace storm::umb