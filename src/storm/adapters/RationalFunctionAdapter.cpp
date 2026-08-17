#include "storm/adapters/RationalFunctionAdapter.h"

namespace storm {
RationalFunctionVariable createRFVariable(std::string const& name) {
    return carl::freshRealVariable(name);
}
}  // namespace storm

// Explicit instantiations
template class carl::MultivariatePolynomial<storm::RationalFunctionCoefficient>;
template class carl::FactorizedPolynomial<storm::RawPolynomial>;
template class carl::Cache<carl::PolynomialFactorizationPair<storm::RawPolynomial>>;
template class carl::RationalFunction<storm::Polynomial, true>;
