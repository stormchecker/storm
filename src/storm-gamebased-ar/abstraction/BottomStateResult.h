#pragma once

#include "storm/dd/Bdd.h"
#include "storm/dd/DdType.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType>
struct BottomStateResult {
   public:
    BottomStateResult(storm::dd::Bdd<DdType> const& states, storm::dd::Bdd<DdType> const& transitions);

    storm::dd::Bdd<DdType> states;
    storm::dd::Bdd<DdType> transitions;
};

}  // namespace abstraction
}  // namespace storm::gbar
