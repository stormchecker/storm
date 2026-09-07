#include "ArgumentType.h"

namespace storm {
namespace settings {
std::ostream& operator<<(std::ostream& out, ArgumentType& argumentType) {
    switch (argumentType) {
        case ArgumentType::String:
            out << "string";
            break;
        case ArgumentType::Integer:
            out << "integer";
            break;
        case ArgumentType::UnsignedInteger:
            out << "unsigned integer";
            break;
        case ArgumentType::Double:
            out << "double";
            break;
        case ArgumentType::Boolean:
            out << "boolean";
            break;
    }

    return out;
}
}  // namespace settings
}  // namespace storm
