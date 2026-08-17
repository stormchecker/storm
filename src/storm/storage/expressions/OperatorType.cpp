#include "storm/storage/expressions/OperatorType.h"
#include <ostream>

namespace storm {
namespace expressions {
std::ostream& operator<<(std::ostream& stream, OperatorType const& operatorType) {
    switch (operatorType) {
        case OperatorType::And:
            stream << "&";
            break;
        case OperatorType::Or:
            stream << "|";
            break;
        case OperatorType::Xor:
            stream << "!=";
            break;
        case OperatorType::Implies:
            stream << "=>";
            break;
        case OperatorType::Iff:
            stream << "<=>";
            break;
        case OperatorType::Plus:
            stream << "+";
            break;
        case OperatorType::Minus:
            stream << "-";
            break;
        case OperatorType::Times:
            stream << "*";
            break;
        case OperatorType::Divide:
            stream << "/";
            break;
        case OperatorType::Min:
            stream << "min";
            break;
        case OperatorType::Max:
            stream << "max";
            break;
        case OperatorType::Power:
            stream << "^";
            break;
        case OperatorType::Modulo:
            stream << "%";
            break;
        case OperatorType::Logarithm:
            stream << "log";
            break;
        case OperatorType::Equal:
            stream << "=";
            break;
        case OperatorType::NotEqual:
            stream << "!=";
            break;
        case OperatorType::Less:
            stream << "<";
            break;
        case OperatorType::LessOrEqual:
            stream << "<=";
            break;
        case OperatorType::Greater:
            stream << ">";
            break;
        case OperatorType::GreaterOrEqual:
            stream << ">=";
            break;
        case OperatorType::Not:
            stream << "!";
            break;
        case OperatorType::Floor:
            stream << "floor";
            break;
        case OperatorType::Ceil:
            stream << "ceil";
            break;
        case OperatorType::Sin:
            stream << "sin";
            break;
        case OperatorType::Cos:
            stream << "cos";
            break;
        case OperatorType::Ite:
            stream << "ite";
            break;
        case OperatorType::AtMostOneOf:
            stream << "atMostOneOf";
            break;
        case OperatorType::AtLeastOneOf:
            stream << "atLeastOneOf";
            break;
        case OperatorType::ExactlyOneOf:
            stream << "exactlyOneOf";
            break;
    }
    return stream;
}
}  // namespace expressions
}  // namespace storm
