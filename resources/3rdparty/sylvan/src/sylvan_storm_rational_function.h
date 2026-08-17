#ifndef SYLVAN_STORM_RATIONAL_FUNCTION_H
#define SYLVAN_STORM_RATIONAL_FUNCTION_H

#include <storm_wrapper.h>
#include <sylvan.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void sylvan_storm_rational_function_init();
uint32_t sylvan_storm_rational_function_get_type();
MTBDD mtbdd_storm_rational_function(storm_rational_function_ptr val);
storm_rational_function_ptr mtbdd_getstorm_rational_function_ptr(MTBDD terminal);

TASK_DECL_2(MTBDD, mtbdd_op_bool_to_storm_rational_function, MTBDD, size_t)
TASK_DECL_1(MTBDD, mtbdd_bool_to_storm_rational_function, MTBDD)
#define mtbdd_bool_to_storm_rational_function(dd) RUN(mtbdd_bool_to_storm_rational_function, dd)

TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_equals, MTBDD*, MTBDD*)
#define mtbdd_equals_rational_function(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_equals))

TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_plus, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_minus, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_times, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_divide, MTBDD*, MTBDD*)

TASK_DECL_3(MTBDD, sylvan_storm_rational_function_abstract_op_plus, MTBDD, MTBDD, int)
TASK_DECL_3(MTBDD, sylvan_storm_rational_function_abstract_op_times, MTBDD, MTBDD, int)
TASK_DECL_3(MTBDD, sylvan_storm_rational_function_abstract_op_min, MTBDD, MTBDD, int)
TASK_DECL_3(MTBDD, sylvan_storm_rational_function_abstract_op_max, MTBDD, MTBDD, int)

TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_less, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_less_or_equal, MTBDD*, MTBDD*)

TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_mod, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_pow, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_min, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_max, MTBDD*, MTBDD*)

TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_neg, MTBDD, size_t)
TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_floor, MTBDD, size_t)
TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_ceil, MTBDD, size_t)

#define sylvan_storm_rational_function_plus(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_plus))
#define sylvan_storm_rational_function_minus(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_minus))
#define sylvan_storm_rational_function_times(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_times))
#define sylvan_storm_rational_function_divide(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_divide))
#define sylvan_storm_rational_function_less(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_less))
#define sylvan_storm_rational_function_less_or_equal(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_less_or_equal))
#define sylvan_storm_rational_function_mod(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_less_or_equal))
#define sylvan_storm_rational_function_min(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_min))
#define sylvan_storm_rational_function_max(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_max))
#define sylvan_storm_rational_function_neg(a) mtbdd_uapply(a, TASK(sylvan_storm_rational_function_op_neg), 0)
#define sylvan_storm_rational_function_floor(a) mtbdd_uapply(a, TASK(sylvan_storm_rational_function_op_floor), 0)
#define sylvan_storm_rational_function_ceil(a) mtbdd_uapply(a, TASK(sylvan_storm_rational_function_op_ceil), 0)
#define sylvan_storm_rational_function_pow(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_function_op_pow))

TASK_DECL_1(MTBDD, sylvan_storm_rational_function_minimum, MTBDD);
#define sylvan_storm_rational_function_minimum(dd) RUN(sylvan_storm_rational_function_minimum, dd)

TASK_DECL_1(MTBDD, sylvan_storm_rational_function_maximum, MTBDD);
#define sylvan_storm_rational_function_maximum(dd) RUN(sylvan_storm_rational_function_maximum, dd)

TASK_DECL_3(MTBDD, sylvan_storm_rational_function_and_exists, MTBDD, MTBDD, MTBDD);
#define sylvan_storm_rational_function_and_exists(a, b, vars) RUN(sylvan_storm_rational_function_and_exists, a, b, vars)

#define sylvan_storm_rational_function_abstract_plus(dd, v) mtbdd_abstract(dd, v, TASK(sylvan_storm_rational_function_abstract_op_plus))
#define sylvan_storm_rational_function_abstract_min(dd, v) mtbdd_abstract(dd, v, TASK(sylvan_storm_rational_function_abstract_op_min))
#define sylvan_storm_rational_function_abstract_max(dd, v) mtbdd_abstract(dd, v, TASK(sylvan_storm_rational_function_abstract_op_max))

TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_to_double, MTBDD, size_t)
#define sylvan_storm_rational_function_to_double(a) mtbdd_uapply(a, TASK(sylvan_storm_rational_function_op_to_double), 0)

TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_threshold, MTBDD, size_t)
TASK_DECL_2(MTBDD, sylvan_storm_rational_function_op_strict_threshold, MTBDD, size_t)

TASK_DECL_2(MTBDD, sylvan_storm_rational_function_threshold, MTBDD, storm_rational_function_ptr);
#define sylvan_storm_rational_function_threshold(dd, value) RUN(sylvan_storm_rational_function_strict_threshold, dd, value)

TASK_DECL_2(MTBDD, sylvan_storm_rational_function_strict_threshold, MTBDD, storm_rational_function_ptr);
#define sylvan_storm_rational_function_strict_threshold(dd, value) RUN(sylvan_storm_rational_function_strict_threshold, dd, value)

TASK_DECL_3(MTBDD, sylvan_storm_rational_function_equal_norm_d, MTBDD, MTBDD, storm_rational_function_ptr);
#define sylvan_storm_rational_function_equal_norm_d(a, b, epsilon) RUN(sylvan_storm_rational_function_equal_norm_d, a, b, epsilon)

TASK_DECL_3(MTBDD, sylvan_storm_rational_function_equal_norm_rel_d, MTBDD, MTBDD, storm_rational_function_ptr);
#define sylvan_storm_rational_function_equal_norm_rel_d(a, b, epsilon) RUN(sylvan_storm_rational_function_equal_norm_rel_d, a, b, epsilon)

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif // SYLVAN_STORM_RATIONAL_FUNCTION_H
