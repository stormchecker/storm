// Compile-time verification that every registered DD backend satisfies the contract in BackendConcepts.h.
// This file contains no code but ensures that the contracts are checked and fail here.

#include "storm/dd/Add.h"
#include "storm/dd/Bdd.h"
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
    static_assert((InternalDdManagerBackend<ManagerType, BddType, InternalAdd<T, ValueTypes>, EnvironmentType, ValueTypes> && ...),
                  "InternalDdManager does not satisfy the backend contract.");
    static_assert((InternalBddBackend<BddType, InternalAdd<T, ValueTypes>, ManagerType, ValueTypes> && ...),
                  "InternalBdd does not satisfy the backend contract.");
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

}  // namespace backend_check
}  // namespace dd
}  // namespace storm
