#pragma once

#include <boost/optional.hpp>
#include <functional>

#include "storm-config.h"
#include "storm/environment/dd/CuddDdManagerEnvironment.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/InternalDdManager.h"
#include "storm/storage/dd/cudd/InternalCuddAdd.h"
#include "storm/storage/dd/cudd/InternalCuddBdd.h"

#ifdef STORM_HAVE_CUDD
#include "cuddObj.hh"
#endif

namespace storm {
namespace dd {
template<DdType LibraryType, typename ValueType>
class InternalAdd;

template<DdType LibraryType>
class InternalBdd;

template<>
class InternalDdManager<DdType::CUDD> {
   public:
    friend class InternalBdd<DdType::CUDD>;

    template<DdType LibraryType, typename ValueType>
    friend class InternalAdd;

    /*!
     * Creates a new internal manager for CUDD DDs.
     *
     * @param environment The environment providing the settings for the CUDD manager.
     */
    explicit InternalDdManager(storm::CuddDdManagerEnvironment const& environment);

    /*!
     * Destroys the CUDD manager.
     */
    ~InternalDdManager();

    /*!
     * Retrieves a BDD representing the constant one function.
     *
     * @return A BDD representing the constant one function.
     */
    InternalBdd<DdType::CUDD> getBddOne() const;

    /*!
     * Retrieves an ADD representing the constant one function.
     *
     * @return An ADD representing the constant one function.
     */
    template<typename ValueType>
    InternalAdd<DdType::CUDD, ValueType> getAddOne() const;

    /*!
     * Retrieves a BDD representing the constant zero function.
     *
     * @return A BDD representing the constant zero function.
     */
    InternalBdd<DdType::CUDD> getBddZero() const;

    /*!
     * Retrieves a BDD that maps to true iff the encoding is less or equal than the given bound.
     *
     * @return A BDD with encodings corresponding to values less or equal than the bound.
     */
    InternalBdd<DdType::CUDD> getBddEncodingLessOrEqualThan(uint64_t bound, InternalBdd<DdType::CUDD> const& cube, uint64_t numberOfDdVariables) const;

    /*!
     * Retrieves an ADD representing the constant zero function.
     *
     * @return An ADD representing the constant zero function.
     */
    template<typename ValueType>
    InternalAdd<DdType::CUDD, ValueType> getAddZero() const;

    /*!
     * Retrieves an ADD representing an undefined value.
     *
     * @return An ADD representing an undefined value.
     */
    template<typename ValueType>
    InternalAdd<DdType::CUDD, ValueType> getAddUndefined() const;

    /*!
     * Retrieves an ADD representing the constant function with the given value.
     *
     * @return An ADD representing the constant function with the given value.
     */
    template<typename ValueType>
    InternalAdd<DdType::CUDD, ValueType> getConstant(ValueType const& value) const;

    /*!
     * Creates new layered DD variables and returns the cubes as a result.
     *
     * @param position An optional position at which to insert the new variable. This may only be given, if the
     * manager supports ordered insertion.
     * @return The cubes belonging to the DD variables.
     */
    std::vector<InternalBdd<DdType::CUDD>> createDdVariables(uint64_t numberOfLayers, boost::optional<uint_fast64_t> const& position = boost::none);

    /*!
     * Checks whether this manager supports the ordered insertion of variables, i.e. inserting variables at
     * positions between already existing variables.
     *
     * @return True iff the manager supports ordered insertion.
     */
    bool supportsOrderedInsertion() const;

    /*!
     * Sets whether or not dynamic reordering is allowed for the DDs managed by this manager.
     *
     * @param value If set to true, dynamic reordering is allowed and forbidden otherwise.
     */
    void allowDynamicReordering(bool value);

    /*!
     * Retrieves whether dynamic reordering is currently allowed.
     *
     * @return True iff dynamic reordering is currently allowed.
     */
    bool isDynamicReorderingAllowed() const;

    /*!
     * Triggers a reordering of the DDs managed by this manager.
     */
    void triggerReordering();

    /*!
     * Performs a debug check if available.
     */
    void debugCheck() const;

    /*!
     * All code that manipulates DDs shall be called through this function.
     * This is generally needed to set-up the correct context.
     * For Cudd, this just execute the given function.
     *
     * @param f the function that is executed
     */
    void execute(std::function<void()> const& f) const;

    /*!
     * Retrieves the number of DD variables managed by this manager.
     *
     * @return The number of managed variables.
     */
    uint_fast64_t getNumberOfDdVariables() const;

#ifdef STORM_HAVE_CUDD
    /*!
     * Retrieves the underlying CUDD manager.
     *
     * @return The underlying CUDD manager.
     */
    cudd::Cudd& getCuddManager();

    /*!
     * Retrieves the underlying CUDD manager.
     *
     * @return The underlying CUDD manager.
     */
    cudd::Cudd const& getCuddManager() const;
#endif

   private:
#ifdef STORM_HAVE_CUDD
    // Helper function to create the BDD whose encodings are below a given bound.
    DdNodePtr getBddEncodingLessOrEqualThanRec(uint64_t minimalValue, uint64_t maximalValue, uint64_t bound, DdNodePtr cube,
                                               uint64_t remainingDdVariables) const;

    // The manager responsible for the DDs created/modified with this DdManager.
    cudd::Cudd cuddManager;

    // The technique that is used for dynamic reordering.
    Cudd_ReorderingType reorderingTechnique;

    // Keeps track of the number of registered DD variables.
    uint_fast64_t numberOfDdVariables;
#endif
};
}  // namespace dd
}  // namespace storm
