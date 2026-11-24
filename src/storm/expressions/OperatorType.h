#pragma once

#include <iosfwd>

namespace storm {
namespace expressions {
// An enum representing all possible operator types.
enum class OperatorType {
    And,
    Or,
    Xor,
    Implies,
    Iff,
    Plus,
    Minus,
    Times,
    Divide,
    Min,
    Max,
    Power,
    Modulo,
    Logarithm,
    Cos,
    Sin,
    Equal,
    NotEqual,
    Less,
    LessOrEqual,
    Greater,
    GreaterOrEqual,
    Not,
    Floor,
    Ceil,
    Ite,
    AtLeastOneOf,
    AtMostOneOf,
    ExactlyOneOf
};

std::ostream& operator<<(std::ostream& stream, OperatorType const& operatorType);
}  // namespace expressions
}  // namespace storm
