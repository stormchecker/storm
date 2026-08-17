#include "storm/environment/dd/DdEnvironment.h"

#include "storm/environment/dd/CuddDdManagerEnvironment.h"
#include "storm/environment/dd/SylvanDdManagerEnvironment.h"

namespace storm {

DdEnvironment::DdEnvironment() {
    // Intentionally left empty
}

DdEnvironment::~DdEnvironment() {
    // Intentionally left empty
}

SylvanDdManagerEnvironment& DdEnvironment::sylvan() {
    return sylvanEnvironment.get();
}

SylvanDdManagerEnvironment const& DdEnvironment::sylvan() const {
    return sylvanEnvironment.get();
}

CuddDdManagerEnvironment& DdEnvironment::cudd() {
    return cuddEnvironment.get();
}

CuddDdManagerEnvironment const& DdEnvironment::cudd() const {
    return cuddEnvironment.get();
}

template<storm::dd::DdType Type>
typename DdEnvironmentSelector<Type>::type& DdEnvironment::get() {
    if constexpr (Type == storm::dd::DdType::Sylvan) {
        return this->sylvan();
    } else {
        static_assert(Type == storm::dd::DdType::CUDD, "Unhandled DD type.");
        return this->cudd();
    }
}

template<storm::dd::DdType Type>
typename DdEnvironmentSelector<Type>::type const& DdEnvironment::get() const {
    if constexpr (Type == storm::dd::DdType::Sylvan) {
        return this->sylvan();
    } else {
        static_assert(Type == storm::dd::DdType::CUDD, "Unhandled DD type.");
        return this->cudd();
    }
}

template SylvanDdManagerEnvironment& DdEnvironment::get<storm::dd::DdType::Sylvan>();
template CuddDdManagerEnvironment& DdEnvironment::get<storm::dd::DdType::CUDD>();
template SylvanDdManagerEnvironment const& DdEnvironment::get<storm::dd::DdType::Sylvan>() const;
template CuddDdManagerEnvironment const& DdEnvironment::get<storm::dd::DdType::CUDD>() const;

}  // namespace storm
