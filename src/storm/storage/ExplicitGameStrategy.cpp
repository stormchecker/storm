#include "ExplicitGameStrategy.h"

#include <algorithm>
#include <limits>

namespace storm {
namespace storage {

const uint64_t ExplicitGameStrategy::UNDEFINED = std::numeric_limits<uint64_t>::max();

ExplicitGameStrategy::ExplicitGameStrategy(uint64_t numberOfStates) : choices(numberOfStates, UNDEFINED) {
    // Intentionally left empty.
}

ExplicitGameStrategy::ExplicitGameStrategy(std::vector<uint64_t>&& choices) : choices(std::move(choices)) {
    // Intentionally left empty.
}

uint64_t ExplicitGameStrategy::getNumberOfStates() const {
    return choices.size();
}

uint64_t ExplicitGameStrategy::getChoice(uint64_t state) const {
    return choices[state];
}

void ExplicitGameStrategy::setChoice(uint64_t state, uint64_t choice) {
    choices[state] = choice;
}

bool ExplicitGameStrategy::hasDefinedChoice(uint64_t state) const {
    return choices[state] != UNDEFINED;
}

void ExplicitGameStrategy::undefineAll() {
    for (auto& e : choices) {
        e = UNDEFINED;
    }
}

uint64_t ExplicitGameStrategy::getNumberOfUndefinedStates() const {
    return std::count_if(choices.begin(), choices.end(), [](uint64_t choice) { return choice == UNDEFINED; });
}

std::ostream& operator<<(std::ostream& out, ExplicitGameStrategy const& strategy) {
    std::vector<uint64_t> undefinedStates;
    for (uint64_t state = 0; state < strategy.getNumberOfStates(); ++state) {
        uint64_t choice = strategy.getChoice(state);
        if (choice == ExplicitGameStrategy::UNDEFINED) {
            undefinedStates.emplace_back(state);
        } else {
            out << state << " -> " << choice << '\n';
        }
    }
    out << "undefined states: ";
    for (auto state : undefinedStates) {
        out << state << ", ";
    }
    out << '\n';

    return out;
}

}  // namespace storage
}  // namespace storm
