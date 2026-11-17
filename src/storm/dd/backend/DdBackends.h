#pragma once

#include <cstddef>
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
    static constexpr DdType values[] = {Types...};
    /// The first registered backend (the fallback if nothing else is selected).
    static constexpr DdType first = values[0];

    template<typename F>
    static constexpr void forEach(F&& f) {
        (f(std::integral_constant<DdType, Types>{}), ...);
    }
};

/// Register Dd types
using RegisteredDdTypes = DdTypeList<DdType::CUDD, DdType::Sylvan>;

namespace detail {

template<typename ValueType, DdType First, DdType... Rest>
constexpr DdType firstDdTypeSupportingImpl() {
    if constexpr (BackendTraits<First>::template supportsValueType<ValueType>) {
        return First;
    } else {
        static_assert(sizeof...(Rest) > 0, "No registered DD backend supports the given value type.");
        if constexpr (sizeof...(Rest) > 0) {
            return firstDdTypeSupportingImpl<ValueType, Rest...>();
        } else {
            return First;
        }
    }
}

template<typename ValueType, DdType... Types>
constexpr DdType firstDdTypeSupporting(DdTypeList<Types...>) {
    return firstDdTypeSupportingImpl<ValueType, Types...>();
}

template<typename F, DdType First, DdType... Rest>
auto dispatchDdType(DdType type, F&& f, DdTypeList<First, Rest...>) {
    if (type == First) {
        return f(std::integral_constant<DdType, First>{});
    }
    if constexpr (sizeof...(Rest) > 0) {
        return dispatchDdType(type, std::forward<F>(f), DdTypeList<Rest...>{});
    } else {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected DD type.");
        __builtin_unreachable();
    }
}

}  // namespace detail

/*!
 * The first registered backend that supports the given value type.
 * Useful to name the result type of a callback that is dispatched over backends (all alternatives must return the same type).
 */
template<typename ValueType>
inline constexpr DdType firstDdTypeSupporting = detail::firstDdTypeSupporting<ValueType>(RegisteredDdTypes{});

/*!
 * Translates the runtime value `type` to the compile-time constant.
 * All alternatives of f must return the same type.
 * Replaces hand-written `switch (ddType) { case CUDD: ...; case Sylvan: ...; }` dispatch code.
 */
template<typename F>
auto dispatchDdType(DdType type, F&& f) {
    return detail::dispatchDdType(type, std::forward<F>(f), RegisteredDdTypes{});
}

}  // namespace dd
}  // namespace storm
