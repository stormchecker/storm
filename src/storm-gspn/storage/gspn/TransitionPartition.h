#pragma once
#include <cstdint>
#include <vector>

namespace storm {
namespace gspn {

struct TransitionPartition {
    std::vector<uint64_t> transitions;
    uint64_t priority;

    uint64_t nrTransitions() const {
        return transitions.size();
    }
};

}  // namespace gspn
}  // namespace storm