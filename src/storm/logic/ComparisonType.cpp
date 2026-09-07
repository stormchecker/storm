#include "ComparisonType.h"

#include <ostream>

namespace storm {
namespace logic {
std::ostream& operator<<(std::ostream& out, ComparisonType const& comparisonType) {
    switch (comparisonType) {
        case ComparisonType::Less:
            out << "<";
            break;
        case ComparisonType::LessEqual:
            out << "<=";
            break;
        case ComparisonType::Greater:
            out << ">";
            break;
        case ComparisonType::GreaterEqual:
            out << ">=";
            break;
    }
    return out;
}
}  // namespace logic
}  // namespace storm
