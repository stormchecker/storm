#pragma once

#include <cmath>
#include <map>
#include <memory>
#include "storm/storage/BitVector.h"

namespace storm {
namespace gspn {
class Marking {
   public:
    /*!
     * Creates an empty marking (at all places contain 0 tokens).
     *
     * @param numberOfPlaces The number of places of the gspn.
     * @param numberOfBits The number of bits used to store the number of tokens for each place
     * @param numberOfTotalBits The length of the internal bitvector
     */
    Marking(uint_fast64_t const& numberOfPlaces, std::map<uint_fast64_t, uint_fast64_t> const& numberOfBits, uint_fast64_t const& numberOfTotalBits);

    /*!
     * Create the marking described by the bitvector.
     *
     * @param numberOfPlaces The number of places of the gspn.
     * @param numberOfBits The number of bits used to store the number of tokens for each place
     * @param bitvector The bitvector encoding the marking.
     */
    Marking(uint_fast64_t const& numberOfPlaces, std::map<uint_fast64_t, uint_fast64_t> const& numberOfBits, storm::storage::BitVector const& bitvector);

    /*!
     * Retrieves the number of places for which the tokens are stored.
     *
     * @return The number of places.
     */
    uint_fast64_t getNumberOfPlaces() const;

    /*!
     * Set the number of tokens for the given place to the given amount.
     *
     * @param place Place must be a valid place for which the number of tokens is changed.
     * @param numberOfTokens The new number of tokens at the place.
     */
    void setNumberOfTokensAt(uint_fast64_t const& place, uint_fast64_t const& numberOfTokens);

    /*!
     * Get the number of tokens for the given place.
     *
     * @param place The place from which the tokens are counted.
     * @return The number of tokens at the place.
     */
    uint_fast64_t getNumberOfTokensAt(uint_fast64_t const& place) const;

    /*!
     * Returns a copy of the bitvector
     *
     * @return The bitvector which encodes the marking
     */
    std::shared_ptr<storm::storage::BitVector> getBitVector() const;

    /*!
     * Overload equality operator
     */
    bool operator==(const Marking& other) const;

   private:
    // the maximal number of places in the gspn
    uint_fast64_t numberOfPlaces;

    // number of bits for each place
    std::map<uint_fast64_t, uint_fast64_t> numberOfBits;

    // contains the information of the marking
    storm::storage::BitVector marking;
};
}  // namespace gspn
}  // namespace storm
