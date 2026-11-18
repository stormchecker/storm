#pragma once

/*!
 * Lists of (backend, value type) combinations for explicit template instantiation.
 *
 * Use it to explicitly instantiate all Dd libraries via macro:
 *     #define STORM_DD_INSTANTIATE(DD, ValueType) template class Dtmc<storm::dd::DdType::DD, ValueType>;
 *     STORM_DD_FOR_EACH_STANDARD(STORM_DD_INSTANTIATE)
 *     #undef STORM_DD_INSTANTIATE
 *
 * A new backend is added to the lists in this file.
 * CheckBackends.cpp verifies that every pair listed here is supported in BackendTraits.
 */

/// Every registered backend, without a value type: X(DD).
#define STORM_DD_FOR_EACH_BACKEND(X) X(CUDD) X(Sylvan)

/// Backend/value-type pairs for all standard value types.
#define STORM_DD_FOR_EACH_STANDARD(X) X(CUDD, double) X(Sylvan, double) X(Sylvan, storm::RationalNumber) X(Sylvan, storm::RationalFunction)

/// Like STORM_DD_FOR_EACH_STANDARD, but without storm::RationalFunction.
#define STORM_DD_FOR_EACH_NON_PARAMETRIC(X) X(CUDD, double) X(Sylvan, double) X(Sylvan, storm::RationalNumber)
