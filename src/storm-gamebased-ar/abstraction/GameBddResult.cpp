#include "storm-gamebased-ar/abstraction/GameBddResult.h"
#include "storm/dd/DdInstantiations.h"
#include "storm/dd/sylvan/InternalSylvanBdd.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType>
GameBddResult<DdType>::GameBddResult() : bdd(), numberOfPlayer2Variables(0) {
    // Intentionally left empty.
}

template<storm::dd::DdType DdType>
GameBddResult<DdType>::GameBddResult(storm::dd::Bdd<DdType> const& gameBdd, uint_fast64_t numberOfPlayer2Variables)
    : bdd(gameBdd), numberOfPlayer2Variables(numberOfPlayer2Variables) {
    // Intentionally left empty.
}

#define STORM_DD_INSTANTIATE(DD) template struct GameBddResult<storm::dd::DdType::DD>;
STORM_DD_FOR_EACH_BACKEND(STORM_DD_INSTANTIATE)
#undef STORM_DD_INSTANTIATE

}  // namespace abstraction
}  // namespace storm::gbar
