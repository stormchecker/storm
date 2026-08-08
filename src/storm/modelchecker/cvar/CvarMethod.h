#pragma once

#include "storm/utility/ExtendSettingEnumWithSelectionField.h"

namespace storm {
namespace modelchecker {
namespace cvar {

ExtendEnumsWithSelectionField(CvarMethod, Auto, WeightedReachability, SspParetoVi) std::string toString(CvarMethod method);

}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
