#include "storm/storage/dd/sylvan/InternalSylvanDdManager.h"

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/sylvan.h"
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace dd {

#ifdef STORM_HAVE_SYLVAN
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc99-extensions"
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
#pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#pragma clang diagnostic ignored "-Wzero-length-array"

#ifndef NDEBUG
VOID_TASK_0(gc_start) {
    STORM_LOG_TRACE("Starting sylvan garbage collection...");
}

VOID_TASK_0(gc_end) {
    STORM_LOG_TRACE("Sylvan garbage collection done.");
}
#endif

VOID_TASK_2(execute_sylvan, std::function<void()> const*, f, std::exception_ptr*, e) {
    try {
        (*f)();
    } catch (std::exception&) {
        *e = std::current_exception();
    }
}

#pragma clang diagnostic pop
#pragma GCC diagnostic pop

uint_fast64_t InternalDdManager<DdType::Sylvan>::numberOfInstances = 0;
bool InternalDdManager<DdType::Sylvan>::suspended = false;

// It is important that the variable pairs start at an even offset, because sylvan assumes this to be true for
// some operations.
uint_fast64_t InternalDdManager<DdType::Sylvan>::nextFreeVariableIndex = 0;

uint_fast64_t findLargestPowerOfTwoFitting(uint_fast64_t number) {
    for (uint_fast64_t index = 0; index < 64; ++index) {
        if ((number & (1ull << (63 - index))) != 0) {
            return 63 - index;
        }
    }
    return 0;
}

InternalDdManager<DdType::Sylvan>::InternalDdManager(storm::SylvanDdManagerEnvironment const& environment) {
    if (numberOfInstances == 0) {
        size_t const task_deque_size = 1024 * 1024;

        lace_set_stacksize(1024 * 1024 * 16);  // 16 MiB

        lace_start(environment.getNumberOfThreads(), task_deque_size);

        sylvan_set_limits(environment.getMaximalMemory() * 1024 * 1024, 0, 0);
        sylvan_init_package();

        sylvan::Sylvan::initBdd();
        sylvan::Sylvan::initMtbdd();
        sylvan::Sylvan::initCustomMtbdd();

#ifndef NDEBUG
        sylvan_gc_hook_pregc(TASK(gc_start));
        sylvan_gc_hook_postgc(TASK(gc_end));
#endif
        // TODO: uncomment these to disable lace threads whenever they are not used. This requires that *all* DD code is run through execute
        // lace_suspend();
        // suspended = true;
    }
    ++numberOfInstances;
}

InternalDdManager<DdType::Sylvan>::~InternalDdManager() {
    --numberOfInstances;
    if (numberOfInstances == 0) {
        // Enable this to print the sylvan statistics to a file.
        //                FILE* filePointer = fopen("sylvan.stats", "w");
        //                sylvan_stats_report(filePointer, 0);
        //                fclose(filePointer);

        sylvan::Sylvan::quitPackage();
        lace_stop();
    }
}

InternalBdd<DdType::Sylvan> InternalDdManager<DdType::Sylvan>::getBddOne() const {
    return InternalBdd<DdType::Sylvan>(this, sylvan::Bdd::bddOne());
}

template<>
InternalAdd<DdType::Sylvan, double> InternalDdManager<DdType::Sylvan>::getAddOne() const {
    return InternalAdd<DdType::Sylvan, double>(this, sylvan::Mtbdd::doubleTerminal(storm::utility::one<double>()));
}

template<>
InternalAdd<DdType::Sylvan, uint_fast64_t> InternalDdManager<DdType::Sylvan>::getAddOne() const {
    return InternalAdd<DdType::Sylvan, uint_fast64_t>(this, sylvan::Mtbdd::int64Terminal(storm::utility::one<uint_fast64_t>()));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalDdManager<DdType::Sylvan>::getAddOne() const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(this, sylvan::Mtbdd::stormRationalNumberTerminal(storm::utility::one<storm::RationalNumber>()));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalDdManager<DdType::Sylvan>::getAddOne() const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(this,
                                                                sylvan::Mtbdd::stormRationalFunctionTerminal(storm::utility::one<storm::RationalFunction>()));
}

InternalBdd<DdType::Sylvan> InternalDdManager<DdType::Sylvan>::getBddZero() const {
    return InternalBdd<DdType::Sylvan>(this, sylvan::Bdd::bddZero());
}

InternalBdd<DdType::Sylvan> InternalDdManager<DdType::Sylvan>::getBddEncodingLessOrEqualThan(uint64_t bound, InternalBdd<DdType::Sylvan> const& cube,
                                                                                             uint64_t numberOfDdVariables) const {
    return InternalBdd<DdType::Sylvan>(this, sylvan::Bdd(this->getBddEncodingLessOrEqualThanRec(0, (1ull << numberOfDdVariables) - 1, bound,
                                                                                                cube.getSylvanBdd().GetBDD(), numberOfDdVariables)));
}

BDD InternalDdManager<DdType::Sylvan>::getBddEncodingLessOrEqualThanRec(uint64_t minimalValue, uint64_t maximalValue, uint64_t bound, BDD cube,
                                                                        uint64_t remainingDdVariables) const {
    if (maximalValue <= bound) {
        return sylvan_true;
    } else if (minimalValue > bound) {
        return sylvan_false;
    }

    STORM_LOG_ASSERT(remainingDdVariables > 0, "Expected more remaining DD variables.");
    uint64_t newRemainingDdVariables = remainingDdVariables - 1;
    BDD elseResult =
        getBddEncodingLessOrEqualThanRec(minimalValue, maximalValue & ~(1ull << newRemainingDdVariables), bound, sylvan_high(cube), newRemainingDdVariables);
    bdd_refs_push(elseResult);
    BDD thenResult =
        getBddEncodingLessOrEqualThanRec(minimalValue | (1ull << newRemainingDdVariables), maximalValue, bound, sylvan_high(cube), newRemainingDdVariables);
    bdd_refs_push(elseResult);
    BDD result = sylvan_makenode(sylvan_var(cube), elseResult, thenResult);
    bdd_refs_pop(2);
    return result;
}

template<>
InternalAdd<DdType::Sylvan, double> InternalDdManager<DdType::Sylvan>::getAddZero() const {
    return InternalAdd<DdType::Sylvan, double>(this, sylvan::Mtbdd::doubleTerminal(storm::utility::zero<double>()));
}

template<>
InternalAdd<DdType::Sylvan, uint_fast64_t> InternalDdManager<DdType::Sylvan>::getAddZero() const {
    return InternalAdd<DdType::Sylvan, uint_fast64_t>(this, sylvan::Mtbdd::int64Terminal(storm::utility::zero<uint_fast64_t>()));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalDdManager<DdType::Sylvan>::getAddZero() const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(this, sylvan::Mtbdd::stormRationalNumberTerminal(storm::utility::zero<storm::RationalNumber>()));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalDdManager<DdType::Sylvan>::getAddZero() const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(this,
                                                                sylvan::Mtbdd::stormRationalFunctionTerminal(storm::utility::zero<storm::RationalFunction>()));
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalDdManager<DdType::Sylvan>::getAddUndefined() const {
    return InternalAdd<DdType::Sylvan, ValueType>(this, sylvan::Mtbdd(sylvan::Bdd::bddZero()));
}

template<>
InternalAdd<DdType::Sylvan, double> InternalDdManager<DdType::Sylvan>::getConstant(double const& value) const {
    return InternalAdd<DdType::Sylvan, double>(this, sylvan::Mtbdd::doubleTerminal(value));
}

template<>
InternalAdd<DdType::Sylvan, uint_fast64_t> InternalDdManager<DdType::Sylvan>::getConstant(uint_fast64_t const& value) const {
    return InternalAdd<DdType::Sylvan, uint_fast64_t>(this, sylvan::Mtbdd::int64Terminal(value));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalDdManager<DdType::Sylvan>::getConstant(storm::RationalNumber const& value) const {
    return InternalAdd<DdType::Sylvan, storm::RationalNumber>(this, sylvan::Mtbdd::stormRationalNumberTerminal(value));
}

template<>
InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalDdManager<DdType::Sylvan>::getConstant(storm::RationalFunction const& value) const {
    return InternalAdd<DdType::Sylvan, storm::RationalFunction>(this, sylvan::Mtbdd::stormRationalFunctionTerminal(value));
}

std::vector<InternalBdd<DdType::Sylvan>> InternalDdManager<DdType::Sylvan>::createDdVariables(uint64_t numberOfLayers,
                                                                                              boost::optional<uint_fast64_t> const& position) {
    STORM_LOG_THROW(!position, storm::exceptions::NotSupportedException, "The manager does not support ordered insertion.");

    std::vector<InternalBdd<DdType::Sylvan>> result;

    for (uint64_t layer = 0; layer < numberOfLayers; ++layer) {
        result.emplace_back(InternalBdd<DdType::Sylvan>(this, sylvan::Bdd::bddVar(nextFreeVariableIndex)));
        ++nextFreeVariableIndex;
    }

    return result;
}

bool InternalDdManager<DdType::Sylvan>::supportsOrderedInsertion() const {
    return false;
}

void InternalDdManager<DdType::Sylvan>::allowDynamicReordering(bool) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation is not supported by sylvan.");
}

bool InternalDdManager<DdType::Sylvan>::isDynamicReorderingAllowed() const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation is not supported by sylvan.");
}

void InternalDdManager<DdType::Sylvan>::triggerReordering() {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation is not supported by sylvan.");
}

void InternalDdManager<DdType::Sylvan>::debugCheck() const {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Operation is not supported by sylvan.");
}

void InternalDdManager<DdType::Sylvan>::execute(std::function<void()> const& f) const {
    // Only wake up the sylvan (i.e. lace) threads when they are suspended.
    std::exception_ptr e = nullptr;  // propagate exception
    if (suspended) {
        lace_resume();
        suspended = false;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
        RUN(execute_sylvan, &f, &e);
#pragma clang diagnostic pop
        lace_suspend();
        suspended = true;
    } else {
        // The sylvan threads are already running, don't suspend afterwards.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wused-but-marked-unused"
        RUN(execute_sylvan, &f, &e);
#pragma clang diagnostic pop
    }
    if (e) {
        std::rethrow_exception(e);
    }
}

uint_fast64_t InternalDdManager<DdType::Sylvan>::getNumberOfDdVariables() const {
    return nextFreeVariableIndex;
}
#else
InternalDdManager<DdType::Sylvan>::InternalDdManager(storm::SylvanDdManagerEnvironment const&) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalDdManager<DdType::Sylvan>::~InternalDdManager() {}

InternalBdd<DdType::Sylvan> InternalDdManager<DdType::Sylvan>::getBddOne() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalDdManager<DdType::Sylvan>::getAddOne() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalDdManager<DdType::Sylvan>::getBddZero() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

InternalBdd<DdType::Sylvan> InternalDdManager<DdType::Sylvan>::getBddEncodingLessOrEqualThan(uint64_t bound, InternalBdd<DdType::Sylvan> const& cube,
                                                                                             uint64_t numberOfDdVariables) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalDdManager<DdType::Sylvan>::getAddZero() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalDdManager<DdType::Sylvan>::getAddUndefined() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

template<typename ValueType>
InternalAdd<DdType::Sylvan, ValueType> InternalDdManager<DdType::Sylvan>::getConstant(ValueType const& value) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

std::vector<InternalBdd<DdType::Sylvan>> InternalDdManager<DdType::Sylvan>::createDdVariables(uint64_t numberOfLayers,
                                                                                              boost::optional<uint_fast64_t> const& position) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

bool InternalDdManager<DdType::Sylvan>::supportsOrderedInsertion() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

void InternalDdManager<DdType::Sylvan>::allowDynamicReordering(bool) {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

bool InternalDdManager<DdType::Sylvan>::isDynamicReorderingAllowed() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

void InternalDdManager<DdType::Sylvan>::triggerReordering() {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

void InternalDdManager<DdType::Sylvan>::debugCheck() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

void InternalDdManager<DdType::Sylvan>::execute(std::function<void()> const& f) const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}

uint_fast64_t InternalDdManager<DdType::Sylvan>::getNumberOfDdVariables() const {
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException,
                    "This version of Storm was compiled without support for Sylvan. Yet, a method was called that requires this support. Please choose a "
                    "version of Storm with Sylvan support.");
}
#endif

#ifndef STORM_HAVE_SYLVAN
// There is already an explicit template instantiations if Sylvan is available
template InternalAdd<DdType::Sylvan, double> InternalDdManager<DdType::Sylvan>::getAddOne() const;
template InternalAdd<DdType::Sylvan, uint_fast64_t> InternalDdManager<DdType::Sylvan>::getAddOne() const;
template InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalDdManager<DdType::Sylvan>::getAddOne() const;
template InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalDdManager<DdType::Sylvan>::getAddOne() const;
template InternalAdd<DdType::Sylvan, double> InternalDdManager<DdType::Sylvan>::getAddZero() const;
template InternalAdd<DdType::Sylvan, uint_fast64_t> InternalDdManager<DdType::Sylvan>::getAddZero() const;
template InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalDdManager<DdType::Sylvan>::getAddZero() const;
template InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalDdManager<DdType::Sylvan>::getAddZero() const;
template InternalAdd<DdType::Sylvan, double> InternalDdManager<DdType::Sylvan>::getConstant(double const& value) const;
template InternalAdd<DdType::Sylvan, uint_fast64_t> InternalDdManager<DdType::Sylvan>::getConstant(uint_fast64_t const& value) const;
template InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalDdManager<DdType::Sylvan>::getConstant(storm::RationalNumber const& value) const;
template InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalDdManager<DdType::Sylvan>::getConstant(storm::RationalFunction const& value) const;
#endif
template InternalAdd<DdType::Sylvan, double> InternalDdManager<DdType::Sylvan>::getAddUndefined() const;
template InternalAdd<DdType::Sylvan, uint_fast64_t> InternalDdManager<DdType::Sylvan>::getAddUndefined() const;
template InternalAdd<DdType::Sylvan, storm::RationalNumber> InternalDdManager<DdType::Sylvan>::getAddUndefined() const;
template InternalAdd<DdType::Sylvan, storm::RationalFunction> InternalDdManager<DdType::Sylvan>::getAddUndefined() const;

}  // namespace dd
}  // namespace storm
