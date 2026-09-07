#include "ExplicitQualitativeResult.h"

#include "storm-gamebased-ar/abstraction/ExplicitQualitativeGameResult.h"

namespace storm::gbar {
namespace abstraction {

ExplicitQualitativeGameResult& ExplicitQualitativeResult::asExplicitQualitativeGameResult() {
    return static_cast<ExplicitQualitativeGameResult&>(*this);
}

ExplicitQualitativeGameResult const& ExplicitQualitativeResult::asExplicitQualitativeGameResult() const {
    return static_cast<ExplicitQualitativeGameResult const&>(*this);
}

}  // namespace abstraction
}  // namespace storm::gbar
