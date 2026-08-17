#pragma once

#include <carl/core/MultivariatePolynomialForward.h>

#include "storm-config.h"
#include "storm/adapters/RationalNumberForward.h"

namespace carl {

template<typename P>
class UnivariatePolynomial;

template<typename P>
class FactorizedPolynomial;

template<typename P>
class Cache;

template<typename P, bool as>
class RationalFunction;

}  // namespace carl

namespace storm {

typedef carl::Variable RationalFunctionVariable;

#if defined(STORM_HAVE_CLN) && defined(STORM_USE_CLN_RF)
typedef ClnRationalNumber RationalFunctionCoefficient;
#elif defined(STORM_HAVE_GMP) && !defined(STORM_USE_CLN_RF)
typedef GmpRationalNumber RationalFunctionCoefficient;
#elif defined(STORM_USE_CLN_RF)
#error CLN is to be used, but is not available.
#else
#error GMP is to be used, but is not available.
#endif

typedef carl::MultivariatePolynomial<RationalFunctionCoefficient> RawPolynomial;
typedef carl::UnivariatePolynomial<RationalFunctionCoefficient> RawUnivariatePolynomial;
typedef carl::FactorizedPolynomial<RawPolynomial> Polynomial;
typedef carl::RationalFunction<Polynomial, true> RationalFunction;

}  // namespace storm
