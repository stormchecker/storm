#include "Place.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace gspn {
Place::Place(uint64_t id) : id(id) {}

void Place::setName(std::string const& name) {
    this->name = name;
}

std::string Place::getName() const {
    return this->name;
}

uint64_t Place::getID() const {
    return this->id;
}

void Place::setNumberOfInitialTokens(uint64_t tokens) {
    this->numberOfInitialTokens = tokens;
}

uint64_t Place::getNumberOfInitialTokens() const {
    return this->numberOfInitialTokens;
}

void Place::setCapacity(boost::optional<uint64_t> const& cap) {
    this->capacity = cap;
}

uint64_t Place::getCapacity() const {
    STORM_LOG_ASSERT(hasRestrictedCapacity(), "Place does not have restricted capacity.");
    return capacity.get();
}

bool Place::hasRestrictedCapacity() const {
    return capacity != boost::none;
}
}  // namespace gspn
}  // namespace storm
