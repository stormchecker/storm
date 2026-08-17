#pragma once

#include <set>

#include "storm/adapters/RationalFunctionForward.h"
#include "storm/storage/SparseMatrix.h"

namespace storm::storage {
std::set<storm::RationalFunctionVariable> getVariables(SparseMatrix<storm::RationalFunction> const& matrix);
}  // namespace storm::storage