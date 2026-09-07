#include "ExplicitQualitativeGameResult.h"

namespace storm::gbar {
namespace abstraction {

ExplicitQualitativeGameResult::ExplicitQualitativeGameResult(storm::utility::graph::ExplicitGameProb01Result const& prob01Result)
    : storm::utility::graph::ExplicitGameProb01Result(prob01Result) {
    // Intentionally left empty.
}

storm::storage::BitVector const& ExplicitQualitativeGameResult::getStates() const {
    return this->getPlayer1States();
}

}  // namespace abstraction
}  // namespace storm::gbar
