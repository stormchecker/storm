#include <stdint.h>
#include <math.h>
#include "sylvan_int.h"

#include "storm_wrapper.h"

// Import the types created for rational numbers and functions.
extern uint32_t srn_type;
extern uint32_t srf_type;

// Forward declare gcd here,
// as we don't want to mess with sylvans internal api too much
// Implemented in sylvan_mtbdd.c
uint32_t gcd(uint32_t, uint32_t);

/**
 * Apply a unary operation <op> to <dd>.
 */
TASK_IMPL_3(MTBDD, mtbdd_uapply_fail_false, MTBDD, dd, mtbdd_uapply_op, op, size_t, param)
{
    /* Maybe perform garbage collection */
    sylvan_gc_test();

    /* Count operation */
    sylvan_stats_count(MTBDD_UAPPLY);

    /* Check cache */
    MTBDD result;
    if (cache_get3(CACHE_MTBDD_UAPPLY, dd, (size_t)op, param, &result)) {
        sylvan_stats_count(MTBDD_UAPPLY_CACHED);
        return result;
    }

    /* Check terminal case */
    result = WRAP(op, dd, param);
    if (result != mtbdd_invalid) {
        /* Store in cache */
        if (cache_put3(CACHE_MTBDD_UAPPLY, dd, (size_t)op, param, result)) {
            sylvan_stats_count(MTBDD_UAPPLY_CACHEDPUT);
        }

        return result;
    }

    /* Get cofactors */
    mtbddnode_t ndd = MTBDD_GETNODE(dd);
    MTBDD ddlow = node_getlow(dd, ndd);
    MTBDD ddhigh = node_gethigh(dd, ndd);

    /* Recursive */
    mtbdd_refs_spawn(SPAWN(mtbdd_uapply, ddhigh, op, param));
    MTBDD low = mtbdd_refs_push(CALL(mtbdd_uapply, ddlow, op, param));
    MTBDD high = mtbdd_refs_sync(SYNC(mtbdd_uapply));
    mtbdd_refs_pop(1);

    if (low == mtbdd_false || high == mtbdd_false) {
        result = mtbdd_false;
    } else {
        result = mtbdd_makenode(mtbddnode_getvariable(ndd), low, high);
    }

    /* Store in cache */
    if (cache_put3(CACHE_MTBDD_UAPPLY, dd, (size_t)op, param, result)) {
        sylvan_stats_count(MTBDD_UAPPLY_CACHEDPUT);
    }

    return result;
}


/**
 * Binary operation Times (for MTBDDs of same type)
 * Only for MTBDDs where either all leaves are Integer or Double.
 * If either operand is mtbdd_false (not defined),
 * then the result is mtbdd_false (i.e. not defined).
 */
TASK_IMPL_2(MTBDD, mtbdd_op_divide, MTBDD*, pa, MTBDD*, pb)
{
    MTBDD a = *pa, b = *pb;
    if (a == mtbdd_false || b == mtbdd_false) return mtbdd_false;

    // Do not handle Boolean MTBDDs...

    mtbddnode_t na = MTBDD_GETNODE(a);
    mtbddnode_t nb = MTBDD_GETNODE(b);

    if (mtbddnode_isleaf(na) && mtbddnode_isleaf(nb)) {
        uint64_t val_a = mtbddnode_getvalue(na);
        uint64_t val_b = mtbddnode_getvalue(nb);
        if (mtbddnode_gettype(na) == 0 && mtbddnode_gettype(nb) == 0) {
            int64_t va = *(int64_t*)(&val_a);
            int64_t vb = *(int64_t*)(&val_b);

            if (va == 0) return a;
            else if (vb == 0) return b;
            else {
                MTBDD result;
                if (va == 1) result = b;
                else if (vb == 1) result = a;
                else result = mtbdd_int64(va*vb);
                return result;
            }
        } else if (mtbddnode_gettype(na) == 1 && mtbddnode_gettype(nb) == 1) {
            // both double
            double vval_a = *(double*)&val_a;
            double vval_b = *(double*)&val_b;
            if (vval_a == 0.0) return a;
            else if (vval_b == 0.0) {
                if (vval_a > 0.0) {
                    return mtbdd_double(INFINITY);
                } else {
                    return mtbdd_double(-INFINITY);
                }
                return b;
            } else {
                MTBDD result;
                if (vval_b == 1.0) result = a;
                result = mtbdd_double(vval_a / vval_b);
                return result;
            }
        }
        else if (mtbddnode_gettype(na) == 2 && mtbddnode_gettype(nb) == 2) {
            // both fraction
            uint64_t nom_a = val_a>>32;
            uint64_t nom_b = val_b>>32;
            uint64_t denom_a = val_a&0xffffffff;
            uint64_t denom_b = val_b&0xffffffff;
            // multiply!
            uint32_t c = gcd(denom_b, denom_a);
            uint32_t d = gcd(nom_a, nom_b);
            nom_a /= d;
            denom_a /= c;
            nom_a *= (denom_b/c);
            denom_a *= (nom_b/d);
            // compute result
            MTBDD result = mtbdd_fraction(nom_a, denom_a);
            return result;
        }
    }

    return mtbdd_invalid;
}

/**
 * Binary operation Equals (for MTBDDs of same type)
 * Only for MTBDDs where either all leaves are Boolean, or Integer, or Double.
 * For Integer/Double MTBDD, if either operand is mtbdd_false (not defined),
 * then the result is mtbdd_false (i.e. not defined).
 */
TASK_IMPL_2(MTBDD, mtbdd_op_equals, MTBDD*, pa, MTBDD*, pb)
{
    MTBDD a = *pa, b = *pb;
    if (a == mtbdd_false && b == mtbdd_false) return mtbdd_true;
    if (a == mtbdd_true && b == mtbdd_true) return mtbdd_true;

    mtbddnode_t na = MTBDD_GETNODE(a);
    mtbddnode_t nb = MTBDD_GETNODE(b);

    if (mtbddnode_isleaf(na) && mtbddnode_isleaf(nb)) {
        uint64_t val_a = mtbddnode_getvalue(na);
        uint64_t val_b = mtbddnode_getvalue(nb);
        if (mtbddnode_gettype(na) == 0 && mtbddnode_gettype(nb) == 0) {
            int64_t va = *(int64_t*)(&val_a);
            int64_t vb = *(int64_t*)(&val_b);
            if (va == vb) return mtbdd_true;
            return mtbdd_false;
        } else if (mtbddnode_gettype(na) == 1 && mtbddnode_gettype(nb) == 1) {
            // both double
            double vval_a = *(double*)&val_a;
            double vval_b = *(double*)&val_b;
            if (vval_a == vval_b) return mtbdd_true;
            return mtbdd_false;
        } else if (mtbddnode_gettype(na) == 2 && mtbddnode_gettype(nb) == 2) {
            // both fraction
            uint64_t nom_a = val_a>>32;
            uint64_t nom_b = val_b>>32;
            uint64_t denom_a = val_a&0xffffffff;
            uint64_t denom_b = val_b&0xffffffff;
            if (nom_a == nom_b && denom_a == denom_b) return mtbdd_true;
            return mtbdd_false;
        }
    }

    if (a < b) {
        *pa = b;
        *pb = a;
    }

    return mtbdd_invalid;
}

/**
 * Binary operation Equals (for MTBDDs of same type)
 * Only for MTBDDs where either all leaves are Boolean, or Integer, or Double.
 * For Integer/Double MTBDD, if either operand is mtbdd_false (not defined),
 * then the result is mtbdd_false (i.e. not defined).
 */
TASK_IMPL_2(MTBDD, mtbdd_op_less, MTBDD*, pa, MTBDD*, pb)
{
    MTBDD a = *pa, b = *pb;
    if (a == mtbdd_false && b == mtbdd_false) return mtbdd_true;
    if (a == mtbdd_true && b == mtbdd_true) return mtbdd_true;

    mtbddnode_t na = MTBDD_GETNODE(a);
    mtbddnode_t nb = MTBDD_GETNODE(b);

    if (mtbddnode_isleaf(na) && mtbddnode_isleaf(nb)) {
        uint64_t val_a = mtbddnode_getvalue(na);
        uint64_t val_b = mtbddnode_getvalue(nb);
        if (mtbddnode_gettype(na) == 0 && mtbddnode_gettype(nb) == 0) {
            int64_t va = *(int64_t*)(&val_a);
            int64_t vb = *(int64_t*)(&val_b);
            if (va < vb) return mtbdd_true;
            return mtbdd_false;
        } else if (mtbddnode_gettype(na) == 1 && mtbddnode_gettype(nb) == 1) {
            // both double
            double vval_a = *(double*)&val_a;
            double vval_b = *(double*)&val_b;
            if (vval_a < vval_b) return mtbdd_true;
            return mtbdd_false;
        } else if (mtbddnode_gettype(na) == 2 && mtbddnode_gettype(nb) == 2) {
            // both fraction
            uint64_t nom_a = val_a>>32;
            uint64_t nom_b = val_b>>32;
            uint64_t denom_a = val_a&0xffffffff;
            uint64_t denom_b = val_b&0xffffffff;
            return nom_a * denom_b < nom_b * denom_a ? mtbdd_true : mtbdd_false;
        }
    }

    return mtbdd_invalid;
}

/**
 * Binary operation Less or Equals (for MTBDDs of same type)
 * Only for MTBDDs where either all leaves are Boolean, or Integer, or Double.
 * For Integer/Double MTBDD, if either operand is mtbdd_false (not defined),
 * then the result is mtbdd_false (i.e. not defined).
 */
TASK_IMPL_2(MTBDD, mtbdd_op_less_or_equal, MTBDD*, pa, MTBDD*, pb)
{
    MTBDD a = *pa, b = *pb;
    if (a == mtbdd_false && b == mtbdd_false) return mtbdd_true;
    if (a == mtbdd_true && b == mtbdd_true) return mtbdd_true;

    mtbddnode_t na = MTBDD_GETNODE(a);
    mtbddnode_t nb = MTBDD_GETNODE(b);

    if (mtbddnode_isleaf(na) && mtbddnode_isleaf(nb)) {
        uint64_t val_a = mtbddnode_getvalue(na);
        uint64_t val_b = mtbddnode_getvalue(nb);
        if (mtbddnode_gettype(na) == 0 && mtbddnode_gettype(nb) == 0) {
            int64_t va = *(int64_t*)(&val_a);
            int64_t vb = *(int64_t*)(&val_b);
            return va <= vb ? mtbdd_true : mtbdd_false;
        } else if (mtbddnode_gettype(na) == 1 && mtbddnode_gettype(nb) == 1) {
            // both double
            double vval_a = *(double*)&val_a;
            double vval_b = *(double*)&val_b;
            if (vval_a <= vval_b) return mtbdd_true;
            return mtbdd_false;
        } else if (mtbddnode_gettype(na) == 2 && mtbddnode_gettype(nb) == 2) {
            // both fraction
            uint64_t nom_a = val_a>>32;
            uint64_t nom_b = val_b>>32;
            uint64_t denom_a = val_a&0xffffffff;
            uint64_t denom_b = val_b&0xffffffff;
            nom_a *= denom_b;
            nom_b *= denom_a;
            return nom_a <= nom_b ? mtbdd_true : mtbdd_false;
        }
    }

    return mtbdd_invalid;
}

/**
 * Binary operation Greater or Equals (for MTBDDs of same type)
 * Only for MTBDDs where either all leaves are Boolean, or Integer, or Double.
 * For Integer/Double MTBDD, if either operand is mtbdd_false (not defined),
 * then the result is mtbdd_false (i.e. not defined).
 */
TASK_IMPL_2(MTBDD, mtbdd_op_greater_or_equal, MTBDD*, pa, MTBDD*, pb)
{
    MTBDD a = *pa, b = *pb;
    if (a == mtbdd_false && b == mtbdd_false) return mtbdd_true;
    if (a == mtbdd_true && b == mtbdd_true) return mtbdd_true;

    mtbddnode_t na = MTBDD_GETNODE(a);
    mtbddnode_t nb = MTBDD_GETNODE(b);

    if (mtbddnode_isleaf(na) && mtbddnode_isleaf(nb)) {
        uint64_t val_a = mtbddnode_getvalue(na);
        uint64_t val_b = mtbddnode_getvalue(nb);
        if (mtbddnode_gettype(na) == 0 && mtbddnode_gettype(nb) == 0) {
            int64_t va = *(int64_t*)(&val_a);
            int64_t vb = *(int64_t*)(&val_b);
            return va >= vb ? mtbdd_true : mtbdd_false;
        } else if (mtbddnode_gettype(na) == 1 && mtbddnode_gettype(nb) == 1) {
            // both double
            double vval_a = *(double*)&val_a;
            double vval_b = *(double*)&val_b;
            if (vval_a >= vval_b) return mtbdd_true;
            return mtbdd_false;
        } else if (mtbddnode_gettype(na) == 2 && mtbddnode_gettype(nb) == 2) {
            // both fraction
            uint64_t nom_a = val_a>>32;
            uint64_t nom_b = val_b>>32;
            uint64_t denom_a = val_a&0xffffffff;
            uint64_t denom_b = val_b&0xffffffff;
            nom_a *= denom_b;
            nom_b *= denom_a;
            return nom_a >= nom_b ? mtbdd_true : mtbdd_false;
        }
    }

    return mtbdd_invalid;
}

/**
 * Binary operation Pow (for MTBDDs of same type)
 * Only for MTBDDs where either all leaves are Double.
 * For Integer/Double MTBDD, if either operand is mtbdd_false (not defined),
 * then the result is mtbdd_false (i.e. not defined).
 */
TASK_IMPL_2(MTBDD, mtbdd_op_pow, MTBDD*, pa, MTBDD*, pb)
{
    MTBDD a = *pa, b = *pb;

    mtbddnode_t na = MTBDD_GETNODE(a);
    mtbddnode_t nb = MTBDD_GETNODE(b);

    if (mtbddnode_isleaf(na) && mtbddnode_isleaf(nb)) {
        uint64_t val_a = mtbddnode_getvalue(na);
        uint64_t val_b = mtbddnode_getvalue(nb);
        if (mtbddnode_gettype(na) == 0 && mtbddnode_gettype(nb) == 0) {
            assert(0);
        } else if (mtbddnode_gettype(na) == 1 && mtbddnode_gettype(nb) == 1) {
            // both double
            double vval_a = *(double*)&val_a;
            double vval_b = *(double*)&val_b;
            return mtbdd_double(pow(vval_a, vval_b));
        } else if (mtbddnode_gettype(na) == 2 && mtbddnode_gettype(nb) == 2) {
            assert(0);
        }
    }

    return mtbdd_invalid;
}

/**
 * Binary operation Mod (for MTBDDs of same type)
 * Only for MTBDDs where either all leaves are Double.
 * For Integer/Double MTBDD, if either operand is mtbdd_false (not defined),
 * then the result is mtbdd_false (i.e. not defined).
 */
TASK_IMPL_2(MTBDD, mtbdd_op_mod, MTBDD*, pa, MTBDD*, pb)
{
    MTBDD a = *pa, b = *pb;

    mtbddnode_t na = MTBDD_GETNODE(a);
    mtbddnode_t nb = MTBDD_GETNODE(b);

    if (mtbddnode_isleaf(na) && mtbddnode_isleaf(nb)) {
        uint64_t val_a = mtbddnode_getvalue(na);
        uint64_t val_b = mtbddnode_getvalue(nb);
        if (mtbddnode_gettype(na) == 0 && mtbddnode_gettype(nb) == 0) {
            assert(0);
        } else if (mtbddnode_gettype(na) == 1 && mtbddnode_gettype(nb) == 1) {
            // both double
            double vval_a = *(double*)&val_a;
            double vval_b = *(double*)&val_b;
            return mtbdd_double(fmod(vval_a, vval_b));
        } else if (mtbddnode_gettype(na) == 2 && mtbddnode_gettype(nb) == 2) {
            assert(0);
        }
    }

    return mtbdd_invalid;
}

/**
 * Binary operation Log (for MTBDDs of same type)
 * Only for MTBDDs where either all leaves are Double.
 * For Integer/Double MTBDD, if either operand is mtbdd_false (not defined),
 * then the result is mtbdd_false (i.e. not defined).
 */
TASK_IMPL_2(MTBDD, mtbdd_op_logxy, MTBDD*, pa, MTBDD*, pb)
{
    MTBDD a = *pa, b = *pb;

    mtbddnode_t na = MTBDD_GETNODE(a);
    mtbddnode_t nb = MTBDD_GETNODE(b);

    if (mtbddnode_isleaf(na) && mtbddnode_isleaf(nb)) {
        uint64_t val_a = mtbddnode_getvalue(na);
        uint64_t val_b = mtbddnode_getvalue(nb);
        if (mtbddnode_gettype(na) == 0 && mtbddnode_gettype(nb) == 0) {
            assert(0);
        } else if (mtbddnode_gettype(na) == 1 && mtbddnode_gettype(nb) == 1) {
            // both double
            double vval_a = *(double*)&val_a;
            double vval_b = *(double*)&val_b;
            return mtbdd_double(log(vval_a) / log(vval_b));
        } else if (mtbddnode_gettype(na) == 2 && mtbddnode_gettype(nb) == 2) {
            assert(0);
        }
    }

    return mtbdd_invalid;
}

TASK_IMPL_2(MTBDD, mtbdd_op_not_zero, MTBDD, a, size_t, v)
{
    /* We only expect "double" terminals, or false */
    if (a == mtbdd_false) return mtbdd_false;
    if (a == mtbdd_true) return mtbdd_true;

    // a != constant
    mtbddnode_t na = MTBDD_GETNODE(a);

    if (mtbddnode_isleaf(na)) {
        if (mtbddnode_gettype(na) == 0) {
            return mtbdd_getint64(a) != 0 ? mtbdd_true : mtbdd_false;
        } else if (mtbddnode_gettype(na) == 1) {
            return mtbdd_getdouble(a) != 0.0 ? mtbdd_true : mtbdd_false;
        } else if (mtbddnode_gettype(na) == 2) {
            return mtbdd_getnumer(a) != 0 ? mtbdd_true : mtbdd_false;
        } else if (mtbddnode_gettype(na) == srn_type) {
            return storm_rational_number_is_zero((storm_rational_number_ptr)mtbdd_getvalue(a)) == 0 ? mtbdd_true : mtbdd_false;
        } else if (mtbddnode_gettype(na) == srf_type) {
            return storm_rational_function_is_zero((storm_rational_function_ptr)mtbdd_getvalue(a)) == 0 ? mtbdd_true : mtbdd_false;
        }
    }

    // Ugly hack to get rid of the error "unused variable v" (because there is no version of uapply without a parameter).
    (void)v;

    return mtbdd_invalid;
}

TASK_IMPL_1(MTBDD, mtbdd_not_zero, MTBDD, dd)
{
    return mtbdd_uapply(dd, TASK(mtbdd_op_not_zero), 0);
}

TASK_IMPL_2(MTBDD, mtbdd_op_floor, MTBDD, a, size_t, v)
{
    /* We only expect "double" terminals, or false */
    if (a == mtbdd_false) return mtbdd_false;
    if (a == mtbdd_true) return mtbdd_true;

    // a != constant
    mtbddnode_t na = MTBDD_GETNODE(a);

    if (mtbddnode_isleaf(na)) {
        if (mtbddnode_gettype(na) == 0) {
            return a;
        } else if (mtbddnode_gettype(na) == 1) {
            MTBDD result = mtbdd_double(floor(mtbdd_getdouble(a)));
            return result;
        } else if (mtbddnode_gettype(na) == 2) {
            MTBDD result = mtbdd_fraction(mtbdd_getnumer(a) / mtbdd_getdenom(a), 1);
            return result;
        }
    }

    // Ugly hack to get rid of the error "unused variable v" (because there is no version of uapply without a parameter).
    (void)v;

    return mtbdd_invalid;
}

TASK_IMPL_1(MTBDD, mtbdd_floor, MTBDD, dd)
{
    return mtbdd_uapply(dd, TASK(mtbdd_op_floor), 0);
}

TASK_IMPL_2(MTBDD, mtbdd_op_ceil, MTBDD, a, size_t, v)
{
    /* We only expect "double" terminals, or false */
    if (a == mtbdd_false) return mtbdd_false;
    if (a == mtbdd_true) return mtbdd_true;

    // a != constant
    mtbddnode_t na = MTBDD_GETNODE(a);

    if (mtbddnode_isleaf(na)) {
        if (mtbddnode_gettype(na) == 0) {
            return a;
        } else if (mtbddnode_gettype(na) == 1) {
            MTBDD result = mtbdd_double(ceil(mtbdd_getdouble(a)));
            return result;
        } else if (mtbddnode_gettype(na) == 2) {
            MTBDD result = mtbdd_fraction(mtbdd_getnumer(a) / mtbdd_getdenom(a) + 1, 1);
            return result;
        }
    }

    // Ugly hack to get rid of the error "unused variable v" (because there is no version of uapply without a parameter).
    (void)v;

    return mtbdd_invalid;
}

TASK_IMPL_1(MTBDD, mtbdd_ceil, MTBDD, dd)
{
    return mtbdd_uapply(dd, TASK(mtbdd_op_ceil), 0);
}

TASK_IMPL_2(MTBDD, mtbdd_op_bool_to_double, MTBDD, a, size_t, v)
{
    /* We only expect "double" terminals, or false */
    if (a == mtbdd_false) return mtbdd_double(0);
    if (a == mtbdd_true) return mtbdd_double(1.0);

    // Ugly hack to get rid of the error "unused variable v" (because there is no version of uapply without a parameter).
    (void)v;

    return mtbdd_invalid;
}

TASK_IMPL_1(MTBDD, mtbdd_bool_to_double, MTBDD, dd)
{
    return mtbdd_uapply(dd, TASK(mtbdd_op_bool_to_double), 0);
}

TASK_IMPL_2(MTBDD, mtbdd_op_bool_to_int64, MTBDD, a, size_t, v)
{
    /* We only expect "double" terminals, or false */
    if (a == mtbdd_false) return mtbdd_int64(0);
    if (a == mtbdd_true) return mtbdd_int64(1);

    // Ugly hack to get rid of the error "unused variable v" (because there is no version of uapply without a parameter).
    (void)v;

    return mtbdd_invalid;
}

TASK_IMPL_1(MTBDD, mtbdd_bool_to_int64, MTBDD, dd)
{
    return mtbdd_uapply(dd, TASK(mtbdd_op_bool_to_int64), 0);
}

/**
 * Calculate the number of satisfying variable assignments according to <variables>.
 */
TASK_IMPL_2(double, mtbdd_non_zero_count, MTBDD, dd, size_t, nvars)
{
    /* Trivial cases */
    if (dd == mtbdd_false) return 0.0;

    mtbddnode_t na = MTBDD_GETNODE(dd);

    if (mtbdd_isleaf(dd)) {
        if (mtbddnode_gettype(na) == 0) {
            return mtbdd_getint64(dd) != 0 ? powl(2.0L, nvars) : 0.0;
        } else if (mtbddnode_gettype(na) == 1) {
            return mtbdd_getdouble(dd) != 0 ? powl(2.0L, nvars) : 0.0;
        } else if (mtbddnode_gettype(na) == 2) {
            return mtbdd_getnumer(dd) != 0 ? powl(2.0L, nvars) : 0.0;
        } else if (mtbddnode_gettype(na) == srn_type) {
            return storm_rational_number_is_zero((storm_rational_number_ptr)mtbdd_getvalue(dd)) == 0 ? powl(2.0L, nvars) : 0.0;
        } else if (mtbddnode_gettype(na) == srf_type) {
            return storm_rational_function_is_zero((storm_rational_function_ptr)mtbdd_getvalue(dd)) == 0 ? powl(2.0L, nvars) : 0.0;
        }
    }

    /* Perhaps execute garbage collection */
    sylvan_gc_test();

    union {
        double d;
        uint64_t s;
    } hack;

    /* Consult cache */
    if (cache_get3(CACHE_MTBDD_NONZERO_COUNT, dd, 0, nvars, &hack.s)) {
        sylvan_stats_count(CACHE_MTBDD_NONZERO_COUNT);
        return hack.d;
    }

    SPAWN(mtbdd_non_zero_count, mtbdd_gethigh(dd), nvars-1);
    double low = CALL(mtbdd_non_zero_count, mtbdd_getlow(dd), nvars-1);
    hack.d = low + SYNC(mtbdd_non_zero_count);

    cache_put3(CACHE_MTBDD_NONZERO_COUNT, dd, 0, nvars, hack.s);
    return hack.d;
}

int mtbdd_iszero(MTBDD dd) {
    if (mtbdd_gettype(dd) == 0) {
        return mtbdd_getint64(dd) == 0;
    } else if (mtbdd_gettype(dd) == 1) {
        return mtbdd_getdouble(dd) == 0;
    } else if (mtbdd_gettype(dd) == 2) {
        return mtbdd_getnumer(dd) == 0;
    } else if (mtbdd_gettype(dd) == srn_type) {
        return storm_rational_number_is_zero((storm_rational_number_ptr)mtbdd_getvalue(dd)) == 1 ? 1 : 0;
    } else if (mtbdd_gettype(dd) == srf_type) {
        return storm_rational_function_is_zero((storm_rational_function_ptr)mtbdd_getvalue(dd)) == 1 ? 1 : 0;
    }
    return 0;
}

int mtbdd_isnonzero(MTBDD dd) {
    return mtbdd_iszero(dd) ? 0 : 1;
}

TASK_IMPL_2(MTBDD, mtbdd_op_complement, MTBDD, a, size_t, k)
{
    // if a is false, then it is a partial function. Keep partial!
    if (a == mtbdd_false) return mtbdd_false;

    // a != constant
    mtbddnode_t na = MTBDD_GETNODE(a);

    if (mtbddnode_isleaf(na)) {
        if (mtbddnode_gettype(na) == 0) {
            int64_t v = mtbdd_getint64(a);
			if (v == 0) {
				return mtbdd_int64(1);
			} else {
				return mtbdd_int64(0);
			}
        } else if (mtbddnode_gettype(na) == 1) {
            double d = mtbdd_getdouble(a);
			if (d == 0.0) {
				return mtbdd_double(1.0);
			} else {
				return mtbdd_double(0.0);
			}
        } else if (mtbddnode_gettype(na) == 2) {
            printf("ERROR: mtbdd_op_complement type FRACTION.\n");
			assert(0);
        }
    }

    return mtbdd_invalid;
    (void)k; // unused variable
}

#include "sylvan_storm_rational_number.h"

TASK_IMPL_2(MTBDD, mtbdd_op_sharpen, MTBDD, a, size_t, p)
{
    /* We only expect double or rational number terminals, or false */
    if (a == mtbdd_false) return mtbdd_false;
    if (a == mtbdd_true) return mtbdd_true;

    // a != constant
    mtbddnode_t na = MTBDD_GETNODE(a);

    if (mtbddnode_isleaf(na)) {
        if (mtbddnode_gettype(na) == 1) {
            storm_rational_number_ptr rnp = storm_double_sharpen(mtbdd_getdouble(a), p);

            // If the sharpening failed, we return mtbdd_false so this can be detected at the top level.
            if (rnp == (storm_rational_number_ptr)0) {
                return mtbdd_false;
            }
            MTBDD result = mtbdd_storm_rational_number(rnp);
            return result;
        } else if (mtbddnode_gettype(na) == srn_type) {
            return mtbdd_storm_rational_number(storm_rational_number_sharpen((storm_rational_number_ptr)mtbdd_getstorm_rational_number_ptr(a), p));
        } else {
            printf("ERROR: Unsupported value type in sharpen.\n");
            assert(0);
        }
    }

    return mtbdd_invalid;
}

TASK_IMPL_2(MTBDD, mtbdd_sharpen, MTBDD, dd, size_t, p)
{
    return mtbdd_uapply_fail_false(dd, TASK(mtbdd_op_sharpen), p);
}

TASK_IMPL_2(MTBDD, mtbdd_op_to_rational_number, MTBDD, a, size_t, p)
{
    /* We only expect double or rational number terminals, or false */
    if (a == mtbdd_false) return mtbdd_false;
    if (a == mtbdd_true) return mtbdd_true;

    // a != constant
    mtbddnode_t na = MTBDD_GETNODE(a);

    if (mtbddnode_isleaf(na)) {
        if (mtbddnode_gettype(na) == 1) {
            MTBDD result = mtbdd_storm_rational_number(storm_rational_number_from_double(mtbdd_getdouble(a)));
            return result;
        } else {
            printf("ERROR: Unsupported value type in conversion to rational number.\n");
            assert(0);
        }
    }

    return mtbdd_invalid;
    (void)p; // unused variable
}

TASK_IMPL_2(MTBDD, mtbdd_to_rational_number, MTBDD, dd, size_t, p)
{
    return mtbdd_uapply(dd, TASK(mtbdd_op_to_rational_number), 0);
    (void)p; // unused variable
}


TASK_IMPL_3(BDD, mtbdd_min_abstract_representative, MTBDD, a, BDD, v, BDDVAR, prev_level) {
	/* Maybe perform garbage collection */
    sylvan_gc_test();

	if (sylvan_set_isempty(v)) {
		return sylvan_true;
	}

	/* Cube is guaranteed to be a cube at this point. */
    if (mtbdd_isleaf(a)) {
		BDD _v = sylvan_set_next(v);
		BDD res = CALL(mtbdd_min_abstract_representative, a, _v, prev_level);
		if (res == sylvan_invalid) {
			return sylvan_invalid;
		}
		sylvan_ref(res);

		BDD res1 = sylvan_not(sylvan_ite(sylvan_ithvar(bddnode_getvariable(MTBDD_GETNODE(v))), sylvan_true, sylvan_not(res)));
		if (res1 == sylvan_invalid) {
			sylvan_deref(res);
			return sylvan_invalid;
		}
		sylvan_deref(res);
		return res1;
    }

    /* Check cache */
    MTBDD result;
    if (cache_get3(CACHE_MTBDD_ABSTRACT_REPRESENTATIVE, a, v, (size_t)1, &result)) {
        sylvan_stats_count(MTBDD_ABSTRACT_CACHED);
        return result;
    }

	mtbddnode_t na = MTBDD_GETNODE(a);
	uint32_t va = mtbddnode_getvariable(na);
	bddnode_t nv = MTBDD_GETNODE(v);
	BDDVAR vv = bddnode_getvariable(nv);

    /* Abstract a variable that does not appear in a. */
    if (va > vv) {
		BDD _v = sylvan_set_next(v);
        BDD res = CALL(mtbdd_min_abstract_representative, a, _v, va);
        if (res == sylvan_invalid) {
            return sylvan_invalid;
        }

        // Fill in the missing variables to make representative unique.
        sylvan_ref(res);
        BDD res1 = sylvan_ite(sylvan_ithvar(vv), sylvan_false, res);
        if (res1 == sylvan_invalid) {
            sylvan_deref(res);
            return sylvan_invalid;
        }
        sylvan_deref(res);
       	return res1;
    }

    MTBDD E = mtbdd_getlow(a);
    MTBDD T = mtbdd_gethigh(a);

    /* If the two indices are the same, so are their levels. */
    if (va == vv) {
		BDD _v = sylvan_set_next(v);
        BDD res1 = CALL(mtbdd_min_abstract_representative, E, _v, va);
        if (res1 == sylvan_invalid) {
            return sylvan_invalid;
        }
        sylvan_ref(res1);

        BDD res2 = CALL(mtbdd_min_abstract_representative, T, _v, va);
        if (res2 == sylvan_invalid) {
            sylvan_deref(res1);
            return sylvan_invalid;
        }
        sylvan_ref(res2);

        MTBDD left = mtbdd_abstract_min(E, _v);
        if (left == mtbdd_invalid) {
            sylvan_deref(res1);
			sylvan_deref(res2);
            return sylvan_invalid;
        }
        mtbdd_ref(left);

        MTBDD right = mtbdd_abstract_min(T, _v);
        if (right == mtbdd_invalid) {
            sylvan_deref(res1);
			sylvan_deref(res2);
			mtbdd_deref(left);
            return sylvan_invalid;
        }
        mtbdd_ref(right);

        BDD tmp = mtbdd_less_or_equal_as_bdd(left, right);
        if (tmp == sylvan_invalid) {
            sylvan_deref(res1);
			sylvan_deref(res2);
			mtbdd_deref(left);
			mtbdd_deref(right);
            return sylvan_invalid;
        }
        sylvan_ref(tmp);

        mtbdd_deref(left);
		mtbdd_deref(right);

        BDD res1Inf = sylvan_ite(tmp, res1, sylvan_false);
        if (res1Inf == sylvan_invalid) {
            sylvan_deref(res1);
			sylvan_deref(res2);
			sylvan_deref(tmp);
            return sylvan_invalid;
        }
        sylvan_ref(res1Inf);
        sylvan_deref(res1);

        BDD res2Inf = sylvan_ite(tmp, sylvan_false, res2);
        if (res2Inf == sylvan_invalid) {
            sylvan_deref(res2);
            sylvan_deref(res1Inf);
            sylvan_deref(tmp);
            return sylvan_invalid;
        }
        sylvan_ref(res2Inf);
        sylvan_deref(res2);
        sylvan_deref(tmp);

        BDD res = (res1Inf == res2Inf) ? sylvan_ite(sylvan_ithvar(va), sylvan_false, res1Inf) : sylvan_ite(sylvan_ithvar(va), res2Inf, res1Inf);

        if (res == sylvan_invalid) {
            sylvan_deref(res1Inf);
            sylvan_deref(res2Inf);
            return sylvan_invalid;
        }
        sylvan_ref(res);
        sylvan_deref(res1Inf);
        sylvan_deref(res2Inf);

        /* Store in cache */
        if (cache_put3(CACHE_MTBDD_ABSTRACT_REPRESENTATIVE, a, v, (size_t)1, res)) {
            sylvan_stats_count(MTBDD_ABSTRACT_CACHEDPUT);
        }

        sylvan_deref(res);
        return res;
    }
    else { /* if (va < vv) */
		BDD res1 = CALL(mtbdd_min_abstract_representative, E, v, va);
        if (res1 == sylvan_invalid) {
			return sylvan_invalid;
		}
        sylvan_ref(res1);
        BDD res2 = CALL(mtbdd_min_abstract_representative, T, v, va);
        if (res2 == sylvan_invalid) {
            sylvan_deref(res1);
            return sylvan_invalid;
        }
        sylvan_ref(res2);

        BDD res = (res1 == res2) ? res1 : sylvan_ite(sylvan_ithvar(va), res2, res1);
        if (res == sylvan_invalid) {
            sylvan_deref(res1);
            sylvan_deref(res2);
            return sylvan_invalid;
        }
        sylvan_deref(res1);
        sylvan_deref(res2);

        /* Store in cache */
        if (cache_put3(CACHE_MTBDD_ABSTRACT_REPRESENTATIVE, a, v, (size_t)1, res)) {
            sylvan_stats_count(MTBDD_ABSTRACT_CACHEDPUT);
        }
        return res;
    }
}

TASK_IMPL_3(BDD, mtbdd_max_abstract_representative, MTBDD, a, MTBDD, v, uint32_t, prev_level) {
	/* Maybe perform garbage collection */
    sylvan_gc_test();

	if (sylvan_set_isempty(v)) {
		return sylvan_true;
	}

    /* Count operation */
    sylvan_stats_count(MTBDD_ABSTRACT);

	/* Cube is guaranteed to be a cube at this point. */
    if (mtbdd_isleaf(a)) {
        /* Compute result */
		BDD _v = sylvan_set_next(v);
		BDD res = CALL(mtbdd_max_abstract_representative, a, _v, prev_level);
		if (res == sylvan_invalid) {
			return sylvan_invalid;
		}
		sylvan_ref(res);

		BDD res1 = sylvan_not(sylvan_ite(sylvan_ithvar(bddnode_getvariable(MTBDD_GETNODE(v))), sylvan_true, sylvan_not(res)));
		if (res1 == sylvan_invalid) {
			sylvan_deref(res);
			return sylvan_invalid;
		}
		sylvan_deref(res);

		return res1;
    }

    /* Check cache */
    MTBDD result;
    if (cache_get3(CACHE_MTBDD_ABSTRACT_REPRESENTATIVE, a, v, (size_t)0, &result)) {
        sylvan_stats_count(MTBDD_ABSTRACT_CACHED);
        return result;
    }

	mtbddnode_t na = MTBDD_GETNODE(a);
	uint32_t va = mtbddnode_getvariable(na);
	bddnode_t nv = MTBDD_GETNODE(v);
	BDDVAR vv = bddnode_getvariable(nv);

    /* Abstract a variable that does not appear in a. */
    if (vv < va) {
		BDD _v = sylvan_set_next(v);
        BDD res = CALL(mtbdd_max_abstract_representative, a, _v, va);
        if (res == sylvan_invalid) {
            return sylvan_invalid;
        }

        // Fill in the missing variables to make representative unique.
        sylvan_ref(res);
        BDD res1 = sylvan_ite(sylvan_ithvar(vv), sylvan_false, res);
        if (res1 == sylvan_invalid) {
            sylvan_deref(res);
            return sylvan_invalid;
        }
        sylvan_deref(res);
       	return res1;
    }

    MTBDD E = mtbdd_getlow(a);
    MTBDD T = mtbdd_gethigh(a);

    /* If the two indices are the same, so are their levels. */
    if (va == vv) {
		BDD _v = sylvan_set_next(v);
        BDD res1 = CALL(mtbdd_max_abstract_representative, E, _v, va);
        if (res1 == sylvan_invalid) {
            return sylvan_invalid;
        }
        sylvan_ref(res1);

        BDD res2 = CALL(mtbdd_max_abstract_representative, T, _v, va);
        if (res2 == sylvan_invalid) {
            sylvan_deref(res1);
            return sylvan_invalid;
        }
        sylvan_ref(res2);

        MTBDD left = mtbdd_abstract_max(E, _v);
        if (left == mtbdd_invalid) {
            sylvan_deref(res1);
			sylvan_deref(res2);
            return sylvan_invalid;
        }
        mtbdd_ref(left);

        MTBDD right = mtbdd_abstract_max(T, _v);
        if (right == mtbdd_invalid) {
            sylvan_deref(res1);
			sylvan_deref(res2);
			mtbdd_deref(left);
            return sylvan_invalid;
        }
        mtbdd_ref(right);

        BDD tmp = mtbdd_greater_or_equal_as_bdd(left, right);
        if (tmp == sylvan_invalid) {
            sylvan_deref(res1);
			sylvan_deref(res2);
			mtbdd_deref(left);
			mtbdd_deref(right);
            return sylvan_invalid;
        }
        sylvan_ref(tmp);

        mtbdd_deref(left);
		mtbdd_deref(right);

        BDD res1Inf = sylvan_ite(tmp, res1, sylvan_false);
        if (res1Inf == sylvan_invalid) {
            sylvan_deref(res1);
			sylvan_deref(res2);
			sylvan_deref(tmp);
            return sylvan_invalid;
        }
        sylvan_ref(res1Inf);
        sylvan_deref(res1);

        BDD res2Inf = sylvan_ite(tmp, sylvan_false, res2);
        if (res2Inf == sylvan_invalid) {
            sylvan_deref(res2);
            sylvan_deref(res1Inf);
            sylvan_deref(tmp);
            return sylvan_invalid;
        }
        sylvan_ref(res2Inf);
        sylvan_deref(res2);
        sylvan_deref(tmp);

        BDD res = (res1Inf == res2Inf) ? sylvan_ite(sylvan_ithvar(va), sylvan_false, res1Inf) : sylvan_ite(sylvan_ithvar(va), res2Inf, res1Inf);

        if (res == sylvan_invalid) {
            sylvan_deref(res1Inf);
            sylvan_deref(res2Inf);
            return sylvan_invalid;
        }
        sylvan_ref(res);
        sylvan_deref(res1Inf);
        sylvan_deref(res2Inf);

        /* Store in cache */
        if (cache_put3(CACHE_MTBDD_ABSTRACT_REPRESENTATIVE, a, v, (size_t)0, res)) {
            sylvan_stats_count(MTBDD_ABSTRACT_CACHEDPUT);
        }

        sylvan_deref(res);
        return res;
    }
    else { /* if (va < vv) */
		BDD res1 = CALL(mtbdd_max_abstract_representative, E, v, va);
        if (res1 == sylvan_invalid) {
			return sylvan_invalid;
		}
        sylvan_ref(res1);
        BDD res2 = CALL(mtbdd_max_abstract_representative, T, v, va);
        if (res2 == sylvan_invalid) {
            sylvan_deref(res1);
            return sylvan_invalid;
        }
        sylvan_ref(res2);

        BDD res = (res1 == res2) ? res1 : sylvan_ite(sylvan_ithvar(va), res2, res1);
        if (res == sylvan_invalid) {
            sylvan_deref(res1);
            sylvan_deref(res2);
            return sylvan_invalid;
        }
        sylvan_deref(res1);
        sylvan_deref(res2);

        /* Store in cache */
        if (cache_put3(CACHE_MTBDD_ABSTRACT_REPRESENTATIVE, a, v, (size_t)0, res)) {
            sylvan_stats_count(MTBDD_ABSTRACT_CACHEDPUT);
        }

        return res;
    }
}

TASK_IMPL_3(MTBDD, mtbdd_uapply_nocache, MTBDD, dd, mtbdd_uapply_op, op, size_t, param)
{
    /* Maybe perform garbage collection */
    sylvan_gc_test();

    /* Check cache */
    MTBDD result;

    // Caching would be done here, but is omitted (as this is the purpose of this function).

    /* Check terminal case */
    result = WRAP(op, dd, param);
    if (result != mtbdd_invalid) {
        // Caching would be done here, but is omitted (as this is the purpose of this function).
        return result;
    }

    /* Get cofactors */
    mtbddnode_t ndd = MTBDD_GETNODE(dd);
    MTBDD ddlow = node_getlow(dd, ndd);
    MTBDD ddhigh = node_gethigh(dd, ndd);

    /* Recursive */
    mtbdd_refs_spawn(SPAWN(mtbdd_uapply_nocache, ddhigh, op, param));
    MTBDD low = mtbdd_refs_push(CALL(mtbdd_uapply_nocache, ddlow, op, param));
    MTBDD high = mtbdd_refs_sync(SYNC(mtbdd_uapply_nocache));
    mtbdd_refs_pop(1);
    result = mtbdd_makenode(mtbddnode_getvariable(ndd), low, high);

    // Caching would be done here, but is omitted (as this is the purpose of this function).
    return result;
}
