#pragma once

#include "storm-config.h"

#include <cstdint>
#include <memory>
#include <set>
#include <tuple>
#include <utility>

#include "storm/storage/dd/AddIterator.h"
#include "storm/storage/expressions/SimpleValuation.h"

#ifdef STORM_HAVE_CUDD
// Include the C++-interface of CUDD.
#include "cuddObj.hh"
#endif

namespace storm {
namespace dd {
// Forward-declare the DdManager class.
template<DdType Type>
class DdManager;

template<DdType Type, typename ValueType>
class InternalAdd;

template<typename ValueType>
class AddIterator<DdType::CUDD, ValueType> {
   public:
    friend class InternalAdd<DdType::CUDD, ValueType>;

    // Default-instantiate the constructor.
    AddIterator();

    // Forbid copy-construction and copy assignment, because ownership of the internal pointer is unclear then.
    AddIterator(AddIterator<DdType::CUDD, ValueType> const& other) = delete;
    AddIterator& operator=(AddIterator<DdType::CUDD, ValueType> const& other) = delete;

    // Provide move-construction and move-assignment, though.
    AddIterator(AddIterator<DdType::CUDD, ValueType>&& other);
    AddIterator& operator=(AddIterator<DdType::CUDD, ValueType>&& other);

    /*!
     * Destroys the forward iterator and frees the generator as well as the cube if they are not the nullptr.
     */
    ~AddIterator();

    /*!
     * Moves the iterator one position forward.
     */
    AddIterator<DdType::CUDD, ValueType>& operator++();

    /*!
     * Returns a pair consisting of a valuation of meta variables and the value to which this valuation is
     * mapped. Note that the result is returned by value.
     *
     * @return A pair of a valuation and the function value.
     */
    std::pair<storm::expressions::SimpleValuation, ValueType> operator*() const;

    /*!
     * Compares the iterator with the given one. Two iterators are considered equal when all their underlying
     * data members are the same or they both are at their end.
     *
     * @param other The iterator with which to compare.
     * @return True if the two iterators are considered equal.
     */
    bool operator==(AddIterator<DdType::CUDD, ValueType> const& other) const;

    /*!
     * Compares the iterator with the given one. Two iterators are considered unequal iff they are not
     * considered equal.
     *
     * @param other The iterator with which to compare.
     * @return True if the two iterators are considered unequal.
     */
    bool operator!=(AddIterator<DdType::CUDD, ValueType> const& other) const;

   private:
#ifdef STORM_HAVE_CUDD
    /*!
     * Constructs a forward iterator using the given generator with the given set of relevant meta variables.
     *
     * @param ddManager The manager responsible for the DD over which to iterate.
     * @param generator The generator used to enumerate the cubes of the DD.
     * @param cube The cube as represented by CUDD.
     * @param value The value the cube is mapped to.
     * @param isAtEnd A flag that indicates whether the iterator is at its end and may not be moved forward any
     * more.
     * @param metaVariables The meta variables that appear in the DD.
     * @param enumerateDontCareMetaVariables If set to true, all meta variable assignments are enumerated, even
     * if a meta variable does not at all influence the the function value.
     */
    AddIterator(DdManager<DdType::CUDD> const& ddManager, DdGen* generator, int* cube, ValueType const& value, bool isAtEnd,
                std::set<storm::expressions::Variable> const* metaVariables = nullptr, bool enumerateDontCareMetaVariables = true);

    /*!
     * Recreates the internal information when a new cube needs to be treated.
     */
    void treatNewCube();

    /*!
     * Updates the internal information when the next solution of the current cube needs to be treated.
     */
    void treatNextInCube();

    // The manager responsible for the meta variables (and therefore the underlying DD).
    DdManager<DdType::CUDD> const* ddManager;

    // The CUDD generator used to enumerate the cubes of the DD.
    DdGen* generator;

    // The currently considered cube of the DD.
    int* cube;

    // The function value of the current cube.
    double valueAsDouble;

    // A flag that indicates whether the iterator is at its end and may not be moved further. This is also used
    // for the check against the end iterator.
    bool isAtEnd;

    // The set of meta variables appearing in the DD.
    std::set<storm::expressions::Variable> const* metaVariables;

    // A flag that indicates whether the iterator is supposed to enumerate meta variable valuations even if
    // they don't influence the function value.
    bool enumerateDontCareMetaVariables;

    // A number that represents how many assignments of the current cube have already been returned previously.
    // This is needed, because cubes may represent many assignments (if they have don't care variables).
    uint_fast64_t cubeCounter;

    // A vector of tuples of the form <metaVariable, bitIndex>.
    std::vector<std::tuple<storm::expressions::Variable, uint_fast64_t>> relevantDontCareDdVariables;

    // The current valuation of meta variables.
    storm::expressions::SimpleValuation currentValuation;
#endif
};
}  // namespace dd
}  // namespace storm
