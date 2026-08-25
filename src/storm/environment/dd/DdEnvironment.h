#pragma once

#include <cstdint>

#include "storm/environment/Environment.h"
#include "storm/environment/SubEnvironment.h"
#include "storm/storage/dd/DdType.h"

namespace storm {

class CuddDdManagerEnvironment;
class SylvanDdManagerEnvironment;

// Select the sub-environment that belongs to the given DD type.
template<storm::dd::DdType Type>
struct DdEnvironmentSelector {
    static_assert(Type == storm::dd::DdType::CUDD || Type == storm::dd::DdType::Sylvan, "Unhandled DD type.");
};
template<>
struct DdEnvironmentSelector<storm::dd::DdType::CUDD> {
    using type = CuddDdManagerEnvironment;
};
template<>
struct DdEnvironmentSelector<storm::dd::DdType::Sylvan> {
    using type = SylvanDdManagerEnvironment;
};

class DdEnvironment {
   public:
    DdEnvironment();
    ~DdEnvironment();

    CuddDdManagerEnvironment& cudd();
    CuddDdManagerEnvironment const& cudd() const;

    SylvanDdManagerEnvironment& sylvan();
    SylvanDdManagerEnvironment const& sylvan() const;

    /*!
     * Retrieves the sub-environment belonging to the given DD type.
     */
    template<storm::dd::DdType Type>
    typename DdEnvironmentSelector<Type>::type& get();

    template<storm::dd::DdType Type>
    typename DdEnvironmentSelector<Type>::type const& get() const;

   private:
    SubEnvironment<CuddDdManagerEnvironment> cuddEnvironment;
    SubEnvironment<SylvanDdManagerEnvironment> sylvanEnvironment;
};

}  // namespace storm
