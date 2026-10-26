#pragma once

#include "storm-cli-utilities/cli.h"
#include "storm-cli-utilities/model-handling.h"
#include "storm-pars/api/region.h"
#include "storm/models/sparse/Model.h"

namespace storm::pars {
template<typename ValueType>
void analyzeMonotonicity(storm::Environment const& env, std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, cli::SymbolicInput const& input,
                         std::vector<storm::storage::ParameterRegion<ValueType>> const& regions);
}