#ifndef STATEACTIONPAIR_H
#define STATEACTIONPAIR_H

#include <cstdint>
#include <memory>

namespace storm {
namespace storage {
class StateActionPair {
    std::pair<uint_fast64_t, uint_fast64_t> stateActionPair;

   public:
    StateActionPair(std::pair<uint_fast64_t, uint_fast64_t> const& sap) : stateActionPair(sap) {}
    StateActionPair(uint_fast64_t state, uint_fast64_t action) : stateActionPair(std::make_pair(state, action)) {}

    uint_fast64_t getState() const {
        return stateActionPair.first;
    }

    uint_fast64_t getAction() const {
        return stateActionPair.second;
    }

    friend bool operator==(StateActionPair const& p1, StateActionPair const& p2) {
        return p1.stateActionPair == p2.stateActionPair;
    }

    friend bool operator!=(StateActionPair const& p1, StateActionPair const& p2) {
        return p1.stateActionPair != p2.stateActionPair;
    }

    friend bool operator<(StateActionPair const& p1, StateActionPair const& p2) {
        return p1.getState() < p2.getState() || (p1.getState() == p2.getState() && p1.getAction() < p2.getAction());
    }
};
}  // namespace storage
}  // namespace storm

namespace std {
template<>
struct hash<storm::storage::StateActionPair> {
    size_t operator()(storm::storage::StateActionPair const& sap) const {
        return (sap.getState() << 3 ^ sap.getAction());
    }
};

}  // namespace std

#endif /* STATEACTIONPAIR_H */
