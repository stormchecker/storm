#pragma once

#include <cstdint>
#include <set>
#include <vector>

#include "storm/storage/dd/DdType.h"

namespace storm {
namespace expressions {
class Variable;
}
namespace dd {
template<storm::dd::DdType Type>
class DdManager;

template<storm::dd::DdType Type>
class Bdd;

template<storm::dd::DdType Type, typename ValueType>
class Add;
}  // namespace dd

namespace utility {
namespace dd {

template<storm::dd::DdType Type>
std::pair<storm::dd::Bdd<Type>, uint64_t> computeReachableStates(storm::dd::Bdd<Type> const& initialStates, storm::dd::Bdd<Type> const& transitions,
                                                                 std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                                 std::set<storm::expressions::Variable> const& columnMetaVariables);

template<storm::dd::DdType Type>
storm::dd::Bdd<Type> computeBackwardsReachableStates(storm::dd::Bdd<Type> const& initialStates, storm::dd::Bdd<Type> const& constraintStates,
                                                     storm::dd::Bdd<Type> const& transitions, std::set<storm::expressions::Variable> const& rowMetaVariables,
                                                     std::set<storm::expressions::Variable> const& columnMetaVariables);

template<storm::dd::DdType Type>
storm::dd::Bdd<Type> getRowColumnDiagonal(storm::dd::DdManager<Type> const& ddManager,
                                          std::vector<std::pair<storm::expressions::Variable, storm::expressions::Variable>> const& rowColumnMetaVariablePairs);

}  // namespace dd
}  // namespace utility
}  // namespace storm
