#pragma once

#include <unordered_map>

#include "storm-config.h"
#include "storm/adapters/sylvan.h"
#include "storm/storage/dd/AddIterator.h"
#include "storm/storage/expressions/SimpleValuation.h"

namespace storm {
namespace dd {
// Forward-declare the DdManager class.
template<DdType Type>
class DdManager;

template<DdType Type, typename ValueType>
class InternalAdd;

template<typename ValueType>
class AddIterator<DdType::Sylvan, ValueType> {
   public:
    friend class InternalAdd<DdType::Sylvan, ValueType>;

    // Default-instantiate the constructor.
    AddIterator();

    // Forbid copy-construction and copy assignment, because ownership of the internal pointer is unclear then.
    AddIterator(AddIterator<DdType::Sylvan, ValueType> const& other) = delete;
    AddIterator& operator=(AddIterator<DdType::Sylvan, ValueType> const& other) = delete;

    // Provide move-construction and move-assignment, though.
    AddIterator(AddIterator<DdType::Sylvan, ValueType>&& other) = default;
    AddIterator& operator=(AddIterator<DdType::Sylvan, ValueType>&& other) = default;

    /*!
     * Moves the iterator one position forward.
     */
    AddIterator<DdType::Sylvan, ValueType>& operator++();

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
    bool operator==(AddIterator<DdType::Sylvan, ValueType> const& other) const;

    /*!
     * Compares the iterator with the given one. Two iterators are considered unequal iff they are not
     * considered equal.
     *
     * @param other The iterator with which to compare.
     * @return True if the two iterators are considered unequal.
     */
    bool operator!=(AddIterator<DdType::Sylvan, ValueType> const& other) const;

   private:
#ifdef STORM_HAVE_SYLVAN
    /*!
     * Constructs a forward iterator using the given generator with the given set of relevant meta variables.
     *
     * @param ddManager The manager responsible for the DD over which to iterate.
     * @param mtbdd The MTBDD over which to iterate.
     * @param variables The variables contained in the MTBDD, represented as a cube.
     * @param numberOfDdVariables The number of DD variables contained in this MTBDD.
     * @param isAtEnd A flag that indicates whether the iterator is at its end and may not be moved forward any
     * more.
     * @param metaVariables The meta variables that appear in the DD.
     * @param enumerateDontCareMetaVariables If set to true, all meta variable assignments are enumerated, even
     * if a meta variable does not at all influence the the function value.
     */
    AddIterator(DdManager<DdType::Sylvan> const& ddManager, sylvan::Mtbdd mtbdd, sylvan::Bdd variables, uint_fast64_t numberOfDdVariables, bool isAtEnd,
                std::set<storm::expressions::Variable> const* metaVariables, bool enumerateDontCareMetaVariables);

    /*!
     * Creates an iterator to the function argument-value-pairs of the given MTBDD.
     *
     * @param ddManager The manager responsible for the DD over which to iterate.
     * @param mtbdd The MTBDD over which to iterate.
     * @param variables The variables contained in the MTBDD, represented as a cube.
     * @param numberOfDdVariables The number of DD variables contained in this MTBDD.
     * @param metaVariables The meta variables that appear in the DD.
     * @param enumerateDontCareMetaVariables If set to true, all meta variable assignments are enumerated, even
     * if a meta variable does not at all influence the the function value.
     */
    static AddIterator createBeginIterator(DdManager<DdType::Sylvan> const& ddManager, sylvan::Mtbdd mtbdd, sylvan::Bdd variables,
                                           uint_fast64_t numberOfDdVariables, std::set<storm::expressions::Variable> const* metaVariables,
                                           bool enumerateDontCareMetaVariables = true);

    /*!
     * Creates an iterator that can be used to determine the end of the iteration process.
     *
     * @param ddManager The manager responsible for the DD over which to iterate.
     */
    static AddIterator createEndIterator(DdManager<DdType::Sylvan> const& ddManager);

    /*!
     * Recreates the internal information when a new cube needs to be treated.
     */
    void treatNewCube();

    /*!
     * Updates the internal information when the next solution of the current cube needs to be treated.
     */
    void treatNextInCube();

    /*!
     * Creates the mapping of global variable indices to local ones.
     */
    void createGlobalToLocalIndexMapping();

    // The manager responsible for the meta variables (and therefore the underlying DD).
    DdManager<DdType::Sylvan> const* ddManager;

    // The MTBDD over which to iterate.
    sylvan::Mtbdd mtbdd;

    // The cube of variables in the MTBDD.
    sylvan::Bdd variables;

    // The currently considered cube of the DD.
    std::vector<uint8_t> cube;

    // The function value of the current cube, represented by the corresponding leaf.
    MTBDD leaf;

    // A flag that indicates whether the iterator is at its end and may not be moved further. This is also used
    // for the check against the end iterator.
    bool isAtEnd;

    // The set of meta variables appearing in the DD.
    std::set<storm::expressions::Variable> const* metaVariables;

    // A mapping from global variable indices to local (i.e. appearing the MTBDD) ones.
    std::unordered_map<uint_fast64_t, uint_fast64_t> globalToLocalIndexMap;

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
