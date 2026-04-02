#include "storm-pars/api/storm-pars.h"
#include "storm-cli-utilities/cli.h"
#include "storm-cli-utilities/model-handling.h"
#include "storm-pars-cli/feasibility.h"
#include "storm-pars-cli/monotonicity.h"
#include "storm-pars-cli/print.h"
#include "storm-pars-cli/sampling.h"
#include "storm-pars-cli/solutionFunctions.h"
#include "storm-pars/api/region.h"
#include "storm-pars/modelchecker/region/RegionSplittingStrategy.h"
#include "storm-pars/modelchecker/region/SparseDtmcParameterLiftingModelChecker.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityHelper.h"
#include "storm-pars/settings/ParsSettings.h"
#include "storm-pars/settings/modules/MonotonicitySettings.h"
#include "storm-pars/settings/modules/ParametricSettings.h"
#include "storm-pars/settings/modules/PartitionSettings.h"
#include "storm-pars/settings/modules/RegionSettings.h"
#include "storm-pars/settings/modules/RegionVerificationSettings.h"
#include "storm-pars/settings/modules/SamplingSettings.h"
#include "storm-pars/transformer/BigStep.h"
#include "storm-pars/transformer/BinaryDtmcTransformer.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm-pars/transformer/SparseParametricMdpSimplifier.h"
#include "storm-pars/utility/parametric.h"
#include "storm/api/storm.h"
#include "storm/exceptions/BaseException.h"
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/ModelBase.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/BisimulationSettings.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/TransformationSettings.h"
#include "storm/solver/stateelimination/NondeterministicModelStateEliminator.h"
#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/utility/Engine.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/initialize.h"
#include "storm/utility/macros.h"

namespace storm {
namespace pars {
struct PreprocessResult {
    PreprocessResult(std::shared_ptr<storm::models::ModelBase> const& model, bool changed) : changed(changed), model(model) {
        // Intentionally left empty.
    }

    bool changed;
    std::shared_ptr<storm::models::ModelBase> model;
    boost::optional<std::vector<std::shared_ptr<storm::logic::Formula const>>> formulas;
};

template<typename ValueType>
std::vector<storm::storage::ParameterRegion<ValueType>> parseRegions(std::shared_ptr<storm::models::ModelBase> const& model) {
    std::vector<storm::storage::ParameterRegion<ValueType>> result;
    auto regionSettings = storm::settings::getModule<storm::settings::modules::RegionSettings>();
    if (regionSettings.isRegionSet()) {
        result = storm::api::parseRegions<ValueType>(regionSettings.getRegionString(), *model);
    } else if (regionSettings.isRegionBoundSet()) {
        result = storm::api::createRegion<ValueType>(regionSettings.getRegionBoundString(), *model);
    }
    if (regionSettings.isAssumeGraphPreservingSet()) {
        // We want to warn the user in case the model is actually not graph preserving.
        // However, determining graph-preservingness precisely is hard.
        // As an approximation, we only check if the region intersects 0 or 1.
        for (auto const& region : result) {
            for (auto const& variable : region.getVariables()) {
                if (region.getLowerBoundary(variable) <= storm::utility::zero<typename storm::utility::parametric::CoefficientType<ValueType>::type>() ||
                    region.getUpperBoundary(variable) >= storm::utility::one<typename storm::utility::parametric::CoefficientType<ValueType>::type>()) {
                    STORM_LOG_WARN(
                        "Region "
                        << region
                        << " appears to not preserve the graph structure of the parametric model. If this is the case, set --assume-graph-preserving false.");
                    break;
                }
            }
        }
    }
    return result;
}

template<typename ValueType>
std::shared_ptr<storm::models::ModelBase> eliminateScc(std::shared_ptr<storm::models::ModelBase> const& model) {
    storm::utility::Stopwatch eliminationWatch(true);
    std::shared_ptr<storm::models::ModelBase> result;
    if (model->isOfType(storm::models::ModelType::Dtmc)) {
        STORM_PRINT("Applying scc elimination\n");
        auto sparseModel = model->as<storm::models::sparse::Model<ValueType>>();
        auto matrix = sparseModel->getTransitionMatrix();
        auto backwardsTransitionMatrix = matrix.transpose();

        storm::storage::StronglyConnectedComponentDecompositionOptions const options;
        auto decomposition = storm::storage::StronglyConnectedComponentDecomposition<ValueType>(matrix, options);

        storm::storage::BitVector selectedStates(matrix.getRowCount());
        storm::storage::BitVector selfLoopStates(matrix.getRowCount());
        for (size_t i = 0; i < decomposition.size(); ++i) {
            auto scc = decomposition.getBlock(i);
            if (scc.size() > 1) {
                auto statesScc = scc.getStates();
                std::vector<uint_fast64_t> entryStates;
                for (auto state : statesScc) {
                    auto row = backwardsTransitionMatrix.getRow(state);
                    bool found = false;
                    for (auto backState : row) {
                        if (!scc.containsState(backState.getColumn())) {
                            found = true;
                        }
                    }
                    if (found) {
                        entryStates.push_back(state);
                        selfLoopStates.set(state);
                    } else {
                        selectedStates.set(state);
                    }
                }

                if (entryStates.size() != 1) {
                    STORM_LOG_THROW(entryStates.size() > 1, storm::exceptions::NotImplementedException,
                                    "state elimination not implemented for scc with more than 1 entry points");
                }
            }
        }

        storm::storage::FlexibleSparseMatrix<ValueType> flexibleMatrix(matrix);
        storm::storage::FlexibleSparseMatrix<ValueType> flexibleBackwardTransitions(backwardsTransitionMatrix, true);
        auto actionRewards = std::vector<ValueType>(matrix.getRowCount(), storm::utility::zero<ValueType>());
        storm::solver::stateelimination::NondeterministicModelStateEliminator<ValueType> stateEliminator(flexibleMatrix, flexibleBackwardTransitions,
                                                                                                         actionRewards);
        for (auto state : selectedStates) {
            stateEliminator.eliminateState(state, true);
        }
        for (auto state : selfLoopStates) {
            auto row = flexibleMatrix.getRow(state);
            stateEliminator.eliminateLoop(state);
        }
        selectedStates.complement();
        auto keptRows = matrix.getRowFilter(selectedStates);
        storm::storage::SparseMatrix<ValueType> newTransitionMatrix = flexibleMatrix.createSparseMatrix(keptRows, selectedStates);
        // TODO @Jip: note that rewards get lost
        result = std::make_shared<storm::models::sparse::Dtmc<ValueType>>(std::move(newTransitionMatrix),
                                                                          sparseModel->getStateLabeling().getSubLabeling(selectedStates));

        eliminationWatch.stop();
        STORM_PRINT("\nTime for scc elimination: " << eliminationWatch << ".\n\n");
        result->printModelInformationToStream(std::cout);
    } else if (model->isOfType(storm::models::ModelType::Mdp)) {
        STORM_LOG_THROW(false, storm::exceptions::NotImplementedException,
                        "Unable to perform SCC elimination for monotonicity analysis on MDP: Not mplemented");
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Unable to perform monotonicity analysis on the provided model type.");
    }
    return result;
}

template<typename ValueType>
std::shared_ptr<storm::models::ModelBase> simplifyModel(std::shared_ptr<storm::models::ModelBase> const& model, cli::SymbolicInput const& input) {
    storm::utility::Stopwatch simplifyingWatch(true);
    std::shared_ptr<storm::models::ModelBase> result;
    if (model->isOfType(storm::models::ModelType::Dtmc)) {
        storm::transformer::SparseParametricDtmcSimplifier<storm::models::sparse::Dtmc<ValueType>> simplifier(
            *(model->template as<storm::models::sparse::Dtmc<ValueType>>()));

        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(input.properties);
        STORM_LOG_THROW(formulas.begin() != formulas.end(), storm::exceptions::NotSupportedException, "Only one formula at the time supported");

        if (!simplifier.simplify(*(formulas[0]))) {
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Simplifying the model was not successfull.");
        }
        result = simplifier.getSimplifiedModel();
    } else if (model->isOfType(storm::models::ModelType::Mdp)) {
        storm::transformer::SparseParametricMdpSimplifier<storm::models::sparse::Mdp<ValueType>> simplifier(
            *(model->template as<storm::models::sparse::Mdp<ValueType>>()));

        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(input.properties);
        STORM_LOG_THROW(formulas.begin() != formulas.end(), storm::exceptions::NotSupportedException, "Only one formula at the time supported");

        if (!simplifier.simplify(*(formulas[0]))) {
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Simplifying the model was not successfull.");
        }
        result = simplifier.getSimplifiedModel();
    } else {
        STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Unable to perform monotonicity analysis on the provided model type.");
    }

    simplifyingWatch.stop();
    STORM_PRINT("\nTime for model simplification: " << simplifyingWatch << ".\n\n");
    result->printModelInformationToStream(std::cout);
    return result;
}

template<typename ValueType>
PreprocessResult preprocessSparseModel(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, cli::SymbolicInput const& input,
                                       storm::cli::ModelProcessingInformation const& mpi) {
    auto bisimulationSettings = storm::settings::getModule<storm::settings::modules::BisimulationSettings>();
    auto parametricSettings = storm::settings::getModule<storm::settings::modules::ParametricSettings>();
    auto transformationSettings = storm::settings::getModule<storm::settings::modules::TransformationSettings>();
    auto monSettings = storm::settings::getModule<storm::settings::modules::MonotonicitySettings>();
    auto regionSettings = storm::settings::getModule<storm::settings::modules::RegionSettings>();

    PreprocessResult result(model, false);
    // TODO: why only simplify in these modes
    if (parametricSettings.getOperationMode() == storm::pars::utility::ParametricMode::Monotonicity ||
        parametricSettings.getOperationMode() == storm::pars::utility::ParametricMode::Feasibility) {
        STORM_LOG_THROW(!input.properties.empty(), storm::exceptions::InvalidSettingsException, "Simplification requires property to be specified");
        result.model = storm::pars::simplifyModel<ValueType>(result.model, input);
        result.changed = true;
    }

    if (result.model->isOfType(storm::models::ModelType::MarkovAutomaton)) {
        result.model = storm::cli::preprocessSparseMarkovAutomaton(result.model->template as<storm::models::sparse::MarkovAutomaton<ValueType>>());
        result.changed = true;
    }

    if (mpi.applyBisimulation) {
        result.model = storm::cli::preprocessSparseModelBisimulation(result.model->template as<storm::models::sparse::Model<ValueType>>(), input,
                                                                     bisimulationSettings, regionSettings.isAssumeGraphPreservingSet());
        result.changed = true;
    }

    if (parametricSettings.isLinearToSimpleEnabled()) {
        STORM_LOG_INFO("Transforming linear to simple...");
        transformer::BinaryDtmcTransformer transformer;
        result.model = transformer.transform(*result.model->template as<storm::models::sparse::Dtmc<RationalFunction>>(), true);
        result.changed = true;
    }

    if (parametricSettings.isBigStepEnabled()) {
        transformer::BigStep tt;
        auto formulas = storm::api::extractFormulasFromProperties(input.properties);
        storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalFunction> checkTask(*formulas[0]);
        auto bigStepResult = tt.bigStep(*result.model->template as<storm::models::sparse::Dtmc<RationalFunction>>(), checkTask);
        result.model = std::make_shared<storm::models::sparse::Dtmc<RationalFunction>>(bigStepResult.first);

        if (mpi.applyBisimulation) {
            result.model = storm::cli::preprocessSparseModelBisimulation(result.model->template as<storm::models::sparse::Model<ValueType>>(), input,
                                                                         bisimulationSettings, regionSettings.isAssumeGraphPreservingSet());
        }
        result.changed = true;
    }

    if (transformationSettings.isChainEliminationSet() && model->isOfType(storm::models::ModelType::MarkovAutomaton)) {
        // TODO: Why only on MAs?
        auto eliminationResult =
            storm::api::eliminateNonMarkovianChains(result.model->template as<storm::models::sparse::MarkovAutomaton<ValueType>>(),
                                                    storm::api::extractFormulasFromProperties(input.properties), transformationSettings.getLabelBehavior());
        result.model = eliminationResult.first;
        // Set transformed properties as new properties in input
        result.formulas = eliminationResult.second;
        result.changed = true;
    }

    if (parametricSettings.transformContinuousModel() &&
        (model->isOfType(storm::models::ModelType::Ctmc) || model->isOfType(storm::models::ModelType::MarkovAutomaton))) {
        auto transformResult = storm::api::transformContinuousToDiscreteTimeSparseModel(
            std::move(*model->template as<storm::models::sparse::Model<ValueType>>()), storm::api::extractFormulasFromProperties(input.properties));
        result.model = transformResult.first;
        // Set transformed properties as new properties in input
        result.formulas = transformResult.second;
        result.changed = true;
    }

    if (monSettings.isSccEliminationSet()) {
        // TODO move this into the API?
        result.model = storm::pars::eliminateScc<ValueType>(result.model);
        result.changed = true;
    }

    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
PreprocessResult preprocessDdModel(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model, cli::SymbolicInput const& input,
                                   storm::cli::ModelProcessingInformation const& mpi) {
    auto bisimulationSettings = storm::settings::getModule<storm::settings::modules::BisimulationSettings>();

    PreprocessResult result(model, false);

    if (mpi.engine == storm::utility::Engine::Hybrid) {
        // Currently, hybrid engine for parametric models just refers to building the model symbolically.
        STORM_LOG_INFO("Translating symbolic model to sparse model...");
        result.model = storm::api::transformSymbolicToSparseModel(model);
        result.changed = true;
        // Invoke preprocessing on the sparse model
        PreprocessResult sparsePreprocessingResult =
            storm::pars::preprocessSparseModel<ValueType>(result.model->as<storm::models::sparse::Model<ValueType>>(), input, mpi);
        if (sparsePreprocessingResult.changed) {
            result.model = sparsePreprocessingResult.model;
            result.formulas = sparsePreprocessingResult.formulas;
        }
    } else {
        STORM_LOG_ASSERT(mpi.engine == storm::utility::Engine::Dd, "Expected Dd engine.");
        if (mpi.applyBisimulation) {
            result.model = storm::cli::preprocessDdModelBisimulation(result.model->template as<storm::models::symbolic::Model<DdType, ValueType>>(), input,
                                                                     bisimulationSettings, mpi);
            result.changed = true;
        }
    }
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
PreprocessResult preprocessModel(std::shared_ptr<storm::models::ModelBase> const& model, cli::SymbolicInput const& input,
                                 storm::cli::ModelProcessingInformation const& mpi) {
    storm::utility::Stopwatch preprocessingWatch(true);

    PreprocessResult result(model, false);
    if (model->isSparseModel()) {
        result = storm::pars::preprocessSparseModel<ValueType>(result.model->as<storm::models::sparse::Model<ValueType>>(), input, mpi);
    } else {
        STORM_LOG_ASSERT(model->isSymbolicModel(), "Unexpected model type.");
        result = storm::pars::preprocessDdModel<DdType, ValueType>(result.model->as<storm::models::symbolic::Model<DdType, ValueType>>(), input, mpi);
    }

    if (result.changed) {
        STORM_PRINT_AND_LOG("\nTime for model preprocessing: " << preprocessingWatch << ".\n\n");
    }
    return result;
}

template<typename ValueType>
void verifyRegionWithSparseEngine(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, cli::SymbolicInput const& input,
                                  std::vector<storm::storage::ParameterRegion<ValueType>> const& regions,
                                  storm::api::MonotonicitySetting monotonicitySettings = storm::api::MonotonicitySetting()) {
    STORM_LOG_THROW(regions.size() == 1, storm::exceptions::NotSupportedException, "Region verification is supported for a (single) region only.");
    storm::storage::ParameterRegion<ValueType> const& region = regions.front();
    STORM_LOG_THROW(input.properties.size() == 1, storm::exceptions::NotSupportedException, "Region verification is supported for a (single) property only.");
    auto const& property = input.properties.front();

    auto const& rvs = storm::settings::getModule<storm::settings::modules::RegionVerificationSettings>();
    auto regionSettings = storm::settings::getModule<storm::settings::modules::RegionSettings>();

    auto engine = rvs.getRegionCheckEngine();
    bool graphPreserving = regionSettings.isAssumeGraphPreservingSet();

    STORM_LOG_THROW(graphPreserving || engine == storm::modelchecker::RegionCheckEngine::RobustParameterLifting, storm::exceptions::NotSupportedException,
                    "Selected region verification engine (--regionverif:engine) requires the assumption that the region is graph-preserving "
                    "(--assume-graph-preserving true).");

    auto splittingStrategy = storm::modelchecker::RegionSplittingStrategy();

    splittingStrategy.heuristic = rvs.getRegionSplittingHeuristic();
    splittingStrategy.estimateKind = rvs.getRegionSplittingEstimateMethod();
    if (rvs.isSplittingThresholdSet()) {
        splittingStrategy.maxSplitDimensions = rvs.getSplittingThreshold();
    }

    auto parsedDiscreteVars = storm::api::parseVariableList<ValueType>(regionSettings.getDiscreteVariablesString(), *model);
    std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> discreteVariables(parsedDiscreteVars.begin(), parsedDiscreteVars.end());

    storm::utility::Stopwatch watch(true);

    auto const& settings = storm::api::RefinementOptions<ValueType>{
        model,
        *(property.getRawFormula()),
        engine,
        splittingStrategy,
        monotonicitySettings,
        discreteVariables,
        true,  // allow model simplification
        graphPreserving,
        false  // preconditions not yet validated
    };

    if (storm::api::verifyRegion<ValueType>(settings, region)) {
        STORM_PRINT_AND_LOG("Formula is satisfied by all parameter instantiations.\n");
    } else {
        STORM_PRINT_AND_LOG("Formula is not satisfied by all parameter instantiations.\n");
    }
    STORM_PRINT_AND_LOG("Time for model checking: " << watch << ".\n");
}

template<typename ValueType>
void parameterSpacePartitioningWithSparseEngine(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, cli::SymbolicInput const& input,
                                                std::vector<storm::storage::ParameterRegion<ValueType>> const& regions,
                                                storm::api::MonotonicitySetting monotonicitySettings = storm::api::MonotonicitySetting(),
                                                uint64_t monThresh = 0) {
    STORM_LOG_ASSERT(!regions.empty(), "Can not analyze an empty set of regions.");
    STORM_LOG_THROW(regions.size() == 1, storm::exceptions::NotSupportedException, "Region refinement is not supported for multiple initial regions.");
    STORM_LOG_THROW(input.properties.size() == 1, storm::exceptions::NotSupportedException, "Region verification is supported for a (single) property only.");
    auto const& property = input.properties.front();

    auto parametricSettings = storm::settings::getModule<storm::settings::modules::ParametricSettings>();
    auto rvs = storm::settings::getModule<storm::settings::modules::RegionVerificationSettings>();
    auto partitionSettings = storm::settings::getModule<storm::settings::modules::PartitionSettings>();
    auto regionSettings = storm::settings::getModule<storm::settings::modules::RegionSettings>();

    ValueType refinementThreshold = storm::utility::convertNumber<ValueType>(partitionSettings.getCoverageThreshold());
    std::optional<uint64_t> optionalDepthLimit;
    if (partitionSettings.isDepthLimitSet()) {
        optionalDepthLimit = partitionSettings.getDepthLimit();
    }

    STORM_PRINT_AND_LOG('\n');
    STORM_PRINT_AND_LOG("Analyzing parameter region " << regions.front());

    auto engine = rvs.getRegionCheckEngine();
    STORM_PRINT_AND_LOG(" using " << engine);

    auto splittingStrategy = storm::modelchecker::RegionSplittingStrategy();

    splittingStrategy.heuristic = rvs.getRegionSplittingHeuristic();
    splittingStrategy.estimateKind = rvs.getRegionSplittingEstimateMethod();
    if (rvs.isSplittingThresholdSet()) {
        splittingStrategy.maxSplitDimensions = rvs.getSplittingThreshold();
    }

    bool graphPreserving = regionSettings.isAssumeGraphPreservingSet();

    auto parsedDiscreteVars = storm::api::parseVariableList<ValueType>(regionSettings.getDiscreteVariablesString(), *model);
    std::set<typename storm::storage::ParameterRegion<ValueType>::VariableType> discreteVariables(parsedDiscreteVars.begin(), parsedDiscreteVars.end());

    STORM_PRINT_AND_LOG(" and splitting heuristic " << splittingStrategy.heuristic);
    if (monotonicitySettings.useMonotonicity) {
        STORM_PRINT_AND_LOG(" with local monotonicity and");
    }

    STORM_PRINT_AND_LOG(" with iterative refinement until "
                        << (1.0 - partitionSettings.getCoverageThreshold()) * 100.0 << "% is covered."
                        << (partitionSettings.isDepthLimitSet() ? " Depth limit is " + std::to_string(partitionSettings.getDepthLimit()) + "." : "") << '\n');

    storm::cli::printModelCheckingProperty(property);
    storm::utility::Stopwatch watch(true);

    auto settings = storm::api::RefinementOptions<ValueType>{
        model,
        storm::api::createTask<ValueType>(property.getRawFormula(), true),
        engine,
        splittingStrategy,
        monotonicitySettings,
        discreteVariables,
        true,  // allow model simplification
        graphPreserving,
        false  // preconditions not yet validated
    };
    std::unique_ptr<storm::modelchecker::CheckResult> result = storm::api::checkAndRefineRegionWithSparseEngine<ValueType>(
        settings, regions.front(), refinementThreshold, optionalDepthLimit, storm::modelchecker::RegionResultHypothesis::Unknown, monThresh);
    watch.stop();
    printInitialStatesResult<ValueType>(result, &watch);

    if (parametricSettings.exportResultToFile()) {
        storm::api::exportRegionCheckResultToFile<ValueType>(result, parametricSettings.exportResultPath());
    }
}

void processInput(cli::SymbolicInput&& input, storm::cli::ModelProcessingInformation const& mpi) {
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    auto buildSettings = storm::settings::getModule<storm::settings::modules::BuildSettings>();
    auto parSettings = storm::settings::getModule<storm::settings::modules::ParametricSettings>();
    auto monSettings = storm::settings::getModule<storm::settings::modules::MonotonicitySettings>();
    auto sampleSettings = storm::settings::getModule<storm::settings::modules::SamplingSettings>();
    auto regionSettings = storm::settings::getModule<storm::settings::modules::RegionSettings>();

    STORM_LOG_THROW(mpi.engine == storm::utility::Engine::Sparse || mpi.engine == storm::utility::Engine::Hybrid || mpi.engine == storm::utility::Engine::Dd,
                    storm::exceptions::InvalidSettingsException, "The selected engine is not supported for parametric models.");
    STORM_LOG_THROW(parSettings.hasOperationModeBeenSet(), storm::exceptions::InvalidSettingsException, "An operation mode must be selected with --mode");
    std::shared_ptr<storm::models::ModelBase> model;
    if (!buildSettings.isNoBuildModelSet()) {
        model = storm::cli::buildModel(input, ioSettings, mpi);
    }

    STORM_LOG_THROW(model, storm::exceptions::InvalidSettingsException, "No input model.");
    if (model) {
        model->printModelInformationToStream(std::cout);
    }

    using ValueType = storm::RationalFunction;
    auto const DdType = storm::dd::DdType::Sylvan;
    STORM_LOG_THROW(model->supportsParameters(), storm::exceptions::UnexpectedException, "Expected a parametric model.");
    STORM_LOG_THROW(model->isSparseModel() || model->getDdType().value() == DdType, storm::exceptions::UnexpectedException,
                    "Expected type of model representation.");

    // If minimization is active and the model is parametric, parameters might be minimized away because they are inconsequential.
    // This is the set of all such inconsequential parameters.
    std::set<RationalFunctionVariable> omittedParameters;

    if (model) {
        auto preprocessingResult = storm::pars::preprocessModel<DdType, ValueType>(model, input, mpi);
        if (preprocessingResult.changed) {
            if (model->isOfType(models::ModelType::Dtmc) || model->isOfType(models::ModelType::Mdp)) {
                auto const previousParams = storm::models::sparse::getAllParameters(*model->template as<storm::models::sparse::Model<ValueType>>());
                auto const currentParams =
                    storm::models::sparse::getAllParameters(*(preprocessingResult.model)->template as<storm::models::sparse::Model<ValueType>>());
                for (auto const& variable : previousParams) {
                    if (!currentParams.count(variable)) {
                        omittedParameters.insert(variable);
                    }
                }
            }
            model = preprocessingResult.model;

            if (preprocessingResult.formulas) {
                std::vector<storm::jani::Property> newProperties;
                for (size_t i = 0; i < preprocessingResult.formulas.get().size(); ++i) {
                    auto formula = preprocessingResult.formulas.get().at(i);
                    STORM_LOG_ASSERT(i < input.properties.size(), "Index " << i << " greater than number of properties.");
                    storm::jani::Property property = input.properties.at(i);
                    newProperties.push_back(storm::jani::Property(property.getName(), formula, property.getUndefinedConstants(), property.getComment()));
                }
                input.properties = newProperties;
            }
            model->printModelInformationToStream(std::cout);
        }
    }

    std::vector<storm::storage::ParameterRegion<ValueType>> regions = parseRegions<ValueType>(model);
    if (!model) {
        return;
    } else {
        storm::cli::castAndApply(model, [&input](auto const& m) { storm::cli::exportModel(m, input); });
    }

    // TODO move this.
    storm::api::MonotonicitySetting monotonicitySettings(parSettings.isUseMonotonicitySet(), false, monSettings.isUsePLABoundsSet());
    uint64_t monThresh = monSettings.getMonotonicityThreshold();

    auto mode = parSettings.getOperationMode();
    if (mode == storm::pars::utility::ParametricMode::SolutionFunction) {
        STORM_LOG_INFO("Solution function mode started.");
        STORM_LOG_THROW(regions.empty(), storm::exceptions::InvalidSettingsException,
                        "Solution function computations cannot be restricted to specific regions");
        STORM_LOG_ERROR_COND(!regionSettings.isAssumeGraphPreservingSet(), "Solution function computations assume graph preservation.");

        if (model->isSparseModel()) {
            computeSolutionFunctionsWithSparseEngine(model->as<storm::models::sparse::Model<ValueType>>(), input);
        } else {
            computeSolutionFunctionsWithSymbolicEngine(model->as<storm::models::symbolic::Model<DdType, ValueType>>(), input);
        }
    } else if (mode == storm::pars::utility::ParametricMode::Monotonicity) {
        STORM_LOG_INFO("Monotonicity mode started.");
        STORM_LOG_THROW(model->isSparseModel(), storm::exceptions::InvalidSettingsException, "Monotonicity analysis is only supported on sparse models.");
        analyzeMonotonicity(model->as<storm::models::sparse::Model<ValueType>>(), input, regions);
    } else if (mode == storm::pars::utility::ParametricMode::Feasibility) {
        STORM_LOG_INFO("Feasibility mode started.");
        STORM_LOG_THROW(model->isSparseModel(), storm::exceptions::InvalidSettingsException, "Feasibility analysis is only supported on sparse models.");
        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(input.properties);
        STORM_LOG_THROW(formulas.size() == 1, storm::exceptions::InvalidSettingsException,
                        "Feasibility analysis is only supported for single-objective properties.");
        auto formula = formulas[0];
        storm::pars::performFeasibility<ValueType>(model->as<storm::models::sparse::Model<ValueType>>(),
                                                   createFeasibilitySynthesisTaskFromSettings(formula, regions), omittedParameters, monotonicitySettings);
    } else if (mode == storm::pars::utility::ParametricMode::Verification) {
        STORM_LOG_INFO("Verification mode started.");
        STORM_LOG_THROW(input.properties.size() == 1, storm::exceptions::InvalidSettingsException,
                        "Verification analysis is only supported for single-objective properties.");
        STORM_LOG_THROW(model->isSparseModel(), storm::exceptions::InvalidSettingsException, "Verification analysis is only supported on sparse models.");
        verifyRegionWithSparseEngine(model->as<storm::models::sparse::Model<ValueType>>(), input, regions, monotonicitySettings);

    } else if (mode == storm::pars::utility::ParametricMode::Partitioning) {
        STORM_LOG_INFO("Partition mode started.");
        STORM_LOG_THROW(model->isSparseModel(), storm::exceptions::InvalidSettingsException,
                        "Parameter space partitioning is only supported on sparse models.");
        STORM_LOG_THROW(regions.size() == 1, storm::exceptions::InvalidSettingsException, "Partitioning requires a (single) initial region.");

        // TODO Partition mode does not support monotonicity. This should generally be possible.
        // TODO here setting monotone parameters from the outside may actually be useful

        assert(!monotonicitySettings.useOnlyGlobalMonotonicity);
        assert(!monotonicitySettings.useBoundsFromPLA);
        storm::pars::parameterSpacePartitioningWithSparseEngine(model->as<storm::models::sparse::Model<ValueType>>(), input, regions, monotonicitySettings,
                                                                monThresh);
    } else if (mode == storm::pars::utility::ParametricMode::Sampling) {
        STORM_LOG_INFO("Sampling mode started.");
        STORM_LOG_THROW(model->isSparseModel(), storm::exceptions::InvalidSettingsException, "Sampling analysis is currently only supported on sparse models.");
        // TODO unclear why this only works for sparse models?

        std::string samplesAsString = sampleSettings.getSamples();
        SampleInformation<ValueType> samples;
        if (!samplesAsString.empty()) {
            samples = parseSamples<ValueType>(model, samplesAsString, sampleSettings.isSamplesAreGraphPreservingSet());
            samples.exact = sampleSettings.isSampleExactSet();
        }
        if (!samples.empty()) {
            STORM_LOG_TRACE("Sampling the model at given points.");

            if (sampleSettings.isSampleDerivativeSet()) {
                if (samples.exact) {
                    verifyPropertiesAtSamplePointsWithSparseEngineDerivatives<ValueType, storm::RationalNumber>(
                        model->as<storm::models::sparse::Model<ValueType>>(), input, samples);
                } else {
                    verifyPropertiesAtSamplePointsWithSparseEngineDerivatives<ValueType, double>(model->as<storm::models::sparse::Model<ValueType>>(), input,
                                                                                                 samples);
                }
            } else {
                if (samples.exact) {
                    verifyPropertiesAtSamplePointsWithSparseEngine<ValueType, storm::RationalNumber>(model->as<storm::models::sparse::Model<ValueType>>(),
                                                                                                     input, samples);
                } else {
                    verifyPropertiesAtSamplePointsWithSparseEngine<ValueType, double>(model->as<storm::models::sparse::Model<ValueType>>(), input, samples);
                }
            }
        }
    } else {
        STORM_LOG_ASSERT(false, "Unknown operation mode.");
    }
}

void processOptions() {
    auto coreSettings = storm::settings::getModule<storm::settings::modules::CoreSettings>();
    auto engine = coreSettings.getEngine();
    STORM_LOG_WARN_COND(
        engine != storm::utility::Engine::Dd || engine != storm::utility::Engine::Hybrid || coreSettings.getDdLibraryType() == storm::dd::DdType::Sylvan,
        "The selected DD library does not support parametric models. Switching to Sylvan...");

    // Parse and preprocess symbolic input (PRISM, JANI, properties, etc.)
    auto symbolicInput = storm::cli::parseSymbolicInput();
    storm::cli::ModelProcessingInformation mpi;
    std::tie(symbolicInput, mpi) = storm::cli::preprocessSymbolicInput(symbolicInput);
    mpi.ddType = storm::dd::DdType::Sylvan;
    mpi.buildValueType = storm::cli::ModelProcessingInformation::ValueType::Parametric;
    mpi.verificationValueType = storm::cli::ModelProcessingInformation::ValueType::Parametric;
    processInput(std::move(symbolicInput), mpi);
}
}  // namespace pars
}  // namespace storm

/*!
 * Main entry point of the executable storm-pars.
 */
int main(const int argc, const char** argv) {
    try {
        return storm::cli::process("Storm-pars", "storm-pars", storm::settings::initializeParsSettings, storm::pars::processOptions, argc, argv);
    } catch (storm::exceptions::BaseException const& exception) {
        STORM_LOG_ERROR("An exception caused Storm-pars to terminate. The message of the exception is: " << exception.what());
        return 1;
    } catch (std::exception const& exception) {
        STORM_LOG_ERROR("An unexpected exception occurred and caused Storm-pars to terminate. The message of this exception is: " << exception.what());
        return 2;
    }
}
