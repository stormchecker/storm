#include "RewardAccumulation.h"

#include <ostream>

namespace storm {
namespace logic {

RewardAccumulation::RewardAccumulation(bool steps, bool time, bool exit) : time(time), steps(steps), exit(exit) {
    // Intentionally left empty
}

bool RewardAccumulation::isStepsSet() const {
    return steps;
}

bool RewardAccumulation::isTimeSet() const {
    return time;
}

bool RewardAccumulation::isExitSet() const {
    return exit;
}

bool RewardAccumulation::isEmpty() const {
    return !isStepsSet() && !isTimeSet() && !isExitSet();
}

uint64_t RewardAccumulation::size() const {
    return (isStepsSet() ? 1 : 0) + (isTimeSet() ? 1 : 0) + (isExitSet() ? 1 : 0);
}

std::ostream& operator<<(std::ostream& out, RewardAccumulation const& acc) {
    bool hasEntry = false;
    if (acc.isStepsSet()) {
        out << "steps";
        hasEntry = true;
    }
    if (acc.isTimeSet()) {
        if (hasEntry) {
            out << ", ";
        }
        out << "time";
        hasEntry = true;
    }
    if (acc.isExitSet()) {
        if (hasEntry) {
            out << ", ";
        }
        out << "exit";
        hasEntry = true;
    }

    return out;
}

}  // namespace logic
}  // namespace storm
