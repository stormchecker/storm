// Compile-time verification that every registered DD backend satisfies the contract in BackendConcepts.h.
// This file contains no code but ensures that the contracts are checked and fail here.

#include "storm/dd/Add.h"
#include "storm/dd/Bdd.h"
#include "storm/dd/DdInstantiations.h"
#include "storm/dd/DdManager.h"
#include "storm/dd/backend/BackendConcepts.h"
#include "storm/dd/backend/BackendTraits.h"
#include "storm/dd/backend/DdBackends.h"
#include "storm/environment/dd/DdEnvironment.h"

namespace storm {
namespace dd {
namespace backend_check {

template<DdType T, typename... ValueTypes>
struct CheckBackendImpl {
    using ManagerType = InternalDdManager<T>;
    using BddType = InternalBdd<T>;
    using EnvironmentType = typename DdEnvironmentSelector<T>::type;

    static_assert((BackendTraits<T>::template supportsValueType<ValueTypes> && ...), "Value type listed for a backend that does not support it.");
    static_assert(InternalDdManagerBackend<ManagerType, BddType, InternalAdd<T, double>, EnvironmentType>,
                  "InternalDdManager does not satisfy the backend contract.");
    static_assert(InternalBddBackend<BddType, InternalAdd<T, double>, ManagerType, double>, "InternalBdd does not satisfy the backend contract.");
    static_assert((InternalAddBackend<InternalAdd<T, ValueTypes>, BddType, ManagerType, ValueTypes, AddIterator<T, ValueTypes>, DdManager<T>> && ...),
                  "InternalAdd does not satisfy the backend contract.");
    static_assert((AddIteratorBackend<AddIterator<T, ValueTypes>> && ...), "AddIterator does not satisfy the backend contract.");
};

template<DdType T, typename ValueTypeList>
struct CheckBackendFor;

template<DdType T, typename... ValueTypes>
struct CheckBackendFor<T, TypeList<ValueTypes...>> : CheckBackendImpl<T, ValueTypes...> {};

template<typename Registered>
struct CheckAll;

template<DdType... Types>
struct CheckAll<DdTypeList<Types...>> : CheckBackendFor<Types, typename BackendTraits<Types>::ValueTypes>... {};

// Checks every backend in the registry (DdBackends.h) against the value types declared in its BackendTraits.
template struct CheckAll<RegisteredDdTypes>;

// The lists used for explicit instantiations (DdInstantiations.h) must only contain supported combinations and must cover every registered backend.
#define STORM_DD_CHECK_PAIR(DD, ValueType) \
    static_assert(BackendTraits<DdType::DD>::template supportsValueType<ValueType>, "DdInstantiations.h lists an unsupported backend/value type pair.");
STORM_DD_FOR_EACH_STANDARD(STORM_DD_CHECK_PAIR)
STORM_DD_FOR_EACH_NON_PARAMETRIC(STORM_DD_CHECK_PAIR)
#undef STORM_DD_CHECK_PAIR

#define STORM_DD_COUNT_BACKEND(DD) +1
static_assert(0 STORM_DD_FOR_EACH_BACKEND(STORM_DD_COUNT_BACKEND) == RegisteredDdTypes::size,
              "STORM_DD_FOR_EACH_BACKEND (DdInstantiations.h) and RegisteredDdTypes (DdBackends.h) list different backends.");
#undef STORM_DD_COUNT_BACKEND

}  // namespace backend_check
}  // namespace dd
}  // namespace storm
