#include "storm/dd/cudd/InternalCuddDdManager.h"

#include "storm/exceptions/MissingLibraryException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

#ifdef STORM_HAVE_CUDD
InternalDdManager<DdType::CUDD>::InternalDdManager(storm::CuddDdManagerEnvironment const& environment) : cuddManager(), numberOfDdVariables(0) {
    this->cuddManager.SetMaxMemory(static_cast<unsigned long>(environment.getMaximalMemory() * 1024ul * 1024ul));
    this->cuddManager.SetEpsilon(environment.getConstantPrecision());

    // Dynamic reordering is currently broken with CUDD, as several places (most notably the construction of DDs) rely on
    // the variable order not changing. We therefore disable it unconditionally. See stormchecker/storm#119.
    this->getCuddManager().AutodynDisable();
}

InternalDdManager<DdType::CUDD>::~InternalDdManager() {
    // Intentionally left empty.
}

InternalBdd<DdType::CUDD> InternalDdManager<DdType::CUDD>::getBddOne() const {
    return InternalBdd<DdType::CUDD>(this, cuddManager.bddOne());
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalDdManager<DdType::CUDD>::getAddOne() const {
    return InternalAdd<DdType::CUDD, ValueType>(this, cuddManager.addOne());
}

InternalBdd<DdType::CUDD> InternalDdManager<DdType::CUDD>::getBddZero() const {
    return InternalBdd<DdType::CUDD>(this, cuddManager.bddZero());
}

InternalBdd<DdType::CUDD> InternalDdManager<DdType::CUDD>::getBddEncodingLessOrEqualThan(uint64_t bound, InternalBdd<DdType::CUDD> const& cube,
                                                                                         uint64_t numberOfDdVariables) const {
    return InternalBdd<DdType::CUDD>(this, cudd::BDD(cuddManager, this->getBddEncodingLessOrEqualThanRec(0, (1ull << numberOfDdVariables) - 1, bound,
                                                                                                         cube.getCuddDdNode(), numberOfDdVariables)));
}

DdNodePtr InternalDdManager<DdType::CUDD>::getBddEncodingLessOrEqualThanRec(uint64_t minimalValue, uint64_t maximalValue, uint64_t bound, DdNodePtr cube,
                                                                            uint64_t remainingDdVariables) const {
    if (maximalValue <= bound) {
        return Cudd_ReadOne(cuddManager.getManager());
    } else if (minimalValue > bound) {
        return Cudd_ReadLogicZero(cuddManager.getManager());
    }

    STORM_LOG_ASSERT(remainingDdVariables > 0, "Expected more remaining DD variables.");
    STORM_LOG_ASSERT(!Cudd_IsConstant(cube), "Expected non-constant cube.");
    uint64_t newRemainingDdVariables = remainingDdVariables - 1;
    DdNodePtr elseResult =
        getBddEncodingLessOrEqualThanRec(minimalValue, maximalValue & ~(1ull << newRemainingDdVariables), bound, Cudd_T(cube), newRemainingDdVariables);
    Cudd_Ref(elseResult);
    DdNodePtr thenResult =
        getBddEncodingLessOrEqualThanRec(minimalValue | (1ull << newRemainingDdVariables), maximalValue, bound, Cudd_T(cube), newRemainingDdVariables);
    Cudd_Ref(thenResult);
    STORM_LOG_ASSERT(thenResult != elseResult, "Expected different results.");

    bool complemented = Cudd_IsComplement(thenResult);
    DdNodePtr result =
        cuddUniqueInter(cuddManager.getManager(), Cudd_NodeReadIndex(cube), Cudd_Regular(thenResult), complemented ? Cudd_Not(elseResult) : elseResult);
    if (complemented) {
        result = Cudd_Not(result);
    }
    Cudd_Deref(thenResult);
    Cudd_Deref(elseResult);
    return result;
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalDdManager<DdType::CUDD>::getAddZero() const {
    return InternalAdd<DdType::CUDD, ValueType>(this, cuddManager.addZero());
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalDdManager<DdType::CUDD>::getAddUndefined() const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Undefined values are not supported by CUDD.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalDdManager<DdType::CUDD>::getConstant(ValueType const& value) const {
    return InternalAdd<DdType::CUDD, ValueType>(this, cuddManager.constant(value));
}

template<>
InternalAdd<DdType::CUDD, storm::RationalNumber> InternalDdManager<DdType::CUDD>::getConstant(storm::RationalNumber const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation not supported.");
}

std::vector<InternalBdd<DdType::CUDD>> InternalDdManager<DdType::CUDD>::createDdVariables(uint64_t numberOfLayers,
                                                                                          boost::optional<uint_fast64_t> const& position) {
    std::vector<InternalBdd<DdType::CUDD>> result;

    if (position) {
        for (uint64_t layer = 0; layer < numberOfLayers; ++layer) {
            result.emplace_back(InternalBdd<DdType::CUDD>(this, cuddManager.bddNewVarAtLevel(position.get() + layer)));
        }
    } else {
        for (uint64_t layer = 0; layer < numberOfLayers; ++layer) {
            result.emplace_back(InternalBdd<DdType::CUDD>(this, cuddManager.bddVar()));
        }
    }

    // Connect the variables so they are not 'torn apart' by dynamic reordering.
    // Note that MTR_FIXED preserves the order of the layers. While this is not always necessary to preserve,
    // (for example) the hybrid engine relies on this connection, so we choose MTR_FIXED instead of MTR_DEFAULT.
    cuddManager.MakeTreeNode(result.front().getIndex(), numberOfLayers, MTR_FIXED);

    // Keep track of the number of variables.
    numberOfDdVariables += numberOfLayers;

    return result;
}

bool InternalDdManager<DdType::CUDD>::supportsOrderedInsertion() const {
    return true;
}

void InternalDdManager<DdType::CUDD>::allowDynamicReordering(bool value) {
    STORM_LOG_THROW(!value, storm::exceptions::NotSupportedException, "Dynamic reordering is not supported for CUDD.");
}

bool InternalDdManager<DdType::CUDD>::isDynamicReorderingAllowed() const {
    return false;
}

void InternalDdManager<DdType::CUDD>::triggerReordering() {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Dynamic reordering is not supported for CUDD.");
}

void InternalDdManager<DdType::CUDD>::debugCheck() const {
    this->getCuddManager().CheckKeys();
    this->getCuddManager().DebugCheck();
}

void InternalDdManager<DdType::CUDD>::execute(std::function<void()> const& f) const {
    f();
}

cudd::Cudd& InternalDdManager<DdType::CUDD>::getCuddManager() {
    return cuddManager;
}

cudd::Cudd const& InternalDdManager<DdType::CUDD>::getCuddManager() const {
    return cuddManager;
}

uint_fast64_t InternalDdManager<DdType::CUDD>::getNumberOfDdVariables() const {
    return numberOfDdVariables;
}

#else

InternalDdManager<DdType::CUDD>::InternalDdManager(storm::CuddDdManagerEnvironment const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalDdManager<DdType::CUDD>::~InternalDdManager() {
    // Intentionally left empty.
}

InternalBdd<DdType::CUDD> InternalDdManager<DdType::CUDD>::getBddOne() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalDdManager<DdType::CUDD>::getAddOne() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalDdManager<DdType::CUDD>::getBddZero() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

InternalBdd<DdType::CUDD> InternalDdManager<DdType::CUDD>::getBddEncodingLessOrEqualThan(uint64_t bound, InternalBdd<DdType::CUDD> const& cube,
                                                                                         uint64_t numberOfDdVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalDdManager<DdType::CUDD>::getAddZero() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalDdManager<DdType::CUDD>::getAddUndefined() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<typename ValueType>
InternalAdd<DdType::CUDD, ValueType> InternalDdManager<DdType::CUDD>::getConstant(ValueType const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

template<>
InternalAdd<DdType::CUDD, storm::RationalNumber> InternalDdManager<DdType::CUDD>::getConstant(storm::RationalNumber const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

std::vector<InternalBdd<DdType::CUDD>> InternalDdManager<DdType::CUDD>::createDdVariables(uint64_t numberOfLayers,
                                                                                          boost::optional<uint_fast64_t> const& position) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

bool InternalDdManager<DdType::CUDD>::supportsOrderedInsertion() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

void InternalDdManager<DdType::CUDD>::allowDynamicReordering(bool value) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

bool InternalDdManager<DdType::CUDD>::isDynamicReorderingAllowed() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

void InternalDdManager<DdType::CUDD>::triggerReordering() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

void InternalDdManager<DdType::CUDD>::debugCheck() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

void InternalDdManager<DdType::CUDD>::execute(std::function<void()> const& f) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}

uint_fast64_t InternalDdManager<DdType::CUDD>::getNumberOfDdVariables() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for CUDD. Yet, a method was called that requires this support. Please choose a version "
                    "of Storm with CUDD support.");
}
#endif

template InternalAdd<DdType::CUDD, double> InternalDdManager<DdType::CUDD>::getAddOne() const;
template InternalAdd<DdType::CUDD, uint_fast64_t> InternalDdManager<DdType::CUDD>::getAddOne() const;
template InternalAdd<DdType::CUDD, storm::RationalNumber> InternalDdManager<DdType::CUDD>::getAddOne() const;

template InternalAdd<DdType::CUDD, double> InternalDdManager<DdType::CUDD>::getAddZero() const;
template InternalAdd<DdType::CUDD, uint_fast64_t> InternalDdManager<DdType::CUDD>::getAddZero() const;
template InternalAdd<DdType::CUDD, storm::RationalNumber> InternalDdManager<DdType::CUDD>::getAddZero() const;

template InternalAdd<DdType::CUDD, double> InternalDdManager<DdType::CUDD>::getConstant(double const& value) const;
template InternalAdd<DdType::CUDD, uint_fast64_t> InternalDdManager<DdType::CUDD>::getConstant(uint_fast64_t const& value) const;
}  // namespace dd
}  // namespace storm
