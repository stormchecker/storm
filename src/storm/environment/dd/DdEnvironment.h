#pragma once

#include <cstdint>

#include "storm/environment/Environment.h"
#include "storm/environment/SubEnvironment.h"
#include "storm/storage/dd/DdType.h"

namespace storm {

class SylvanDdManagerEnvironment;
class CuddDdManagerEnvironment;

// Select the sub-environment that belongs to the given DD type.
template<storm::dd::DdType Type>
struct DdEnvironmentSelector {
    static_assert(Type == storm::dd::DdType::Sylvan || Type == storm::dd::DdType::CUDD, "Unhandled DD type.");
};
template<>
struct DdEnvironmentSelector<storm::dd::DdType::Sylvan> {
    using type = SylvanDdManagerEnvironment;
};
template<>
struct DdEnvironmentSelector<storm::dd::DdType::CUDD> {
    using type = CuddDdManagerEnvironment;
};

class DdEnvironment {
   public:
    DdEnvironment();
    ~DdEnvironment();

    SylvanDdManagerEnvironment& sylvan();
    SylvanDdManagerEnvironment const& sylvan() const;

    CuddDdManagerEnvironment& cudd();
    CuddDdManagerEnvironment const& cudd() const;

    /*!
     * Retrieves the sub-environment belonging to the given DD type.
     */
    template<storm::dd::DdType Type>
    typename DdEnvironmentSelector<Type>::type& get();

    template<storm::dd::DdType Type>
    typename DdEnvironmentSelector<Type>::type const& get() const;

   private:
    SubEnvironment<SylvanDdManagerEnvironment> sylvanEnvironment;
    SubEnvironment<CuddDdManagerEnvironment> cuddEnvironment;
};

}  // namespace storm
