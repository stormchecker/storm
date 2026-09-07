#pragma once

#include "storm-pars/api/region.h"
#include "storm-pars/settings/modules/FeasibilitySettings.h"
#include "storm/models/sparse/Model.h"

namespace storm::pars {

class FeasibilitySynthesisTask;

std::shared_ptr<FeasibilitySynthesisTask const> createFeasibilitySynthesisTaskFromSettings(
    std::shared_ptr<storm::logic::Formula const> const& formula, std::vector<storm::storage::ParameterRegion<storm::RationalFunction>> const& regions);

template<typename ValueType>
void performFeasibility(std::shared_ptr<storm::models::sparse::Model<ValueType>> model,
                        std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                        boost::optional<std::set<RationalFunctionVariable>> omittedParameters,
                        storm::api::MonotonicitySetting monotonicitySettings = storm::api::MonotonicitySetting());

template<typename ValueType>
void runFeasibilityWithGD(std::shared_ptr<storm::models::sparse::Model<ValueType>> model,
                          std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                          boost::optional<std::set<RationalFunctionVariable>> omittedParameters,
                          storm::api::MonotonicitySetting monotonicitySettings = storm::api::MonotonicitySetting());

template<typename ValueType>
void runFeasibilityWithPLA(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                           std::shared_ptr<storm::pars::FeasibilitySynthesisTask const> const& task,
                           boost::optional<std::set<RationalFunctionVariable>> omittedParameters,
                           storm::api::MonotonicitySetting monotonicitySettings = storm::api::MonotonicitySetting());
}  // namespace storm::pars