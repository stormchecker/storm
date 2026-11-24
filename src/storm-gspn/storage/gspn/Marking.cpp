#include "Marking.h"

#include <cstdint>

namespace storm {
namespace gspn {
Marking::Marking(uint_fast64_t const& numberOfPlaces, std::map<uint_fast64_t, uint_fast64_t> const& numberOfBits, uint_fast64_t const& numberOfTotalBits) {
    this->numberOfPlaces = numberOfPlaces;
    this->numberOfBits = numberOfBits;
    this->marking = storm::storage::BitVector(numberOfTotalBits);
}

Marking::Marking(uint_fast64_t const& numberOfPlaces, std::map<uint_fast64_t, uint_fast64_t> const& numberOfBits, storm::storage::BitVector const& bitvector) {
    this->numberOfPlaces = numberOfPlaces;
    this->numberOfBits = numberOfBits;
    this->marking = bitvector;
}

uint_fast64_t Marking::getNumberOfPlaces() const {
    return this->numberOfPlaces;
}

void Marking::setNumberOfTokensAt(uint_fast64_t const& place, uint_fast64_t const& numberOfTokens) {
    uint_fast64_t index = 0;
    for (uint_fast64_t i = 0; i < place; ++i) {
        index += numberOfBits[i];
    }
    marking.setFromInt(index, numberOfBits[place], numberOfTokens);
}

uint_fast64_t Marking::getNumberOfTokensAt(uint_fast64_t const& place) const {
    uint_fast64_t index = 0;
    for (uint_fast64_t i = 0; i < place; ++i) {
        index += numberOfBits.at(i);
    }
    return marking.getAsInt(index, numberOfBits.at(place));
}

std::shared_ptr<storm::storage::BitVector> Marking::getBitVector() const {
    auto result = std::make_shared<storm::storage::BitVector>();
    *result = storm::storage::BitVector(marking);
    return result;
}

bool Marking::operator==(const Marking& other) const {
    if (getNumberOfPlaces() != other.getNumberOfPlaces()) {
        return false;
    }
    if (&numberOfBits == &other.numberOfBits) {
        return marking == other.marking;
    }
    for (uint_fast64_t i = 0; i < getNumberOfPlaces(); ++i) {
        if (getNumberOfTokensAt(i) != other.getNumberOfTokensAt(i)) {
            return false;
        }
    }
    return true;
}
}  // namespace gspn
}  // namespace storm
