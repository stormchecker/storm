#pragma once

#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

#include "storm/dd/DdType.h"
#include "storm/dd/backend/BackendTraits.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

/*!
 * Registry of DD backends.
 */
template<DdType... Types>
struct DdTypeList {
    static constexpr std::size_t size = sizeof...(Types);
    static constexpr DdType values[sizeof...(Types)] = {Types...};
    /// The first registered backend (the fallback if nothing else is selected).
    static constexpr DdType first = values[0];

    template<typename F>
    static constexpr void forEach(F&& f) {
        (f(std::integral_constant<DdType, Types>{}), ...);
    }
};

/// Register Dd types
using RegisteredDdTypes = DdTypeList<DdType::CUDD, DdType::Sylvan>;

/*!
 * The name of the given backend.
 */
inline std::string_view ddLibraryName(DdType type) {
    std::string_view name = "unknown";
    RegisteredDdTypes::forEach([&](auto tag) {
        if (type == decltype(tag)::value) {
            name = BackendTraits<decltype(tag)::value>::name;
        }
    });
    return name;
}

namespace detail {

template<typename F, DdType First, DdType... Rest>
auto dispatchDdType(DdType type, F&& f, DdTypeList<First, Rest...>) {
    if (type == First) {
        return f(std::integral_constant<DdType, First>{});
    }
    if constexpr (sizeof...(Rest) > 0) {
        return dispatchDdType(type, std::forward<F>(f), DdTypeList<Rest...>{});
    } else {
        STORM_LOG_THROW_UNCONDITIONALLY(storm::exceptions::UnexpectedException, "Unexpected DD type.");
    }
}

template<typename ValueType, typename F, DdType First, DdType... Rest>
auto dispatchDdTypeSupporting(DdType type, F&& f, DdTypeList<First, Rest...>) {
    if constexpr (BackendTraits<First>::template supportsValueType<ValueType>) {
        if (type == First) {
            return f(std::integral_constant<DdType, First>{});
        }
    }
    if constexpr (sizeof...(Rest) > 0) {
        return dispatchDdTypeSupporting<ValueType>(type, std::forward<F>(f), DdTypeList<Rest...>{});
    } else {
        STORM_LOG_THROW_UNCONDITIONALLY(storm::exceptions::UnexpectedException,
                                        "Unexpected combination of DD library '" << ddLibraryName(type) << "' and value type.");
    }
}

}  // namespace detail

/*!
 * Translates the runtime value `type` to the compile-time constant.
 * All alternatives of f must return the same type.
 * Replaces handwritten `switch (ddType) { case CUDD: ...; case Sylvan: ...; }` dispatch code.
 */
template<typename F>
auto dispatchDdType(DdType type, F&& f) {
    return detail::dispatchDdType(type, std::forward<F>(f), RegisteredDdTypes{});
}

/*!
 * Like dispatchDdType, but only considers backends that support the ValueType.
 * Throws an UnexpectedException if the given backend does not support ValueType.
 */
template<typename ValueType, typename F>
auto dispatchDdTypeSupporting(DdType type, F&& f) {
    return detail::dispatchDdTypeSupporting<ValueType>(type, std::forward<F>(f), RegisteredDdTypes{});
}

}  // namespace dd
}  // namespace storm
