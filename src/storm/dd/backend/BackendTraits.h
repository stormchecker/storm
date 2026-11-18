#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>

#include "storm-config.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/dd/DdType.h"

namespace storm {
namespace dd {

/*!
 * Static description of a DD backend: which optional capabilities it offers and for which value types its
 * Internal{Bdd,Add,DdManager} specializations are actually functional.
 *
 * A new backend adds one specialization here and implements the contract in BackendConcepts.h.
 *
 * "Supports a value type" means that the backend's InternalAdd<..., ValueType> works, not merely that it is
 * explicitly instantiated (CUDD, for example, is instantiated for RationalNumber, but most operations throw).
 */
/// A list of types, used to enumerate the value types of a backend.
template<typename... Types>
struct TypeList {
    template<typename T>
    static constexpr bool contains = (std::is_same_v<T, Types> || ...);
};

template<DdType LibraryType>
struct BackendTraits;

/// CUDD
template<>
struct BackendTraits<DdType::CUDD> {
    static constexpr std::string_view name = "cudd";

    /// The library was found/enabled at build time. If false, the backend's classes still exist but throw MissingLibraryException.
#ifdef STORM_HAVE_CUDD
    static constexpr bool available = true;
#else
    static constexpr bool available = false;
#endif

    /// DD variables can be inserted at a given level
    static constexpr bool orderedInsertion = true;
    /// Dynamic variable reordering is available
    static constexpr bool dynamicReordering = true;
    /// InternalDdManager::getAddUndefined is available
    static constexpr bool hasUndefined = false;
    /// All DD operations must be executed through InternalDdManager::execute (e.g. inside a thread-pool task)
    static constexpr bool needsExecuteWrapper = false;

    using ValueTypes = TypeList<double, uint_fast64_t>;
    template<typename ValueType>
    static constexpr bool supportsValueType = ValueTypes::template contains<ValueType>;
};

/// Sylvan
template<>
struct BackendTraits<DdType::Sylvan> {
    static constexpr std::string_view name = "sylvan";

#ifdef STORM_HAVE_SYLVAN
    static constexpr bool available = true;
#else
    static constexpr bool available = false;
#endif

    static constexpr bool orderedInsertion = false;
    static constexpr bool dynamicReordering = false;
    static constexpr bool hasUndefined = true;
    static constexpr bool needsExecuteWrapper = true;

    using ValueTypes = TypeList<double, uint_fast64_t, storm::RationalNumber, storm::RationalFunction>;
    template<typename ValueType>
    static constexpr bool supportsValueType = ValueTypes::template contains<ValueType>;
};

}  // namespace dd
}  // namespace storm
