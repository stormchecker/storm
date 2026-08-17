#pragma once

#include <cmath>  // isfinite
#include <limits>
#include <type_traits>

#include <nlohmann/detail/abi_macros.hpp>

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/constants.h"

NLOHMANN_JSON_NAMESPACE_BEGIN

namespace storm
{

template<typename T>
constexpr bool is_trivial = std::is_trivial_v<T>;

template<typename T>
constexpr bool supports_nan = std::numeric_limits<T>::is_iec559;

template<typename T>
constexpr bool is_exact = ::storm::NumberTraits<T>::IsExact;

template<typename T>
constexpr bool is_floating_point = std::is_floating_point_v<T> || std::is_same_v<T, ::storm::RationalNumber>;

template<typename T>
constexpr bool is_arithmetic = std::is_arithmetic_v<T> || std::is_same_v<T, ::storm::RationalNumber>;

template<typename T>
constexpr bool is_scalar = std::is_scalar_v<T> || std::is_same_v<T, ::storm::RationalNumber>;

template<typename T>
using storage_type = std::conditional_t<nlohmann::storm::is_trivial<T>, T, std::add_pointer_t<T>>;

template<typename T>
constexpr storage_type<T> to_storage(T& t)
{
    if constexpr (nlohmann::storm::is_trivial<T>)
    {
        return t;
    }
    else
    {
        return std::addressof(t);
    }
}

template<typename T>
constexpr std::remove_pointer_t<T> to_value(T t)
{
    if constexpr (std::is_pointer_v<T>)
    {
        return *t;
    }
    else
    {
        return t;
    }
}

template<typename T>
constexpr std::add_pointer_t<T> to_pointer(T t)
{
    if constexpr (std::is_pointer_v<T>)
    {
        return t;
    }
    else
    {
        return std::addressof(t);
    }
}

template<typename T>
T from_string(std::string const& str)
{
    return ::storm::utility::convertNumber<T>(str);
}

template<typename T>
constexpr T zero()
{
    return T(0);
}

template<typename T>
constexpr bool is_finite(T const& t)
{
    if constexpr (std::numeric_limits<T>::is_iec559)
    {
        return std::isfinite(t);
    }
    else
    {
        return true;
    }
}

template<typename ToType, typename FromType>
constexpr ToType convert(FromType const& v)
{
    if constexpr (is_trivial<FromType> && is_trivial<ToType>)
    {
        return static_cast<ToType>(v);
    }
	else if constexpr (std::is_same_v<ToType, int>)
	{
		// Conversion to int is e.g. necessary for scoped enums with default underlying type (enum class Name {...};)
		return static_cast<int>(::storm::utility::convertNumber<int64_t>(v));
	}
    else
    {
        return ::storm::utility::convertNumber<ToType>(v);
    }
}

// void float_to_stream()
}  // namespace storm

NLOHMANN_JSON_NAMESPACE_END