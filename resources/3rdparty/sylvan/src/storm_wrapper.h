#ifndef SYLVAN_STORM_WRAPPER_H
#define SYLVAN_STORM_WRAPPER_H

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

    /***************************************************
     Function-wrappers for storm::RationalNumber
     ****************************************************/

    typedef void* storm_rational_number_ptr;

    // Functions that are registered to sylvan for the rational number type.
    void storm_rational_number_init(storm_rational_number_ptr* a);
    void storm_rational_number_destroy(storm_rational_number_ptr a);
    int storm_rational_number_equals(storm_rational_number_ptr a, storm_rational_number_ptr b);
    char* storm_rational_number_to_str(storm_rational_number_ptr val, char *buf, size_t buflen);

    // Fundamental functions.
    storm_rational_number_ptr storm_rational_number_clone(storm_rational_number_ptr a);
    storm_rational_number_ptr storm_rational_number_get_zero();
    storm_rational_number_ptr storm_rational_number_get_one();
    storm_rational_number_ptr storm_rational_number_get_infinity();
    int storm_rational_number_is_zero(storm_rational_number_ptr a);
    uint64_t storm_rational_number_hash(storm_rational_number_ptr const a, uint64_t const seed);
    double storm_rational_number_get_value_double(storm_rational_number_ptr a);
    storm_rational_number_ptr storm_rational_number_from_double(double value);

    // Binary operations.
    storm_rational_number_ptr storm_rational_number_plus(storm_rational_number_ptr a, storm_rational_number_ptr b);
    storm_rational_number_ptr storm_rational_number_minus(storm_rational_number_ptr a, storm_rational_number_ptr b);
    storm_rational_number_ptr storm_rational_number_times(storm_rational_number_ptr a, storm_rational_number_ptr b);
    storm_rational_number_ptr storm_rational_number_divide(storm_rational_number_ptr a, storm_rational_number_ptr b);
    storm_rational_number_ptr storm_rational_number_pow(storm_rational_number_ptr a, storm_rational_number_ptr b);
    storm_rational_number_ptr storm_rational_number_mod(storm_rational_number_ptr a, storm_rational_number_ptr b);
    storm_rational_number_ptr storm_rational_number_min(storm_rational_number_ptr a, storm_rational_number_ptr b);
    storm_rational_number_ptr storm_rational_number_max(storm_rational_number_ptr a, storm_rational_number_ptr b);

    // Binary relations.
    int storm_rational_number_less(storm_rational_number_ptr a, storm_rational_number_ptr b);
    int storm_rational_number_less_or_equal(storm_rational_number_ptr a, storm_rational_number_ptr b);

    // Unary operations.
    storm_rational_number_ptr storm_rational_number_negate(storm_rational_number_ptr a);
    storm_rational_number_ptr storm_rational_number_floor(storm_rational_number_ptr a);
    storm_rational_number_ptr storm_rational_number_ceil(storm_rational_number_ptr a);

    storm_rational_number_ptr storm_double_sharpen(double value, size_t precision);
    storm_rational_number_ptr storm_rational_number_sharpen(storm_rational_number_ptr a, size_t precision);

    // Other operations.
    int storm_rational_number_equal_modulo_precision(int relative, storm_rational_number_ptr a, storm_rational_number_ptr b, storm_rational_number_ptr precision);

    // Printing functions.
    void print_storm_rational_number(storm_rational_number_ptr a);
    void print_storm_rational_number_to_file(storm_rational_number_ptr a, FILE* out);

    /***************************************************
     Function-wrappers for storm::RationalFunction
     ****************************************************/

    typedef void* storm_rational_function_ptr;

    // Functions that are registered to sylvan for the rational function type.
    void storm_rational_function_init(storm_rational_function_ptr* a);
    void storm_rational_function_destroy(storm_rational_function_ptr a);
    int storm_rational_function_equals(storm_rational_function_ptr a, storm_rational_function_ptr b);
    char* storm_rational_function_to_str(storm_rational_function_ptr val, char *buf, size_t buflen);

    // Fundamental functions.
    storm_rational_function_ptr storm_rational_function_clone(storm_rational_function_ptr a);
    storm_rational_function_ptr storm_rational_function_get_zero();
    storm_rational_function_ptr storm_rational_function_get_one();
    storm_rational_number_ptr storm_rational_function_get_infinity();
    int storm_rational_function_is_zero(storm_rational_function_ptr a);
    uint64_t storm_rational_function_hash(storm_rational_function_ptr const a, uint64_t const seed);
    double storm_rational_function_get_value_double(storm_rational_function_ptr a);

    // Binary operations.
    storm_rational_function_ptr storm_rational_function_plus(storm_rational_function_ptr a, storm_rational_function_ptr b);
    storm_rational_function_ptr storm_rational_function_minus(storm_rational_function_ptr a, storm_rational_function_ptr b);
    storm_rational_function_ptr storm_rational_function_times(storm_rational_function_ptr a, storm_rational_function_ptr b);
    storm_rational_function_ptr storm_rational_function_divide(storm_rational_function_ptr a, storm_rational_function_ptr b);
    storm_rational_function_ptr storm_rational_function_pow(storm_rational_function_ptr a, storm_rational_function_ptr b);
    storm_rational_function_ptr storm_rational_function_mod(storm_rational_function_ptr a, storm_rational_function_ptr b);
    storm_rational_function_ptr storm_rational_function_min(storm_rational_function_ptr a, storm_rational_function_ptr b);
    storm_rational_function_ptr storm_rational_function_max(storm_rational_function_ptr a, storm_rational_function_ptr b);

    // Binary relations.
    int storm_rational_function_less(storm_rational_function_ptr a, storm_rational_function_ptr b);
    int storm_rational_function_less_or_equal(storm_rational_function_ptr a, storm_rational_function_ptr b);

    // Unary operations.
    storm_rational_function_ptr storm_rational_function_negate(storm_rational_function_ptr a);
    storm_rational_function_ptr storm_rational_function_floor(storm_rational_function_ptr a);
    storm_rational_function_ptr storm_rational_function_ceil(storm_rational_function_ptr a);

    // Other operations.
    int storm_rational_function_equal_modulo_precision(int relative, storm_rational_function_ptr a, storm_rational_function_ptr b, storm_rational_function_ptr precision);

    // Printing functions.
    void print_storm_rational_function(storm_rational_function_ptr a);
    void print_storm_rational_function_to_file(storm_rational_function_ptr a, FILE* out);

#ifdef __cplusplus
}
#endif

#endif // SYLVAN_STORM_WRAPPER_H
