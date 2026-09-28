#pragma once

#include <memory>

namespace storm {

// The member functions below are declared but defined out-of-line afterwards.
// This is needed because `extern template class` otherwise still implicitly inlines and instantiates member functions.

template<typename EnvironmentType>
class SubEnvironment {
   public:
    SubEnvironment();
    SubEnvironment(SubEnvironment const& other);
    SubEnvironment<EnvironmentType>& operator=(SubEnvironment const& other);
    EnvironmentType const& get() const;
    EnvironmentType& get();

   private:
    void assertInitialized() const;
    mutable std::unique_ptr<EnvironmentType> subEnv;
};

template<typename EnvironmentType>
SubEnvironment<EnvironmentType>::SubEnvironment() : subEnv(nullptr) {
    // Intentionally left empty
}

template<typename EnvironmentType>
SubEnvironment<EnvironmentType>::SubEnvironment(SubEnvironment const& other) : subEnv(other.subEnv ? new EnvironmentType(*other.subEnv) : nullptr) {
    // Intentionally left empty
}

template<typename EnvironmentType>
SubEnvironment<EnvironmentType>& SubEnvironment<EnvironmentType>::operator=(SubEnvironment const& other) {
    if (other.subEnv) {
        subEnv = std::make_unique<EnvironmentType>(*other.subEnv);
    } else {
        subEnv.reset();
    }
    return *this;
}

template<typename EnvironmentType>
EnvironmentType const& SubEnvironment<EnvironmentType>::get() const {
    assertInitialized();
    return *subEnv;
}

template<typename EnvironmentType>
EnvironmentType& SubEnvironment<EnvironmentType>::get() {
    assertInitialized();
    return *subEnv;
}

template<typename EnvironmentType>
void SubEnvironment<EnvironmentType>::assertInitialized() const {
    if (!subEnv) {
        subEnv = std::make_unique<EnvironmentType>();
    }
}

}  // namespace storm
