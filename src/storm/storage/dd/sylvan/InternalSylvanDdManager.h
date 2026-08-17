#pragma once

#include <boost/optional.hpp>

#include "storm-config.h"
#include "storm/adapters/RationalFunctionForward.h"
#include "storm/environment/dd/SylvanDdManagerEnvironment.h"
#include "storm/storage/dd/DdType.h"
#include "storm/storage/dd/InternalDdManager.h"
#include "storm/storage/dd/sylvan/InternalSylvanAdd.h"
#include "storm/storage/dd/sylvan/InternalSylvanBdd.h"

namespace storm {
namespace dd {
template<DdType LibraryType, typename ValueType>
class InternalAdd;

template<DdType LibraryType>
class InternalBdd;

template<>
class InternalDdManager<DdType::Sylvan> {
   public:
    friend class InternalBdd<DdType::Sylvan>;

    template<DdType LibraryType, typename ValueType>
    friend class InternalAdd;

    /*!
     * Creates a new internal manager for Sylvan DDs.
     *
     * @param environment The environment providing the settings for the Sylvan manager.
     */
    explicit InternalDdManager(storm::SylvanDdManagerEnvironment const& environment);

    /*!
     * Destroys the internal manager.
     */
    ~InternalDdManager();

    /*!
     * Retrieves a BDD representing the constant one function.
     *
     * @return A BDD representing the constant one function.
     */
    InternalBdd<DdType::Sylvan> getBddOne() const;

    /*!
     * Retrieves an ADD representing the constant one function.
     *
     * @return An ADD representing the constant one function.
     */
    template<typename ValueType>
    InternalAdd<DdType::Sylvan, ValueType> getAddOne() const;

    /*!
     * Retrieves a BDD representing the constant zero function.
     *
     * @return A BDD representing the constant zero function.
     */
    InternalBdd<DdType::Sylvan> getBddZero() const;

    /*!
     * Retrieves a BDD that maps to true iff the encoding is less or equal than the given bound.
     *
     * @return A BDD with encodings corresponding to values less or equal than the bound.
     */
    InternalBdd<DdType::Sylvan> getBddEncodingLessOrEqualThan(uint64_t bound, InternalBdd<DdType::Sylvan> const& cube, uint64_t numberOfDdVariables) const;

    /*!
     * Retrieves an ADD representing the constant zero function.
     *
     * @return An ADD representing the constant zero function.
     */
    template<typename ValueType>
    InternalAdd<DdType::Sylvan, ValueType> getAddZero() const;

    /*!
     * Retrieves an ADD representing an undefined value.
     *
     * @return An ADD representing an undefined value.
     */
    template<typename ValueType>
    InternalAdd<DdType::Sylvan, ValueType> getAddUndefined() const;

    /*!
     * Retrieves an ADD representing the constant function with the given value.
     *
     * @return An ADD representing the constant function with the given value.
     */
    template<typename ValueType>
    InternalAdd<DdType::Sylvan, ValueType> getConstant(ValueType const& value) const;

    /*!
     * Creates new layered DD variables and returns the cubes as a result.
     *
     * @param position An optional position at which to insert the new variable. This may only be given, if the
     * manager supports ordered insertion.
     * @return The cubes belonging to the DD variables.
     */
    std::vector<InternalBdd<DdType::Sylvan>> createDdVariables(uint64_t numberOfLayers, boost::optional<uint_fast64_t> const& position = boost::none);

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
     * Specifically for sylvan, this is required to make sure that DD-manipulating code is executed as a LACE task.
     * Example usage: `manager->execute([&]() { bar = foo(arg1,arg2); }`
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

   private:
#ifdef STORM_HAVE_SYLVAN

    // Helper function to create the BDD whose encodings are below a given bound.
    BDD getBddEncodingLessOrEqualThanRec(uint64_t minimalValue, uint64_t maximalValue, uint64_t bound, BDD cube, uint64_t remainingDdVariables) const;

    // A counter for the number of instances of this class. This is used to determine when to initialize and
    // quit the sylvan. This is because Sylvan does not know the concept of managers but implicitly has a
    // 'global' manager.
    static uint_fast64_t numberOfInstances;

    // Since the sylvan (more specifically: lace) processes do busy waiting, we suspend them as long as
    // sylvan is not used. This flag keeps track of whether we are currently suspending.
    static bool suspended;

    // The index of the next free variable index. This needs to be shared across all instances since the sylvan
    // manager is implicitly 'global'.
    static uint_fast64_t nextFreeVariableIndex;
#endif
};

}  // namespace dd
}  // namespace storm
