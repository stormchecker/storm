#include "storm_wrapper.h"

#include <cstring>
#include <iostream>
#include <sstream>
#include <set>
#include <map>
#include <mutex>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/numbers/constants.h"
#include "storm/numbers/KwekMehlhorn.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/PrecisionExceededException.h"

#include <sylvan.h>

#if defined(STORM_HAVE_GMP) && !defined(STORM_USE_CLN_EA)
#define RATIONAL_NUMBER_THREAD_SAFE
#else
#warning "Rational numbers do not appear to be thread-safe. Use in sylvan will be protected by mutexes, performance might degrade."
#endif

// A mutex that is used to lock all operations accessing rational numbers as they are not necessarily thread-safe.
#ifndef RATIONAL_NUMBER_THREAD_SAFE
std::mutex rationalNumberMutex;
#endif

// A mutex that is used to lock all operations accessing rational functions as carl is not necessarily thread-safe.
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
std::mutex rationalFunctionMutex;
#endif

/***************************************************
 Function-wrappers for storm::RationalNumber
 ****************************************************/

void storm_rational_number_init(storm_rational_number_ptr* a) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm_rational_number_ptr srn_ptr = new storm::RationalNumber(*(static_cast<storm::RationalNumber*>(*a)));
    *a = srn_ptr;
}

void storm_rational_number_destroy(storm_rational_number_ptr a) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber* srn_ptr = static_cast<storm::RationalNumber*>(a);
    delete srn_ptr;
}

int storm_rational_number_equals(storm_rational_number_ptr a, storm_rational_number_ptr b) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber*>(a);
    storm::RationalNumber const& srn_b = *static_cast<storm::RationalNumber*>(b);

    return (srn_a == srn_b) ? 1 : 0;
}

char* storm_rational_number_to_str(storm_rational_number_ptr val, char *buf, size_t buflen) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    std::stringstream ss;
    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber*>(val);
    ss << srn_a;
    std::string s = ss.str();
    if (s.size() + 1 < buflen) {
        std::memcpy(buf, s.c_str(), s.size() + 1);
        return buf;
    } else {
        char* result = static_cast<char*>(malloc(s.size() + 1));
        std::memcpy(result, s.c_str(), s.size() + 1);
        return result;
    }
}

storm_rational_number_ptr storm_rational_number_clone(storm_rational_number_ptr a) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber* result_srn = new storm::RationalNumber(*static_cast<storm::RationalNumber const*>(a));
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_rational_number_get_zero() {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber* result_srn = new storm::RationalNumber(storm::numbers::zero<storm::RationalNumber>());
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_rational_number_get_one() {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber* result_srn = new storm::RationalNumber(storm::numbers::one<storm::RationalNumber>());
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_rational_number_get_infinity() {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber* result_srn = new storm::RationalNumber(storm::numbers::infinity<storm::RationalNumber>());
    return static_cast<storm_rational_number_ptr>(result_srn);
}

int storm_rational_number_is_zero(storm_rational_number_ptr a) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    return storm::numbers::isZero(*static_cast<storm::RationalNumber const*>(a)) ? 1 : 0;
}

uint64_t storm_rational_number_hash(storm_rational_number_ptr const a, uint64_t const seed) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);

    // Taken from boost::hash_combine that we do not call here for the lack of boost headers.
    return seed ^ (std::hash<storm::RationalNumber>()(srn_a) + 0x9e3779b9 + (seed<<6) + (seed>>2));
}

double storm_rational_number_get_value_double(storm_rational_number_ptr a) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    return storm::numbers::convert<double>(srn_a);
}

storm_rational_number_ptr storm_rational_number_from_double(double value) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber* number = new storm::RationalNumber(storm::numbers::convert<storm::RationalNumber>(value));
    return number;
}

storm_rational_number_ptr storm_rational_number_plus(storm_rational_number_ptr a, storm_rational_number_ptr b) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber const& srn_b = *static_cast<storm::RationalNumber const*>(b);

    storm::RationalNumber* result_srn = new storm::RationalNumber(srn_a + srn_b);
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_rational_number_minus(storm_rational_number_ptr a, storm_rational_number_ptr b) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber const& srn_b = *static_cast<storm::RationalNumber const*>(b);

    storm::RationalNumber* result_srn = new storm::RationalNumber(srn_a - srn_b);
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_rational_number_times(storm_rational_number_ptr a, storm_rational_number_ptr b) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber const& srn_b = *static_cast<storm::RationalNumber const*>(b);

    storm::RationalNumber* result_srn = new storm::RationalNumber(srn_a * srn_b);
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_rational_number_divide(storm_rational_number_ptr a, storm_rational_number_ptr b) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber const& srn_b = *static_cast<storm::RationalNumber const*>(b);

    storm::RationalNumber* result_srn = new storm::RationalNumber(srn_a / srn_b);
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_rational_number_pow(storm_rational_number_ptr a, storm_rational_number_ptr b) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber const& srn_b = *static_cast<storm::RationalNumber const*>(b);

    carl::sint exponentAsInteger = carl::toInt<carl::sint>(srn_b);
    storm::RationalNumber* result_srn = new storm::RationalNumber(storm::numbers::pow(srn_a, exponentAsInteger));
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_rational_number_mod(storm_rational_number_ptr a, storm_rational_number_ptr b) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber const& srn_b = *static_cast<storm::RationalNumber const*>(b);
    if (carl::isInteger(srn_a) && carl::isInteger(srn_b)) {
        storm::RationalNumber* result_srn = new storm::RationalNumber(carl::mod(carl::getNum(srn_a), carl::getNum(srn_b)));
        return static_cast<storm_rational_number_ptr>(result_srn);
    }
    throw storm::exceptions::InvalidOperationException() << "Modulo not supported for rational, non-integer numbers.";
}

storm_rational_number_ptr storm_rational_number_min(storm_rational_number_ptr a, storm_rational_number_ptr b) {
    return storm_rational_number_less_or_equal(a, b) ? storm_rational_number_clone(a) : storm_rational_number_clone(b);
}

storm_rational_number_ptr storm_rational_number_max(storm_rational_number_ptr a, storm_rational_number_ptr b) {
    return storm_rational_number_less(a, b) ? storm_rational_number_clone(b) : storm_rational_number_clone(a);
}

int storm_rational_number_less(storm_rational_number_ptr a, storm_rational_number_ptr b) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber const& srn_b = *static_cast<storm::RationalNumber const*>(b);

    if (storm::numbers::isInfinity<storm::RationalNumber>(srn_b)) {
        return storm::numbers::isInfinity<storm::RationalNumber>(srn_a) ? 0 : 1;
    } else if (storm::numbers::isInfinity<storm::RationalNumber>(srn_a)) {
        return 0;
    }

    return srn_a < srn_b ? 1 : 0;
}

int storm_rational_number_less_or_equal(storm_rational_number_ptr a, storm_rational_number_ptr b) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber const& srn_b = *static_cast<storm::RationalNumber const*>(b);

    if (storm::numbers::isInfinity<storm::RationalNumber>(srn_b)) {
        return 1;
    } else if (storm::numbers::isInfinity<storm::RationalNumber>(srn_a)) {
        return 0;
    }

    return srn_a <= srn_b ? 1 : 0;
}

storm_rational_number_ptr storm_rational_number_negate(storm_rational_number_ptr a) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber* result_srn = new storm::RationalNumber(-srn_a);
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_rational_number_floor(storm_rational_number_ptr a) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber* result_srn = new storm::RationalNumber(carl::floor(srn_a));
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_rational_number_ceil(storm_rational_number_ptr a) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber* result_srn = new storm::RationalNumber(carl::ceil(srn_a));
    return static_cast<storm_rational_number_ptr>(result_srn);
}

storm_rational_number_ptr storm_double_sharpen(double value, size_t precision) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    try {
        storm::RationalNumber tmp = storm::numbers::kwek_mehlhorn::sharpen<storm::RationalNumber, double>(precision, value);
        storm::RationalNumber* result_srn = new storm::RationalNumber(tmp);
        return static_cast<storm_rational_number_ptr>(result_srn);
    } catch (storm::exceptions::PrecisionExceededException const& e) {
        return nullptr;
    }
}

storm_rational_number_ptr storm_rational_number_sharpen(storm_rational_number_ptr a, size_t precision) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber tmp = storm::numbers::kwek_mehlhorn::sharpen<storm::RationalNumber, storm::RationalNumber>(precision, srn_a);
    storm::RationalNumber* result_srn = new storm::RationalNumber(tmp);
    return static_cast<storm_rational_number_ptr>(result_srn);
}

int storm_rational_number_equal_modulo_precision(int relative, storm_rational_number_ptr a, storm_rational_number_ptr b, storm_rational_number_ptr precision) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    storm::RationalNumber const& srn_b = *static_cast<storm::RationalNumber const*>(b);
    storm::RationalNumber const& srn_p = *static_cast<storm::RationalNumber const*>(precision);

    if (relative) {
        if (storm::numbers::isZero<storm::RationalNumber>(srn_a)) {
            return storm::numbers::isZero<storm::RationalNumber>(srn_b);
        } else {
            return carl::abs(srn_a - srn_b)/srn_a < srn_p ? 1 : 0;
        }
    } else {
        return carl::abs(srn_a - srn_b) < srn_p ? 1 : 0;
    }
}

void print_storm_rational_number(storm_rational_number_ptr a) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    std::cout << srn_a << std::flush;
}

void print_storm_rational_number_to_file(storm_rational_number_ptr a, FILE* out) {
#ifndef RATIONAL_NUMBER_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalNumberMutex);
#endif

    std::stringstream ss;
    storm::RationalNumber const& srn_a = *static_cast<storm::RationalNumber const*>(a);
    ss << srn_a;
    std::string s = ss.str();
    fprintf(out, "%s", s.c_str());
}

/***************************************************
 Function-wrappers for storm::RationalFunction
 ****************************************************/

void storm_rational_function_init(storm_rational_function_ptr* a) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm_rational_function_ptr srf_ptr = new storm::RationalFunction(*static_cast<storm::RationalFunction*>(*a));
    *a = srf_ptr;
}

void storm_rational_function_destroy(storm_rational_function_ptr a) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction* srf = static_cast<storm::RationalFunction*>(a);
    delete srf;
}

int storm_rational_function_equals(storm_rational_function_ptr a, storm_rational_function_ptr b) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction*>(a);
    storm::RationalFunction const& srf_b = *static_cast<storm::RationalFunction*>(b);

    return (srf_a == srf_b) ? 1 : 0;
}

char* storm_rational_function_to_str(storm_rational_function_ptr val, char* buf, size_t buflen) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    std::stringstream ss;
    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction*>(val);
    ss << srf_a;
    std::string s = ss.str();
    if (s.size() + 1 < buflen) {
        std::memcpy(buf, s.c_str(), s.size() + 1);
        return buf;
    } else {
        char* result = static_cast<char*>(malloc(s.size() + 1));
        std::memcpy(result, s.c_str(), s.size() + 1);
        return result;
    }
}

storm_rational_function_ptr storm_rational_function_clone(storm_rational_function_ptr a) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction* result_srf = new storm::RationalFunction(*static_cast<storm::RationalFunction const*>(a));
    return static_cast<storm_rational_function_ptr>(result_srf);
}

storm_rational_function_ptr storm_rational_function_get_zero() {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction* result_srf = new storm::RationalFunction(storm::numbers::zero<storm::RationalFunction>());
    return static_cast<storm_rational_function_ptr>(result_srf);
}

storm_rational_function_ptr storm_rational_function_get_one() {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction* result_srf = new storm::RationalFunction(storm::numbers::one<storm::RationalFunction>());
    return static_cast<storm_rational_function_ptr>(result_srf);
}

storm_rational_function_ptr storm_rational_function_get_infinity() {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction* result_srf = new storm::RationalFunction(storm::numbers::infinity<storm::RationalFunction>());
    return static_cast<storm_rational_function_ptr>(result_srf);
}


int storm_rational_function_is_zero(storm_rational_function_ptr a) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    return storm::numbers::isZero(*static_cast<storm::RationalFunction const*>(a)) ? 1 : 0;
}

uint64_t storm_rational_function_hash(storm_rational_function_ptr const a, uint64_t const seed) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);

    // Taken from boost::hash_combine that we do not call here for the lack of boost headers.
    return seed ^ (std::hash<storm::RationalFunction>()(srf_a) + 0x9e3779b9 + (seed<<6) + (seed>>2));
}

double storm_rational_function_get_value_double(storm_rational_function_ptr a) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    if (srf_a.isConstant()) {
        return storm::numbers::convert<double>(srf_a);
    } else {
        throw storm::exceptions::InvalidOperationException() << "Cannot evaluate rational function as it's not constant.";
    }
}

storm_rational_function_ptr storm_rational_function_plus(storm_rational_function_ptr a, storm_rational_function_ptr b) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    storm::RationalFunction const& srf_b = *static_cast<storm::RationalFunction const*>(b);

    storm::RationalFunction* result_srf = new storm::RationalFunction(srf_a);
    *result_srf += srf_b;
    return static_cast<storm_rational_function_ptr>(result_srf);
}

storm_rational_function_ptr storm_rational_function_minus(storm_rational_function_ptr a, storm_rational_function_ptr b) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    storm::RationalFunction const& srf_b = *static_cast<storm::RationalFunction const*>(b);

    storm::RationalFunction* result_srf = new storm::RationalFunction(srf_a);
    *result_srf -= srf_b;
    return static_cast<storm_rational_function_ptr>(result_srf);
}

storm_rational_function_ptr storm_rational_function_times(storm_rational_function_ptr a, storm_rational_function_ptr b) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    storm::RationalFunction const& srf_b = *static_cast<storm::RationalFunction const*>(b);

    storm::RationalFunction* result_srf = new storm::RationalFunction(srf_a);
    *result_srf *= srf_b;
    return static_cast<storm_rational_function_ptr>(result_srf);
}

storm_rational_function_ptr storm_rational_function_divide(storm_rational_function_ptr a, storm_rational_function_ptr b) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    storm::RationalFunction const& srf_b = *static_cast<storm::RationalFunction const*>(b);

    storm::RationalFunction* result_srf = new storm::RationalFunction(srf_a);
    *result_srf /= srf_b;
    return static_cast<storm_rational_function_ptr>(result_srf);
}

storm_rational_function_ptr storm_rational_function_pow(storm_rational_function_ptr a, storm_rational_function_ptr b) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    storm::RationalFunction const& srf_b = *static_cast<storm::RationalFunction const*>(b);

    carl::uint exponentAsInteger = carl::toInt<carl::uint>(srf_b.nominatorAsNumber());
    storm::RationalFunction* result_srf = new storm::RationalFunction(carl::pow(srf_a, exponentAsInteger));
    return static_cast<storm_rational_function_ptr>(result_srf);
}

storm_rational_function_ptr storm_rational_function_mod(storm_rational_function_ptr a, storm_rational_function_ptr b) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    storm::RationalFunction const& srf_b = *static_cast<storm::RationalFunction const*>(b);

    if (!storm::numbers::isConstant(srf_a) || !storm::numbers::isConstant(srf_b)) {
        throw storm::exceptions::InvalidOperationException() << "Operands of mod must not be non-constant rational functions.";
    }
    throw storm::exceptions::InvalidOperationException() << "Modulo not supported for rational functions.";
}

storm_rational_function_ptr storm_rational_function_min(storm_rational_function_ptr a, storm_rational_function_ptr b) {
    return storm_rational_function_less_or_equal(a, b) ? storm_rational_function_clone(a) : storm_rational_function_clone(b);
}

storm_rational_function_ptr storm_rational_function_max(storm_rational_function_ptr a, storm_rational_function_ptr b) {
    return storm_rational_function_less(a, b) ? storm_rational_function_clone(b) : storm_rational_function_clone(a);
}

int storm_rational_function_less(storm_rational_function_ptr a, storm_rational_function_ptr b) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    storm::RationalFunction const& srf_b = *static_cast<storm::RationalFunction const*>(b);
    if (!storm::numbers::isConstant(srf_a) || !storm::numbers::isConstant(srf_b)) {
        throw storm::exceptions::InvalidOperationException() << "Operands of less must not be non-constant rational functions.";
    }

    storm::RationalFunctionCoefficient srn_a = storm::numbers::convert<storm::RationalFunctionCoefficient>(srf_a);
    storm::RationalFunctionCoefficient srn_b = storm::numbers::convert<storm::RationalFunctionCoefficient>(srf_b);

    if (storm::numbers::isInfinity<storm::RationalFunctionCoefficient>(srn_b)) {
        return storm::numbers::isInfinity<storm::RationalFunctionCoefficient>(srn_a) ? 0 : 1;
    } else if (storm::numbers::isInfinity<storm::RationalFunctionCoefficient>(srn_a)) {
        return 0;
    }

    return (srn_a < srn_b) ? 1 : 0;
}

int storm_rational_function_less_or_equal(storm_rational_function_ptr a, storm_rational_function_ptr b) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    storm::RationalFunction const& srf_b = *static_cast<storm::RationalFunction const*>(b);
    if (!storm::numbers::isConstant(srf_a) || !storm::numbers::isConstant(srf_b)) {
        throw storm::exceptions::InvalidOperationException() << "Operands of less-or-equal must not be non-constant rational functions.";
    }

    storm::RationalFunctionCoefficient srn_a = storm::numbers::convert<storm::RationalFunctionCoefficient>(srf_a);
    storm::RationalFunctionCoefficient srn_b = storm::numbers::convert<storm::RationalFunctionCoefficient>(srf_b);

    if (storm::numbers::isInfinity<storm::RationalFunctionCoefficient>(srn_b)) {
        return 1;
    } else if (storm::numbers::isInfinity<storm::RationalFunctionCoefficient>(srn_a)) {
        return 0;
    }

    return (srn_a <= srn_b) ? 1 : 0;
}

storm_rational_function_ptr storm_rational_function_negate(storm_rational_function_ptr a) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    storm::RationalFunction* result_srf = new storm::RationalFunction(-srf_a);
    return static_cast<storm_rational_function_ptr>(result_srf);
}

storm_rational_function_ptr storm_rational_function_floor(storm_rational_function_ptr a) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    if (!storm::numbers::isConstant(srf_a)) {
        throw storm::exceptions::InvalidOperationException() << "Operand of floor must not be non-constant rational function.";
    }
    storm::RationalFunction* result_srf = new storm::RationalFunction(carl::floor(storm::numbers::convert<storm::RationalFunctionCoefficient>(srf_a)));
    return static_cast<storm_rational_function_ptr>(result_srf);
}

storm_rational_function_ptr storm_rational_function_ceil(storm_rational_function_ptr a) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    if (!storm::numbers::isConstant(srf_a)) {
        throw storm::exceptions::InvalidOperationException() << "Operand of ceil must not be non-constant rational function.";
    }
    storm::RationalFunction* result_srf = new storm::RationalFunction(carl::ceil(storm::numbers::convert<storm::RationalFunctionCoefficient>(srf_a)));
    return static_cast<storm_rational_function_ptr>(result_srf);
}

int storm_rational_function_equal_modulo_precision(int relative, storm_rational_function_ptr a, storm_rational_function_ptr b, storm_rational_function_ptr precision) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    storm::RationalFunction const& srf_b = *static_cast<storm::RationalFunction const*>(b);
    storm::RationalFunction const& srf_p = *static_cast<storm::RationalFunction const*>(precision);

    if (!storm::numbers::isConstant(srf_a) || !storm::numbers::isConstant(srf_b) || !storm::numbers::isConstant(srf_p)) {
        throw storm::exceptions::InvalidOperationException() << "Operands of equal-modulo-precision must not be non-constant rational functions.";
    }

    storm::RationalFunctionCoefficient srn_a = storm::numbers::convert<storm::RationalFunctionCoefficient>(srf_a);
    storm::RationalFunctionCoefficient srn_b = storm::numbers::convert<storm::RationalFunctionCoefficient>(srf_b);
    storm::RationalFunctionCoefficient srn_p = storm::numbers::convert<storm::RationalFunctionCoefficient>(srf_p);

    if (relative) {
        return carl::abs(srn_a - srn_b)/srn_a < srn_p ? 1 : 0;
    } else {
        return carl::abs(srn_a - srn_b) < srn_p ? 1 : 0;
    }
}

void print_storm_rational_function(storm_rational_function_ptr a) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    std::cout << srf_a << std::flush;
}

void print_storm_rational_function_to_file(storm_rational_function_ptr a, FILE* out) {
#ifndef RATIONAL_FUNCTION_THREAD_SAFE
    std::lock_guard<std::mutex> lock(rationalFunctionMutex);
#endif

    std::stringstream ss;
    storm::RationalFunction const& srf_a = *static_cast<storm::RationalFunction const*>(a);
    ss << srf_a;
    std::string s = ss.str();
    fprintf(out, "%s", s.c_str());
}
