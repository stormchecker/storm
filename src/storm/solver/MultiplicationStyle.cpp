#include "MultiplicationStyle.h"

namespace storm {
namespace solver {

std::ostream& operator<<(std::ostream& out, MultiplicationStyle const& style) {
    switch (style) {
        case MultiplicationStyle::GaussSeidel:
            out << "Gauss-Seidel";
            break;
        case MultiplicationStyle::Regular:
            out << "Regular";
            break;
    }
    return out;
}

}  // namespace solver
}  // namespace storm
