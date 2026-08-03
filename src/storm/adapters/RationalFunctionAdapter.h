#pragma once

#include "storm/adapters/RationalFunctionForward.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-negative"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wunused-template"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfree-nonheap-object"
#pragma GCC diagnostic ignored "-Wpessimizing-move"

#include <carl/core/FactorizedPolynomial.h>
#include <carl/core/MultivariatePolynomial.h>
#include <carl/core/RationalFunction.h>
#include <carl/core/Relation.h>
#include <carl/core/VariablePool.h>

#pragma GCC diagnostic pop
#pragma clang diagnostic pop

namespace carl {
// Define hash values for all polynomials and rational function.
// Needed for boost::hash_combine() and other functions
template<typename C, typename O, typename P>
inline size_t hash_value(carl::MultivariatePolynomial<C, O, P> const& p) {
    std::hash<carl::MultivariatePolynomial<C, O, P>> h;
    return h(p);
}

template<typename Pol>
inline size_t hash_value(carl::FactorizedPolynomial<Pol> const& p) {
    std::hash<carl::FactorizedPolynomial<Pol>> h;
    return h(p);
}

template<typename Pol, bool AutoSimplify>
inline size_t hash_value(carl::RationalFunction<Pol, AutoSimplify> const& f) {
    std::hash<carl::RationalFunction<Pol, AutoSimplify>> h;
    return h(f);
}
}  // namespace carl

namespace storm {
typedef carl::Cache<carl::PolynomialFactorizationPair<RawPolynomial>> RawPolynomialCache;
typedef carl::Relation CompareRelation;

RationalFunctionVariable createRFVariable(std::string const& name);

}  // namespace storm
