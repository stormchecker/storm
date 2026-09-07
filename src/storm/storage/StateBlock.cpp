#include "StateBlock.h"

namespace storm {
namespace storage {
StateBlock::iterator StateBlock::begin() {
    return states.begin();
}

StateBlock::const_iterator StateBlock::begin() const {
    return states.begin();
}

StateBlock::iterator StateBlock::end() {
    return states.end();
}

StateBlock::const_iterator StateBlock::end() const {
    return states.end();
}

std::size_t StateBlock::size() const {
    return states.size();
}

bool StateBlock::empty() const {
    return states.empty();
}

void StateBlock::insert(value_type const& state) {
    states.insert(state);
}

StateBlock::iterator StateBlock::insert(container_type::const_iterator iterator, value_type const& state) {
    return states.insert(iterator, state);
}

void StateBlock::erase(value_type const& state) {
    states.erase(state);
}

bool StateBlock::containsState(value_type const& state) const {
    return this->states.find(state) != this->states.end();
}

StateBlock::container_type const& StateBlock::getStates() const {
    return this->states;
}

std::ostream& operator<<(std::ostream& out, FlatSetStateContainer const& block) {
    out << "{";
    for (auto const& element : block) {
        out << element << ", ";
    }
    out << "}";
    return out;
}

std::ostream& operator<<(std::ostream& out, StateBlock const& block) {
    out << block.getStates();
    return out;
}

}  // namespace storage
}  // namespace storm
