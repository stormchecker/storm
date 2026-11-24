#pragma once

#include "storm/storage/decomposition/MaximalEndComponent.h"
#include "storm/storage/decomposition/StronglyConnectedComponent.h"

namespace storm {
namespace modelchecker {
namespace helper {
namespace internal {

/// Auxiliary functions that deal with the different kinds of components (MECs on potentially nondeterministic models and BSCCs on deterministic models)
// BSCCS:
inline uint64_t getComponentElementState(typename storm::storage::StronglyConnectedComponent::value_type const& element) {
    return element;
}
inline constexpr uint64_t getComponentElementChoiceCount(typename storm::storage::StronglyConnectedComponent::value_type const& /*element*/) {
    return 1;
}  // Assumes deterministic model!
inline uint64_t const* getComponentElementChoicesBegin(typename storm::storage::StronglyConnectedComponent::value_type const& element) {
    return &element;
}
inline uint64_t const* getComponentElementChoicesEnd(typename storm::storage::StronglyConnectedComponent::value_type const& element) {
    return &element + 1;
}
inline bool componentElementChoicesContains(typename storm::storage::StronglyConnectedComponent::value_type const& element, uint64_t choice) {
    return element == choice;
}
// MECS:
inline uint64_t getComponentElementState(typename storm::storage::MaximalEndComponent::map_type::value_type const& element) {
    return element.first;
}
inline uint64_t getComponentElementChoiceCount(typename storm::storage::MaximalEndComponent::map_type::value_type const& element) {
    return element.second.size();
}
inline typename storm::storage::MaximalEndComponent::set_type::const_iterator getComponentElementChoicesBegin(
    typename storm::storage::MaximalEndComponent::map_type::value_type const& element) {
    return element.second.begin();
}
inline typename storm::storage::MaximalEndComponent::set_type::const_iterator getComponentElementChoicesEnd(
    typename storm::storage::MaximalEndComponent::map_type::value_type const& element) {
    return element.second.end();
}
inline bool componentElementChoicesContains(storm::storage::MaximalEndComponent::map_type::value_type const& element, uint64_t choice) {
    return element.second.count(choice) > 0;
}
}  // namespace internal
}  // namespace helper
}  // namespace modelchecker
}  // namespace storm