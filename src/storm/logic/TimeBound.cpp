#include "TimeBound.h"

namespace storm {
namespace logic {

TimeBound::TimeBound(bool strict, storm::expressions::Expression const& bound) : strict(strict), bound(bound) {
    // Intentionally left empty.
}

storm::expressions::Expression const& TimeBound::getBound() const {
    return bound;
}

bool TimeBound::isStrict() const {
    return strict;
}

}  // namespace logic
}  // namespace storm
