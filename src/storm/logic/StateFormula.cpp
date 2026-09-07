#include "StateFormula.h"

namespace storm {
namespace logic {
bool StateFormula::isStateFormula() const {
    return true;
}

bool StateFormula::isProbabilityPathFormula() const {
    // a single state formula can be seen as a path formula as well
    return true;
}

}  // namespace logic
}  // namespace storm
