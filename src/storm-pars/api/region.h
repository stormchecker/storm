#pragma once

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "storm-pars/modelchecker/region/AnnotatedRegion.h"
#include "storm-pars/modelchecker/region/RegionCheckEngine.h"
#include "storm-pars/modelchecker/region/RegionModelChecker.h"
#include "storm-pars/modelchecker/region/RegionOptions.h"
#include "storm-pars/modelchecker/region/RegionRefinementChecker.h"
#include "storm-pars/modelchecker/region/RegionResult.h"
#include "storm-pars/modelchecker/region/RegionResultHypothesis.h"
#include "storm-pars/modelchecker/region/RegionSplitEstimateKind.h"
#include "storm-pars/modelchecker/region/RegionSplittingStrategy.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityBackend.h"
#include "storm-pars/modelchecker/region/monotonicity/OrderBasedMonotonicityBackend.h"
#include "storm-pars/modelchecker/results/RegionCheckResult.h"
#include "storm-pars/modelchecker/results/RegionRefinementCheckResult.h"
#include "storm-pars/parser/MonotonicityParser.h"
#include "storm-pars/parser/ParameterRegionParser.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm-pars/utility/parameterlifting.h"

#include "storm/environment/Environment.h"

#include "storm/api/transformation.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/io/file.h"
#include "storm/models/sparse/Model.h"

namespace storm {

namespace api {

// Type aliases for backward compatibility
using MonotonicitySetting = storm::pars::modelchecker::MonotonicityOptions;

template<typename ValueType>
using RefinementOptions = storm::pars::modelchecker::RegionRefinementOptions<ValueType>;

template<typename ValueType>
std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> getModelParameters(storm::models::ModelBase const& model) {
    std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> modelParameters;
    if (model.isSparseModel()) {
        auto const& sparseModel = dynamic_cast<storm::models::sparse::Model<ValueType> const&>(model);
        modelParameters = storm::models::sparse::getProbabilityParameters(sparseModel);
        auto rewParameters = storm::models::sparse::getRewardParameters(sparseModel);
        modelParameters.insert(rewParameters.begin(), rewParameters.end());
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Retrieving model parameters is not supported for the given model type.");
    }
    return modelParameters;
}

template<typename ValueType>
std::vector<typename storm::storage::ParameterRegion<ValueType>::VariableType> parseVariableList(
    std::string const& inputString, std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> const& consideredVariables) {
    if (inputString == "") {
        return {};
    }
    std::vector<typename storm::storage::ParameterRegion<ValueType>::VariableType> variables;
    std::vector<std::string> variableStrings;
    boost::split(variableStrings, inputString, boost::is_any_of(","));
    for (auto& var : variableStrings) {
        boost::trim(var);
        bool found = false;
        // Find parameter in list
        for (auto const& param : consideredVariables) {
            if (var == param.name()) {
                variables.push_back(param);
                found = true;
                break;
            }
        }
        if (!found) {
            STORM_LOG_ERROR("Variable " << var << " not found.");
        }
    }
    return variables;
}

template<typename ValueType>
std::vector<typename storm::storage::ParameterRegion<ValueType>::VariableType> parseVariableList(std::string const& inputString,
                                                                                                 storm::models::ModelBase const& model) {
    return parseVariableList<ValueType>(inputString, getModelParameters<ValueType>(model));
}

template<typename ValueType>
std::vector<storm::storage::ParameterRegion<ValueType>> parseRegions(
    std::string const& inputString, std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> const& consideredVariables) {
    // If the given input string looks like a file (containing a dot and there exists a file with that name),
    // we try to parse it as a file, otherwise we assume it's a region string.
    if (inputString.find(".") != std::string::npos && storm::io::fileExistsAndIsReadable(inputString)) {
        return storm::parser::ParameterRegionParser<ValueType>().parseMultipleRegionsFromFile(inputString, consideredVariables);
    } else {
        return storm::parser::ParameterRegionParser<ValueType>().parseMultipleRegions(inputString, consideredVariables);
    }
}

template<typename ValueType>
std::vector<storm::storage::ParameterRegion<ValueType>> parseRegions(std::string const& inputString, storm::models::ModelBase const& model) {
    return parseRegions<ValueType>(inputString, getModelParameters<ValueType>(model));
}

template<typename ValueType>
storm::storage::ParameterRegion<ValueType> createRegion(
    std::string const& inputString, std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> const& consideredVariables) {
    return storm::parser::ParameterRegionParser<ValueType>().createRegion(inputString, consideredVariables);
}

template<typename ValueType>
std::vector<storm::storage::ParameterRegion<ValueType>> createRegion(std::string const& inputString, storm::models::ModelBase const& model) {
    std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> modelParameters;
    if (model.isSparseModel()) {
        auto const& sparseModel = dynamic_cast<storm::models::sparse::Model<ValueType> const&>(model);
        modelParameters = storm::models::sparse::getProbabilityParameters(sparseModel);
        auto rewParameters = storm::models::sparse::getRewardParameters(sparseModel);
        modelParameters.insert(rewParameters.begin(), rewParameters.end());
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Retrieving model parameters is not supported for the given model type.");
    }
    return std::vector<storm::storage::ParameterRegion<ValueType>>({createRegion<ValueType>(inputString, modelParameters)});
}

template<typename ValueType>
storm::storage::ParameterRegion<ValueType> parseRegion(std::string const& inputString,
                                                       std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> const& consideredVariables) {
    // Handle the "empty region" case
    if (inputString == "" && consideredVariables.empty()) {
        return storm::storage::ParameterRegion<ValueType>();
    }

    auto res = parseRegions<ValueType>(inputString, consideredVariables);
    STORM_LOG_THROW(res.size() == 1, storm::exceptions::InvalidOperationException, "Parsed " << res.size() << " regions but exactly one was expected.");
    return res.front();
}

template<typename ValueType>
storm::storage::ParameterRegion<ValueType> parseRegion(std::string const& inputString, storm::models::ModelBase const& model) {
    // Handle the "empty region" case
    if (inputString == "" && !model.hasParameters()) {
        return storm::storage::ParameterRegion<ValueType>();
    }

    auto res = parseRegions<ValueType>(inputString, model);
    STORM_LOG_THROW(res.size() == 1, storm::exceptions::InvalidOperationException, "Parsed " << res.size() << " regions but exactly one was expected.");
    return res.front();
}

template<typename ValueType>
std::pair<std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType>,
          std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType>>
parseMonotoneParameters(std::string const& fileName, std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model) {
    std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> modelParameters;
    modelParameters = storm::models::sparse::getProbabilityParameters(*model);
    auto rewParameters = storm::models::sparse::getRewardParameters(*model);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());
    return std::move(storm::parser::MonotonicityParser<typename storm::storage::ParameterRegion<ValueType>::VariableType>().parseMonotoneVariablesFromFile(
        fileName, modelParameters));
}

template<typename ParametricType>
std::shared_ptr<storm::models::sparse::Model<ParametricType>> preprocessSparseModelForParameterLifting(
    std::shared_ptr<storm::models::sparse::Model<ParametricType>> const& model,
    storm::modelchecker::CheckTask<storm::logic::Formula, ParametricType> const& task, storm::modelchecker::RegionCheckEngine engine,
    bool preconditionsValidatedManually = false) {
    STORM_LOG_WARN_COND(preconditionsValidatedManually || storm::utility::parameterlifting::validateParameterLiftingSound(*model, task.getFormula(), engine),
                        "Could not validate whether parameter lifting is applicable. Please validate manually...");
    std::shared_ptr<storm::models::sparse::Model<ParametricType>> consideredModel = model;

    // Treat continuous time models
    if (consideredModel->isOfType(storm::models::ModelType::Ctmc) || consideredModel->isOfType(storm::models::ModelType::MarkovAutomaton)) {
        STORM_LOG_WARN("Parameter lifting not supported for continuous time models. Transforming continuous model to discrete model...");
        std::vector<std::shared_ptr<storm::logic::Formula const>> taskFormulaAsVector{task.getFormula().asSharedPointer()};
        consideredModel = storm::api::transformContinuousToDiscreteTimeSparseModel(consideredModel, taskFormulaAsVector).first;
        STORM_LOG_THROW(consideredModel->isOfType(storm::models::ModelType::Dtmc) || consideredModel->isOfType(storm::models::ModelType::Mdp),
                        storm::exceptions::UnexpectedException, "Transformation to discrete time model has failed.");
    }
    return consideredModel;
}

template<typename ParametricType, typename ImpreciseType = double, typename PreciseType = storm::RationalNumber>
std::unique_ptr<storm::modelchecker::RegionModelChecker<ParametricType>> createRegionModelChecker(storm::modelchecker::RegionCheckEngine engine,
                                                                                                  storm::models::ModelType modelType);

extern template std::unique_ptr<storm::modelchecker::RegionModelChecker<storm::RationalFunction>>
createRegionModelChecker<storm::RationalFunction, double, storm::RationalNumber>(storm::modelchecker::RegionCheckEngine engine,
                                                                                 storm::models::ModelType modelType);

template<typename ParametricType, typename ImpreciseType = double, typename PreciseType = storm::RationalNumber>
std::unique_ptr<storm::modelchecker::MonotonicityBackend<ParametricType>> initializeMonotonicityBackend(
    storm::modelchecker::RegionModelChecker<ParametricType> const& regionChecker, storm::modelchecker::RegionCheckEngine engine,
    storm::modelchecker::CheckTask<storm::logic::Formula, ParametricType> const& task, MonotonicitySetting const& monotonicitySetting,
    std::optional<std::pair<std::set<typename storm::storage::ParameterRegion<ParametricType>::VariableType>,
                            std::set<typename storm::storage::ParameterRegion<ParametricType>::VariableType>>>
        monotoneParameters = std::nullopt) {
    // Initialize default backend
    auto monotonicityBackend = std::make_unique<storm::modelchecker::MonotonicityBackend<ParametricType>>();

    // Potentially replace default by order-based monotonicity
    if (monotonicitySetting.useMonotonicity) {
        std::unique_ptr<storm::modelchecker::MonotonicityBackend<ParametricType>> orderBasedBackend;
        if (engine == storm::modelchecker::RegionCheckEngine::ExactParameterLifting) {
            orderBasedBackend = std::make_unique<storm::modelchecker::OrderBasedMonotonicityBackend<ParametricType, PreciseType>>(
                monotonicitySetting.useOnlyGlobalMonotonicity, monotonicitySetting.useBoundsFromPLA);
        } else {
            orderBasedBackend = std::make_unique<storm::modelchecker::OrderBasedMonotonicityBackend<ParametricType, ImpreciseType>>(
                monotonicitySetting.useOnlyGlobalMonotonicity, monotonicitySetting.useBoundsFromPLA);
        }
        if (regionChecker.isMonotonicitySupported(*orderBasedBackend, task)) {
            monotonicityBackend = std::move(orderBasedBackend);
        } else {
            STORM_LOG_WARN("Order-based Monotonicity enabled for region checking engine " << engine << " but not supported in this configuration.");
        }
    }

    // Insert monotone parameters if available
    if (monotoneParameters) {
        for (auto const& incrPar : monotoneParameters->first) {
            monotonicityBackend->setMonotoneParameter(incrPar, storm::analysis::MonotonicityKind::Incr);
        }
        for (auto const& decrPar : monotoneParameters->second) {
            monotonicityBackend->setMonotoneParameter(decrPar, storm::analysis::MonotonicityKind::Decr);
        }
    }
    return monotonicityBackend;
}

template<typename ValueType, typename ImpreciseType = double, typename PreciseType = storm::RationalNumber>
std::unique_ptr<storm::modelchecker::RegionModelChecker<ValueType>> initializeRegionModelChecker(
    Environment const& env, std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
    storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& task, storm::modelchecker::RegionCheckEngine engine,
    bool allowModelSimplification = true, bool graphPreserving = true, bool preconditionsValidated = false,
    MonotonicitySetting monotonicitySetting = MonotonicitySetting(),
    std::optional<std::pair<std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType>,
                            std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType>>>
        monotoneParameters = std::nullopt) {
    auto consideredModel = preprocessSparseModelForParameterLifting(model, task, engine, preconditionsValidated);
    auto regionChecker = createRegionModelChecker<ValueType, ImpreciseType, PreciseType>(engine, model->getType());
    auto monotonicityBackend =
        initializeMonotonicityBackend<ValueType, ImpreciseType, PreciseType>(*regionChecker, engine, task, monotonicitySetting, monotoneParameters);
    if (allowModelSimplification) {
        allowModelSimplification = monotonicityBackend->recommendModelSimplifications();
        STORM_LOG_WARN_COND(allowModelSimplification, "Model simplification is disabled because the monotonicity algorithm does not recommend it.");
    }
    regionChecker->specify(env, consideredModel, task, std::nullopt, std::move(monotonicityBackend), allowModelSimplification, graphPreserving);
    return regionChecker;
}

template<typename ValueType>
std::unique_ptr<storm::modelchecker::RegionModelChecker<ValueType>> initializeRegionModelChecker(
    std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& task,
    storm::modelchecker::RegionCheckEngine engine) {
    Environment env;
    return initializeRegionModelChecker(env, model, task, engine);
}

template<typename ValueType>
std::unique_ptr<storm::modelchecker::RegionCheckResult<ValueType>> checkRegionsWithSparseEngine(
    std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& task,
    std::vector<storm::storage::ParameterRegion<ValueType>> const& regions, storm::modelchecker::RegionCheckEngine engine,
    std::vector<storm::modelchecker::RegionResultHypothesis> const& hypotheses, bool sampleVerticesOfRegions) {
    Environment env;
    auto regionChecker = initializeRegionModelChecker(env, model, task, engine);
    return regionChecker->analyzeRegions(env, regions, hypotheses, sampleVerticesOfRegions);
}

template<typename ValueType>
std::unique_ptr<storm::modelchecker::RegionCheckResult<ValueType>> checkRegionsWithSparseEngine(
    std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> const& task,
    std::vector<storm::storage::ParameterRegion<ValueType>> const& regions, storm::modelchecker::RegionCheckEngine engine,
    storm::modelchecker::RegionResultHypothesis const& hypothesis = storm::modelchecker::RegionResultHypothesis::Unknown,
    bool sampleVerticesOfRegions = false) {
    std::vector<storm::modelchecker::RegionResultHypothesis> hypotheses(regions.size(), hypothesis);
    return checkRegionsWithSparseEngine(model, task, regions, engine, hypotheses, sampleVerticesOfRegions);
}

template<typename ValueType, typename ImpreciseType = double, typename PreciseType = storm::RationalNumber>
std::unique_ptr<storm::modelchecker::RegionRefinementChecker<ValueType>> initializeRegionRefinementChecker(Environment const& env,
                                                                                                           RefinementOptions<ValueType> settings) {
    auto consideredModel = preprocessSparseModelForParameterLifting(settings.model, settings.task, settings.engine, settings.preconditionsValidated);
    auto regionChecker = createRegionModelChecker<ValueType, ImpreciseType, PreciseType>(settings.engine, settings.model->getType());
    auto monotonicityBackend = initializeMonotonicityBackend<ValueType, ImpreciseType, PreciseType>(*regionChecker, settings.engine, settings.task,
                                                                                                    settings.monotonicitySetting, settings.monotoneParameters);
    settings.allowModelSimplification = settings.allowModelSimplification && monotonicityBackend->recommendModelSimplifications();
    auto refinementChecker = std::make_unique<storm::modelchecker::RegionRefinementChecker<ValueType>>(std::move(regionChecker));
    refinementChecker->specify(env, consideredModel, settings.task, std::move(settings.regionSplittingStrategy), std::move(settings.discreteVariables),
                               std::move(monotonicityBackend), settings.allowModelSimplification, settings.graphPreserving);
    return refinementChecker;
}

/*!
 * Checks and iteratively refines the given region with the sparse engine
 * @param engine The considered region checking engine
 * @param coverageThreshold if given, the refinement stops as soon as the fraction of the area of the subregions with inconclusive result is less then this
 * threshold
 * @param refinementDepthThreshold if given, the refinement stops at the given depth. depth=0 means no refinement.
 * @param hypothesis if not 'unknown', it is only checked whether the hypothesis holds (and NOT the complementary result).
 * @param allowModelSimplification
 * @param useMonotonicity
 * @param monThresh if given, determines at which depth to start using monotonicity
 */
template<typename ValueType>
std::unique_ptr<storm::modelchecker::RegionRefinementCheckResult<ValueType>> checkAndRefineRegionWithSparseEngine(
    RefinementOptions<ValueType> settings, storm::storage::ParameterRegion<ValueType> const& region, std::optional<ValueType> const& coverageThreshold,
    std::optional<uint64_t> const& refinementDepthThreshold = std::nullopt,
    storm::modelchecker::RegionResultHypothesis hypothesis = storm::modelchecker::RegionResultHypothesis::Unknown, uint64_t monThresh = 0) {
    Environment env;
    auto const& regionRefinementChecker = initializeRegionRefinementChecker(env, settings);
    return regionRefinementChecker->performRegionPartitioning(env, region, coverageThreshold, refinementDepthThreshold, hypothesis, monThresh);
}

// TODO: update documentation
/*!
 * Finds the extremal value in the given region
 * @param settings The refinement settings
 * @param region The region to be analyzed
 * @param dir The optimization direction
 * @param precision The precision to be used
 * @param absolutePrecision If true, the precision is interpreted as an absolute precision, otherwise as a relative precision
 * @param boundInvariant Bound invariant that should hold
 * @return A pair consisting of the extremal value and a parameter valuation achieving this value
 */
template<typename ValueType>
std::pair<storm::RationalNumber, typename storm::storage::ParameterRegion<ValueType>::Valuation> computeExtremalValue(
    RefinementOptions<ValueType> settings, storm::storage::ParameterRegion<ValueType> const& region, storm::solver::OptimizationDirection const& dir,
    std::optional<ValueType> const& precision, bool absolutePrecision, std::optional<storm::logic::Bound> const& boundInvariant) {
    Environment env;
    auto refinementChecker = initializeRegionRefinementChecker(env, settings);
    auto res =
        refinementChecker->computeExtremalValue(env, region, dir, precision.value_or(storm::utility::zero<ValueType>()), absolutePrecision, boundInvariant);
    return {storm::utility::convertNumber<storm::RationalNumber>(res.first), std::move(res.second)};
}

/*!
 * Verifies whether a region satisfies a property.
 * @param settings The refinement options
 * @param region The region to be analyzed
 * @return true if the region satisfies the property, false otherwise
 */
template<typename ValueType>
bool verifyRegion(RefinementOptions<ValueType> settings, storm::storage::ParameterRegion<ValueType> const& region) {
    Environment env;
    STORM_LOG_THROW(settings.task.getFormula().isProbabilityOperatorFormula() || settings.task.getFormula().isRewardOperatorFormula(),
                    storm::exceptions::NotSupportedException, "Only probability and reward operators supported.");
    STORM_LOG_THROW(settings.task.getFormula().asOperatorFormula().hasBound(), storm::exceptions::NotSupportedException,
                    "Verification requires a bounded operator formula.");

    storm::logic::Bound const& bound = settings.task.getFormula().asOperatorFormula().getBound();
    auto refinementChecker = initializeRegionRefinementChecker(env, settings);
    return refinementChecker->verifyRegion(env, region, bound);
}

template<typename ValueType>
void exportRegionCheckResultToFile(std::unique_ptr<storm::modelchecker::CheckResult> const& checkResult, std::string const& filename,
                                   bool onlyConclusiveResults = false) {
    auto const* regionCheckResult = dynamic_cast<storm::modelchecker::RegionCheckResult<ValueType> const*>(checkResult.get());
    STORM_LOG_THROW(regionCheckResult != nullptr, storm::exceptions::UnexpectedException,
                    "Can not export region check result: The given checkresult does not have the expected type.");

    std::ofstream filestream;
    storm::io::openFile(filename, filestream);
    for (auto const& res : regionCheckResult->getRegionResults()) {
        if (!onlyConclusiveResults || res.second == storm::modelchecker::RegionResult::AllViolated || res.second == storm::modelchecker::RegionResult::AllSat ||
            res.second == storm::modelchecker::RegionResult::AllIllDefined) {
            filestream << res.second << ": " << res.first << '\n';
        }
    }
    storm::io::closeFile(filestream);
}

}  // namespace api
}  // namespace storm
