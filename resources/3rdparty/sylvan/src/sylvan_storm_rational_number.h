#ifndef SYLVAN_STORM_RATIONAL_NUMBER_H
#define SYLVAN_STORM_RATIONAL_NUMBER_H

#include <storm_wrapper.h>
#include <sylvan.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void sylvan_storm_rational_number_init();
uint32_t sylvan_storm_rational_number_get_type();
MTBDD mtbdd_storm_rational_number(storm_rational_number_ptr val);
storm_rational_number_ptr mtbdd_getstorm_rational_number_ptr(MTBDD terminal);

TASK_DECL_2(MTBDD, mtbdd_op_bool_to_storm_rational_number, MTBDD, size_t)
TASK_DECL_1(MTBDD, mtbdd_bool_to_storm_rational_number, MTBDD)
#define mtbdd_bool_to_storm_rational_number(dd) RUN(mtbdd_bool_to_storm_rational_number, dd)

TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_equals, MTBDD*, MTBDD*)
#define mtbdd_equals_rational_number(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_equals))

TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_plus, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_minus, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_times, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_divide, MTBDD*, MTBDD*)

TASK_DECL_3(MTBDD, sylvan_storm_rational_number_abstract_op_plus, MTBDD, MTBDD, int)
TASK_DECL_3(MTBDD, sylvan_storm_rational_number_abstract_op_times, MTBDD, MTBDD, int)
TASK_DECL_3(MTBDD, sylvan_storm_rational_number_abstract_op_min, MTBDD, MTBDD, int)
TASK_DECL_3(MTBDD, sylvan_storm_rational_number_abstract_op_max, MTBDD, MTBDD, int)

TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_less, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_greater, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_less_or_equal, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_greater_or_equal, MTBDD*, MTBDD*)

TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_mod, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_pow, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_min, MTBDD*, MTBDD*)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_max, MTBDD*, MTBDD*)

TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_neg, MTBDD, size_t)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_floor, MTBDD, size_t)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_ceil, MTBDD, size_t)

#define sylvan_storm_rational_number_plus(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_plus))
#define sylvan_storm_rational_number_minus(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_minus))
#define sylvan_storm_rational_number_times(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_times))
#define sylvan_storm_rational_number_divide(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_divide))
#define sylvan_storm_rational_number_less(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_less))
#define sylvan_storm_rational_number_greater(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_greater))
#define sylvan_storm_rational_number_less_or_equal(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_less_or_equal))
#define sylvan_storm_rational_number_greater_or_equal(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_greater_or_equal))
#define sylvan_storm_rational_number_mod(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_mod))
#define sylvan_storm_rational_number_min(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_min))
#define sylvan_storm_rational_number_max(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_max))
#define sylvan_storm_rational_number_neg(a) mtbdd_uapply(a, TASK(sylvan_storm_rational_number_op_neg), 0)
#define sylvan_storm_rational_number_floor(a) mtbdd_uapply(a, TASK(sylvan_storm_rational_number_op_floor), 0)
#define sylvan_storm_rational_number_ceil(a) mtbdd_uapply(a, TASK(sylvan_storm_rational_number_op_ceil), 0)
#define sylvan_storm_rational_number_pow(a, b) mtbdd_apply(a, b, TASK(sylvan_storm_rational_number_op_pow))

TASK_DECL_1(MTBDD, sylvan_storm_rational_number_minimum, MTBDD);
#define sylvan_storm_rational_number_minimum(dd) RUN(sylvan_storm_rational_number_minimum, dd)

TASK_DECL_1(MTBDD, sylvan_storm_rational_number_maximum, MTBDD);
#define sylvan_storm_rational_number_maximum(dd) RUN(sylvan_storm_rational_number_maximum, dd)

TASK_DECL_3(MTBDD, sylvan_storm_rational_number_and_exists, MTBDD, MTBDD, MTBDD);
#define sylvan_storm_rational_number_and_exists(a, b, vars) RUN(sylvan_storm_rational_number_and_exists, a, b, vars)

#define sylvan_storm_rational_number_abstract_plus(dd, v) mtbdd_abstract(dd, v, TASK(sylvan_storm_rational_number_abstract_op_plus))
#define sylvan_storm_rational_number_abstract_min(dd, v) mtbdd_abstract(dd, v, TASK(sylvan_storm_rational_number_abstract_op_min))
#define sylvan_storm_rational_number_abstract_max(dd, v) mtbdd_abstract(dd, v, TASK(sylvan_storm_rational_number_abstract_op_max))

TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_to_double, MTBDD, size_t)
#define sylvan_storm_rational_number_to_double(a) mtbdd_uapply(a, TASK(sylvan_storm_rational_number_op_to_double), 0)

TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_threshold, MTBDD, size_t)
TASK_DECL_2(MTBDD, sylvan_storm_rational_number_op_strict_threshold, MTBDD, size_t)
#define sylvan_storm_rational_number_threshold(dd, value) mtbdd_uapply_nocache(dd, TASK(sylvan_storm_rational_number_op_threshold), (size_t)(void*)value)
#define sylvan_storm_rational_number_strict_threshold(dd, value) mtbdd_uapply_nocache(dd, TASK(sylvan_storm_rational_number_op_strict_threshold), (size_t)(void*)value)

TASK_DECL_3(MTBDD, sylvan_storm_rational_number_equal_norm_d, MTBDD, MTBDD, storm_rational_number_ptr);
#define sylvan_storm_rational_number_equal_norm_d(a, b, epsilon) RUN(sylvan_storm_rational_number_equal_norm_d, a, b, epsilon)

TASK_DECL_3(MTBDD, sylvan_storm_rational_number_equal_norm_rel_d, MTBDD, MTBDD, storm_rational_number_ptr);
#define sylvan_storm_rational_number_equal_norm_rel_d(a, b, epsilon) RUN(sylvan_storm_rational_number_equal_norm_rel_d, a, b, epsilon)

TASK_DECL_3(BDD, sylvan_storm_rational_number_min_abstract_representative, MTBDD, MTBDD, uint32_t);
#define sylvan_storm_rational_number_min_abstract_representative(a, vars) (RUN(sylvan_storm_rational_number_min_abstract_representative, a, vars, 0))

TASK_DECL_3(BDD, sylvan_storm_rational_number_max_abstract_representative, MTBDD, MTBDD, uint32_t);
#define sylvan_storm_rational_number_max_abstract_representative(a, vars) (RUN(sylvan_storm_rational_number_max_abstract_representative, a, vars, 0))

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif // SYLVAN_STORM_RATIONAL_NUMBER_H
