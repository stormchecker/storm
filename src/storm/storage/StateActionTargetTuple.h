
#ifndef STATEACTIONTARGETTUPLE_H
#define STATEACTIONTARGETTUPLE_H

#include <cstdint>
#include <memory>

namespace storm {
namespace storage {
struct StateActionTarget {
    uint_fast64_t state;
    uint_fast64_t action;
    uint_fast64_t target;
};

inline std::string to_string(StateActionTarget const& sat) {
    return std::to_string(sat.state) + "_" + std::to_string(sat.action) + "_" + std::to_string(sat.target);
}

inline bool operator==(StateActionTarget const& sat1, StateActionTarget const& sat2) {
    return sat1.state == sat2.state && sat1.action == sat2.action && sat1.target == sat2.target;
}

}  // namespace storage
}  // namespace storm

namespace std {
template<>
struct hash<storm::storage::StateActionTarget> {
    bool operator()(storm::storage::StateActionTarget const& sat) const {
        return (sat.state ^ sat.target) << 3 | sat.action;
    }
};

}  // namespace std

#endif /* STATEACTIONTARGETTUPLE_H */
