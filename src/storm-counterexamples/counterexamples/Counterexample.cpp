#include "Counterexample.h"

namespace storm {
namespace counterexamples {

std::ostream& operator<<(std::ostream& out, Counterexample const& counterexample) {
    counterexample.writeToStream(out);
    return out;
}

}  // namespace counterexamples
}  // namespace storm
