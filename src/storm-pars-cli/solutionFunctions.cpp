#include "storm-pars-cli/solutionFunctions.h"

#include "storm-cli-utilities/model-handling.h"
#include "storm-pars-cli/print.h"
#include "storm-pars/api/export.h"
#include "storm-pars/settings/modules/ParametricSettings.h"
#include "storm/analysis/GraphConditions.h"
#include "storm/api/verification.h"
#include "storm/exceptions/BaseException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/logic/Formula.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/modelchecker/results/SymbolicQuantitativeCheckResult.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Model.h"
#include "storm/models/symbolic/Dtmc.h"
#include "storm/models/symbolic/Model.h"
#include "storm/settings/SettingsManager.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/macros.h"

namespace storm::pars {

template<typename ValueType>
void verifyProperties(
    std::vector<storm::jani::Property> const& properties,
    std::function<std::unique_ptr<storm::modelchecker::CheckResult>(std::shared_ptr<storm::logic::Formula const> const& formula)> const& verificationCallback,
    std::function<void(std::unique_ptr<storm::modelchecker::CheckResult> const&)> const& postprocessingCallback) {
    for (auto const& property : properties) {
        storm::cli::printModelCheckingProperty(property);
        STORM_LOG_THROW(property.getRawFormula()->isOperatorFormula(), storm::exceptions::NotSupportedException,
                        "We only support operator formulas (P=?, R=?, etc).");
        STORM_LOG_THROW(!property.getRawFormula()->asOperatorFormula().hasBound(), storm::exceptions::NotSupportedException,
                        "We only support unbounded operator formulas (P=?, R=?, etc).");
        storm::utility::Stopwatch watch(true);
        std::unique_ptr<storm::modelchecker::CheckResult> result = verificationCallback(property.getRawFormula());
        watch.stop();
        printInitialStatesResult<ValueType>(result, &watch);
        postprocessingCallback(result);
    }
}

template<typename ValueType>
void computeSolutionFunctionsWithSparseEngine(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, storm::cli::SymbolicInput const& input) {
    verifyProperties<ValueType>(
        input.properties,
        [&model](std::shared_ptr<storm::logic::Formula const> const& formula) {
            std::unique_ptr<storm::modelchecker::CheckResult> result =
                storm::api::verifyWithSparseEngine<ValueType>(model, storm::api::createTask<ValueType>(formula, true));
            if (result) {
                result->filter(storm::modelchecker::ExplicitQualitativeCheckResult<ValueType>(model->getInitialStates()));
            }
            return result;
        },
        [&model](std::unique_ptr<storm::modelchecker::CheckResult> const& result) {
            auto parametricSettings = storm::settings::getModule<storm::settings::modules::ParametricSettings>();
            if (parametricSettings.exportResultToFile() && model->isOfType(storm::models::ModelType::Dtmc)) {
                auto dtmc = model->template as<storm::models::sparse::Dtmc<ValueType>>();
                std::optional<ValueType> rationalFunction = result->asExplicitQuantitativeCheckResult<ValueType>()[*model->getInitialStates().begin()];
                auto constraintCollector = storm::analysis::ConstraintCollector<ValueType>(*dtmc);
                api::exportParametricResultToFile<ValueType>(rationalFunction, constraintCollector, parametricSettings.exportResultPath());
            } else if (parametricSettings.exportResultToFile() && model->isOfType(storm::models::ModelType::Ctmc)) {
                auto ctmc = model->template as<storm::models::sparse::Ctmc<ValueType>>();
                std::optional<ValueType> rationalFunction = result->asExplicitQuantitativeCheckResult<ValueType>()[*model->getInitialStates().begin()];
                auto constraintCollector = storm::analysis::ConstraintCollector<ValueType>(*ctmc);
                api::exportParametricResultToFile<ValueType>(rationalFunction, constraintCollector, parametricSettings.exportResultPath());
            }
        });
}

template<storm::dd::DdType DdType, typename ValueType>
void computeSolutionFunctionsWithSymbolicEngine(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model,
                                                storm::cli::SymbolicInput const& input) {
    verifyProperties<ValueType>(
        input.properties,
        [&model](std::shared_ptr<storm::logic::Formula const> const& formula) {
            std::unique_ptr<storm::modelchecker::CheckResult> result =
                storm::api::verifyWithDdEngine<DdType, ValueType>(model, storm::api::createTask<ValueType>(formula, true));
            if (result) {
                result->filter(storm::modelchecker::SymbolicQualitativeCheckResult<DdType>(model->getReachableStates(), model->getInitialStates()));
            }
            return result;
        },
        [&model](std::unique_ptr<storm::modelchecker::CheckResult> const& result) {
            auto parametricSettings = storm::settings::getModule<storm::settings::modules::ParametricSettings>();
            if (parametricSettings.exportResultToFile() && model->isOfType(storm::models::ModelType::Dtmc)) {
                STORM_LOG_WARN("For symbolic engines, we currently do not support collecting graph-preserving constraints.");
                std::optional<ValueType> rationalFunction = result->asSymbolicQuantitativeCheckResult<DdType, ValueType>().sum();
                api::exportParametricResultToFile<ValueType>(rationalFunction, storm::NullRef, parametricSettings.exportResultPath());
            }
        });
}

template void verifyProperties<storm::RationalFunction>(
    std::vector<storm::jani::Property> const&,
    std::function<std::unique_ptr<storm::modelchecker::CheckResult>(std::shared_ptr<storm::logic::Formula const> const&)> const&,
    std::function<void(std::unique_ptr<storm::modelchecker::CheckResult> const&)> const&);

template void computeSolutionFunctionsWithSparseEngine<storm::RationalFunction>(std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> const&,
                                                                                storm::cli::SymbolicInput const&);

template void computeSolutionFunctionsWithSymbolicEngine<storm::dd::DdType::Sylvan, storm::RationalFunction>(
    std::shared_ptr<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction>> const&, storm::cli::SymbolicInput const&);

}  // namespace storm::pars
