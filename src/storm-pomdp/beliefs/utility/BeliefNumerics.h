#pragma once

namespace storm::pomdp::beliefs {

/**
 * Numeric predicates used when comparing and hashing beliefs.
 *
 * Floating-point specializations use the configured tolerance, while exact number types preserve exact semantics.
 */
template<typename ValueType>
struct BeliefNumerics {
    static bool lessOrEqual(ValueType const& lhs, ValueType const& rhs);
    static bool equal(ValueType const& lhs, ValueType const& rhs);
    static bool isZero(ValueType const& val);
    static bool isOne(ValueType const& val);

    /*!
     * @return a representative value that shall be used for hashing.
     */
    static ValueType valueForHash(ValueType const& val);
};

}  // namespace storm::pomdp::beliefs
