#pragma once

#include <filesystem>
#include <sstream>

#include "storm-cli-utilities/model-handling.h"

#include "storm-counterexamples/api/counterexamples.h"
#include "storm-gamebased-ar/api/verification.h"
#include "storm-parsers/parser/ExpressionParser.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/settings/modules/AbstractionSettings.h"
#include "storm/settings/modules/CounterexampleGeneratorSettings.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/SignalHandler.h"

namespace storm {
namespace cli {

inline void exportSymbolicInput(SymbolicInput const& input) {
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    if (input.model && input.model.get().isJaniModel()) {
        storm::storage::SymbolicModelDescription const& model = input.model.get();
        if (ioSettings.isExportJaniDotSet()) {
            storm::api::exportJaniModelAsDot(model.asJaniModel(), ioSettings.getExportJaniDotFilename());
        }
    }
}

inline void printComputingCounterexample(storm::jani::Property const& property) {
    STORM_PRINT("Computing counterexample for property " << *property.getRawFormula() << " ...\n");
}

inline void printCounterexample(std::shared_ptr<storm::counterexamples::Counterexample> const& counterexample, storm::utility::Stopwatch* watch = nullptr) {
    if (counterexample) {
        STORM_PRINT(*counterexample << '\n');
        if (watch) {
            STORM_PRINT("Time for computation: " << *watch << ".\n");
        }
    } else {
        STORM_PRINT(" failed.\n");
    }
}

template<typename ModelType>
    requires(!std::derived_from<ModelType, storm::models::sparse::Model<double>>)
inline void generateCounterexamples(std::shared_ptr<ModelType> const&, SymbolicInput const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Counterexample generation is not supported for this data-type.");
}

template<typename ModelType>
    requires(std::derived_from<ModelType, storm::models::sparse::Model<double>>)
inline void generateCounterexamples(std::shared_ptr<ModelType> const& sparseModel, SymbolicInput const& input) {
    using ValueType = typename ModelType::ValueType;

    for (auto& rewModel : sparseModel->getRewardModels()) {
        rewModel.second.reduceToStateBasedRewards(sparseModel->getTransitionMatrix(), true);
    }

    STORM_LOG_THROW(sparseModel->isOfType(storm::models::ModelType::Dtmc) || sparseModel->isOfType(storm::models::ModelType::Mdp),
                    storm::exceptions::NotSupportedException, "Counterexample is currently only supported for discrete-time models.");

    auto counterexampleSettings = storm::settings::getModule<storm::settings::modules::CounterexampleGeneratorSettings>();
    if (counterexampleSettings.isMinimalCommandSetGenerationSet()) {
        bool useMilp = counterexampleSettings.isUseMilpBasedMinimalCommandSetGenerationSet();
        for (auto const& property : input.properties) {
            std::shared_ptr<storm::counterexamples::Counterexample> counterexample;
            printComputingCounterexample(property);
            storm::utility::Stopwatch watch(true);
            if (useMilp) {
                STORM_LOG_THROW(sparseModel->isOfType(storm::models::ModelType::Mdp), storm::exceptions::NotSupportedException,
                                "Counterexample generation using MILP is currently only supported for MDPs.");
                counterexample = storm::api::computeHighLevelCounterexampleMilp(
                    input.model.get(), sparseModel->template as<storm::models::sparse::Mdp<ValueType>>(), property.getRawFormula());
            } else {
                STORM_LOG_THROW(sparseModel->isOfType(storm::models::ModelType::Dtmc) || sparseModel->isOfType(storm::models::ModelType::Mdp),
                                storm::exceptions::NotSupportedException,
                                "Counterexample generation using MaxSAT is currently only supported for discrete-time models.");

                if (sparseModel->isOfType(storm::models::ModelType::Dtmc)) {
                    counterexample = storm::api::computeHighLevelCounterexampleMaxSmt(
                        input.model.get(), sparseModel->template as<storm::models::sparse::Dtmc<ValueType>>(), property.getRawFormula());
                } else {
                    counterexample = storm::api::computeHighLevelCounterexampleMaxSmt(
                        input.model.get(), sparseModel->template as<storm::models::sparse::Mdp<ValueType>>(), property.getRawFormula());
                }
            }
            watch.stop();
            printCounterexample(counterexample, &watch);
        }
    } else if (counterexampleSettings.isShortestPathGenerationSet()) {
        for (auto const& property : input.properties) {
            std::shared_ptr<storm::counterexamples::Counterexample> counterexample;
            printComputingCounterexample(property);
            storm::utility::Stopwatch watch(true);
            STORM_LOG_THROW(sparseModel->isOfType(storm::models::ModelType::Dtmc), storm::exceptions::NotSupportedException,
                            "Counterexample generation using shortest paths is currently only supported for DTMCs.");
            counterexample = storm::api::computeKShortestPathCounterexample(sparseModel->template as<storm::models::sparse::Dtmc<ValueType>>(),
                                                                            property.getRawFormula(), counterexampleSettings.getShortestPathMaxK());
            watch.stop();
            printCounterexample(counterexample, &watch);
        }
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "The selected counterexample formalism is unsupported.");
    }
}

template<typename ValueType>
    requires(!storm::IsIntervalType<ValueType>)
void printFilteredResult(std::unique_ptr<storm::modelchecker::CheckResult> const& result, storm::modelchecker::FilterType ft) {
    if (result->isQuantitative()) {
        if (ft == storm::modelchecker::FilterType::VALUES) {
            STORM_PRINT(*result);
        } else {
            ValueType resultValue;
            switch (ft) {
                case storm::modelchecker::FilterType::SUM:
                    resultValue = result->asQuantitativeCheckResult<ValueType>().sum();
                    break;
                case storm::modelchecker::FilterType::AVG:
                    resultValue = result->asQuantitativeCheckResult<ValueType>().average();
                    break;
                case storm::modelchecker::FilterType::MIN:
                    resultValue = result->asQuantitativeCheckResult<ValueType>().getMin();
                    break;
                case storm::modelchecker::FilterType::MAX:
                    resultValue = result->asQuantitativeCheckResult<ValueType>().getMax();
                    break;
                case storm::modelchecker::FilterType::ARGMIN:
                case storm::modelchecker::FilterType::ARGMAX:
                    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Outputting states is not supported.");
                case storm::modelchecker::FilterType::EXISTS:
                case storm::modelchecker::FilterType::FORALL:
                case storm::modelchecker::FilterType::COUNT:
                    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Filter type only defined for qualitative results.");
                default:
                    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Unhandled filter type.");
            }
            if (storm::NumberTraits<ValueType>::IsExact && storm::utility::isConstant(resultValue)) {
                STORM_PRINT(resultValue << " (approx. " << storm::utility::convertNumber<double>(resultValue) << ")");
            } else {
                STORM_PRINT(resultValue);
            }
        }
    } else {
        switch (ft) {
            case storm::modelchecker::FilterType::VALUES:
                STORM_PRINT(*result << '\n');
                break;
            case storm::modelchecker::FilterType::EXISTS:
                STORM_PRINT(result->asQualitativeCheckResult().existsTrue());
                break;
            case storm::modelchecker::FilterType::FORALL:
                STORM_PRINT(result->asQualitativeCheckResult().forallTrue());
                break;
            case storm::modelchecker::FilterType::COUNT:
                STORM_PRINT(result->asQualitativeCheckResult().count());
                break;
            case storm::modelchecker::FilterType::ARGMIN:
            case storm::modelchecker::FilterType::ARGMAX:
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Outputting states is not supported.");
            case storm::modelchecker::FilterType::SUM:
            case storm::modelchecker::FilterType::AVG:
            case storm::modelchecker::FilterType::MIN:
            case storm::modelchecker::FilterType::MAX:
                STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException, "Filter type only defined for quantitative results.");
        }
    }
    STORM_PRINT('\n');
}

template<typename ValueType>
    requires(!storm::IsIntervalType<ValueType>)
void printResult(std::unique_ptr<storm::modelchecker::CheckResult> const& result, storm::logic::Formula const& filterStatesFormula,
                 storm::modelchecker::FilterType const& filterType, storm::utility::Stopwatch* watch = nullptr) {
    if (result) {
        std::stringstream ss;
        ss << "'" << filterStatesFormula << "'";
        STORM_PRINT((storm::utility::resources::isTerminate() ? "Result till abort" : "Result")
                    << " (for " << (filterStatesFormula.isInitialFormula() ? "initial" : ss.str()) << " states): ");
        printFilteredResult<ValueType>(result, filterType);
        if (watch) {
            STORM_PRINT("Time for model checking: " << *watch << ".\n");
        }
    } else {
        STORM_LOG_ERROR("Property is unsupported by selected engine/settings.\n");
    }
}

template<typename ValueType>
void printResult(std::unique_ptr<storm::modelchecker::CheckResult> const& result, storm::jani::Property const& property,
                 storm::utility::Stopwatch* watch = nullptr) {
    printResult<ValueType>(result, *property.getFilter().getStatesFormula(), property.getFilter().getFilterType(), watch);
}

using VerificationCallbackType = std::function<std::unique_ptr<storm::modelchecker::CheckResult>(std::shared_ptr<storm::logic::Formula const> const& formula,
                                                                                                 std::shared_ptr<storm::logic::Formula const> const& states)>;
using PostprocessingCallbackType = std::function<void(std::unique_ptr<storm::modelchecker::CheckResult> const&)>;

struct PostprocessingIdentity {
    void operator()(std::unique_ptr<storm::modelchecker::CheckResult> const&) {
        // Intentionally left empty.
    }
};

/*!
 * Verifies the given formula plus a filter formula to identify relevant states and warns the user in case of issues
 * @param formula the formula to check
 * @param statesFilter a second formula that identifies the relevant states (needs to be qualitative)
 * @param verificationCallback Function to perform the actual verification task for a given formula plus a filter formula to identify relevant states
 */
template<typename ValueType>
std::unique_ptr<storm::modelchecker::CheckResult> verifyProperty(std::shared_ptr<storm::logic::Formula const> const& formula,
                                                                 std::shared_ptr<storm::logic::Formula const> const& statesFilter,
                                                                 VerificationCallbackType const& verificationCallback) {
    auto transformationSettings = storm::settings::getModule<storm::settings::modules::TransformationSettings>();

    try {
        if constexpr (storm::IsIntervalType<ValueType>) {
            STORM_LOG_ASSERT(!transformationSettings.isChainEliminationSet() && !transformationSettings.isToNondeterministicModelSet(),
                             "Unsupported transformation has been invoked.");
            return verificationCallback(formula, statesFilter);
        }
        if (transformationSettings.isChainEliminationSet() && !storm::transformer::NonMarkovianChainTransformer<ValueType>::preservesFormula(*formula)) {
            STORM_LOG_WARN("Property is not preserved by elimination of non-markovian states.");
        } else if (transformationSettings.isToDiscreteTimeModelSet()) {
            auto transformedFormula = storm::api::checkAndTransformContinuousToDiscreteTimeFormula<ValueType>(*formula);
            auto transformedStatesFilter = storm::api::checkAndTransformContinuousToDiscreteTimeFormula<ValueType>(*statesFilter);
            if (transformedFormula && transformedStatesFilter) {
                // invoke verification algorithm on transformed formulas
                return verificationCallback(transformedFormula, transformedStatesFilter);
            } else {
                STORM_LOG_WARN("Property is not preserved by transformation to discrete time model.");
            }
        } else {
            // invoke verification algorithm on given formulas
            return verificationCallback(formula, statesFilter);
        }
    } catch (storm::exceptions::BaseException const& ex) {
        STORM_LOG_WARN("Cannot handle property: " << ex.what());
    }
    return nullptr;
}

/*!
 * Verifies all (potentially preprocessed) properties given in `input`.
 * @param input Where the properties are read from
 * @param verificationCallback Function to perform the actual verification task for a given formula plus a filter formula to identify relevant states
 * @param postprocessingCallback Function that processes the verification result, such as e.g. output to a file
 */
template<typename ValueType>
void verifyProperties(SymbolicInput const& input, VerificationCallbackType const& verificationCallback,
                      PostprocessingCallbackType const& postprocessingCallback = PostprocessingIdentity()) {
    auto const& properties = input.preprocessedProperties ? input.preprocessedProperties.get() : input.properties;
    for (auto const& property : properties) {
        printModelCheckingProperty(property);
        storm::utility::Stopwatch watch(true);
        auto result = verifyProperty<ValueType>(property.getRawFormula(), property.getFilter().getStatesFormula(), verificationCallback);
        watch.stop();
        if (result) {
            postprocessingCallback(result);
        }
        printResult<storm::IntervalBaseType<ValueType>>(result, property, &watch);
    }
}

/*!
 * Computes values for each state (such as the steady-state probability distribution).
 * If one or more formulas are given, they serve as filter to identify which states are relevant.
 * @param description A string description of what is to be computed used for sensible command line output.
 * @param computationCallback A function that performs the actual computation of the state values
 * @param input Where the properties are read from
 * @param verificationCallback Function to perform the actual verification task for a given formula plus a filter formula to identify relevant states
 * @param postprocessingCallback Function that processes the verification result, such as e.g. output to a file
 */
template<typename ValueType>
void computeStateValues(std::string const& description, std::function<std::unique_ptr<storm::modelchecker::CheckResult>()> const& computationCallback,
                        SymbolicInput const& input, VerificationCallbackType const& verificationCallback,
                        PostprocessingCallbackType const& postprocessingCallback = PostprocessingIdentity()) {
    // First compute the state values for all the states by invoking the computationCallback
    storm::utility::Stopwatch watch(true);
    STORM_PRINT("\nComputing " << description << " ...\n");
    std::unique_ptr<storm::modelchecker::CheckResult> result;
    try {
        result = computationCallback();
    } catch (storm::exceptions::BaseException const& ex) {
        STORM_LOG_ERROR("Cannot compute " << description << ": " << ex.what());
    }
    if (!result) {
        STORM_LOG_ERROR("Computation had no result.");
        return;
    }
    // Now process the (potentially filtered) result
    if (input.properties.empty()) {
        // Do not apply any filtering, consider result for *all* states
        postprocessingCallback(result);
        printResult<ValueType>(result, *storm::logic::Formula::getTrueFormula(), storm::modelchecker::FilterType::VALUES, &watch);
    } else {
        // Each property identifies a subset of states to which we restrict (aka filter) the state-value result to
        auto const& properties = input.preprocessedProperties ? input.preprocessedProperties.get() : input.properties;
        for (uint64_t propertyIndex = 0; propertyIndex < properties.size(); ++propertyIndex) {
            auto const& property = properties[propertyIndex];
            // As the property serves as filter, it should (a) be qualitative and should (b) not consider a filter itself.
            if (!property.getRawFormula()->hasQualitativeResult()) {
                STORM_LOG_ERROR("Property '" << *property.getRawFormula()
                                             << "' can not be used for filtering states as it does not have a qualitative result.");
                continue;
            }

            // Invoke verification algorithm on filtering property
            auto propertyFilter = verifyProperty<ValueType>(property.getRawFormula(), storm::logic::Formula::getTrueFormula(), verificationCallback);

            if (propertyFilter) {
                // Filter and process result
                std::unique_ptr<storm::modelchecker::CheckResult> filteredResult = result->clone();
                filteredResult->filter(propertyFilter->asQualitativeCheckResult());
                postprocessingCallback(filteredResult);
                printResult<ValueType>(filteredResult, *property.getRawFormula(), property.getFilter().getFilterType(),
                                       propertyIndex == properties.size() - 1 ? &watch : nullptr);
            }
        }
    }
}

inline std::vector<storm::expressions::Expression> parseConstraints(storm::expressions::ExpressionManager const& expressionManager,
                                                                    std::string const& constraintsString) {
    std::vector<storm::expressions::Expression> constraints;

    std::vector<std::string> constraintsAsStrings;
    boost::split(constraintsAsStrings, constraintsString, boost::is_any_of(","));

    storm::parser::ExpressionParser expressionParser(expressionManager);
    std::unordered_map<std::string, storm::expressions::Expression> variableMapping;
    for (auto const& variableTypePair : expressionManager) {
        variableMapping[variableTypePair.first.getName()] = variableTypePair.first;
    }
    expressionParser.setIdentifierMapping(variableMapping);

    for (auto const& constraintString : constraintsAsStrings) {
        if (constraintString.empty()) {
            continue;
        }

        storm::expressions::Expression constraint = expressionParser.parseFromString(constraintString);
        STORM_LOG_TRACE("Adding special (user-provided) constraint " << constraint << ".");
        constraints.emplace_back(constraint);
    }

    return constraints;
}

inline std::vector<std::vector<storm::expressions::Expression>> parseInjectedRefinementPredicates(
    storm::expressions::ExpressionManager const& expressionManager, std::string const& refinementPredicatesString) {
    std::vector<std::vector<storm::expressions::Expression>> injectedRefinementPredicates;

    storm::parser::ExpressionParser expressionParser(expressionManager);
    std::unordered_map<std::string, storm::expressions::Expression> variableMapping;
    for (auto const& variableTypePair : expressionManager) {
        variableMapping[variableTypePair.first.getName()] = variableTypePair.first;
    }
    expressionParser.setIdentifierMapping(variableMapping);

    std::vector<std::string> predicateGroupsAsStrings;
    boost::split(predicateGroupsAsStrings, refinementPredicatesString, boost::is_any_of(";"));

    if (!predicateGroupsAsStrings.empty()) {
        for (auto const& predicateGroupString : predicateGroupsAsStrings) {
            if (predicateGroupString.empty()) {
                continue;
            }

            std::vector<std::string> predicatesAsStrings;
            boost::split(predicatesAsStrings, predicateGroupString, boost::is_any_of(":"));

            if (!predicatesAsStrings.empty()) {
                injectedRefinementPredicates.emplace_back();
                for (auto const& predicateString : predicatesAsStrings) {
                    storm::expressions::Expression predicate = expressionParser.parseFromString(predicateString);
                    STORM_LOG_TRACE("Adding special (user-provided) refinement predicate " << predicateString << ".");
                    injectedRefinementPredicates.back().emplace_back(predicate);
                }

                STORM_LOG_THROW(!injectedRefinementPredicates.back().empty(), storm::exceptions::InvalidArgumentException,
                                "Expecting non-empty list of predicates to inject for each (mentioned) refinement step.");

                // Finally reverse the list, because we take the predicates from the back.
                std::reverse(injectedRefinementPredicates.back().begin(), injectedRefinementPredicates.back().end());
            }
        }

        // Finally reverse the list, because we take the predicates from the back.
        std::reverse(injectedRefinementPredicates.begin(), injectedRefinementPredicates.end());
    }

    return injectedRefinementPredicates;
}

template<storm::dd::DdType DdType, typename ValueType>
void verifyWithAbstractionRefinementEngine(SymbolicInput const& input, ModelProcessingInformation const& mpi) {
    STORM_LOG_ASSERT(input.model, "Expected symbolic model description.");
    storm::settings::modules::AbstractionSettings const& abstractionSettings = storm::settings::getModule<storm::settings::modules::AbstractionSettings>();
    storm::gbar::api::AbstractionRefinementOptions options(
        parseConstraints(input.model->getManager(), abstractionSettings.getConstraintString()),
        parseInjectedRefinementPredicates(input.model->getManager(), abstractionSettings.getInjectedRefinementPredicates()));

    verifyProperties<ValueType>(input, [&input, &options, &mpi](std::shared_ptr<storm::logic::Formula const> const& formula,
                                                                std::shared_ptr<storm::logic::Formula const> const& states) {
        STORM_LOG_THROW(states->isInitialFormula(), storm::exceptions::NotSupportedException, "Abstraction-refinement can only filter initial states.");
        return storm::gbar::api::verifyWithAbstractionRefinementEngine<DdType, ValueType>(mpi.env, input.model.get(),
                                                                                          storm::api::createTask<ValueType>(formula, true), options);
    });
}

template<typename ValueType>
void verifyWithExplorationEngine(SymbolicInput const& input, ModelProcessingInformation const& mpi) {
    STORM_LOG_ASSERT(input.model, "Expected symbolic model description.");
    STORM_LOG_THROW((std::is_same<ValueType, double>::value), storm::exceptions::NotSupportedException,
                    "Exploration does not support other data-types than floating points.");
    verifyProperties<ValueType>(
        input, [&input, &mpi](std::shared_ptr<storm::logic::Formula const> const& formula, std::shared_ptr<storm::logic::Formula const> const& states) {
            STORM_LOG_THROW(states->isInitialFormula(), storm::exceptions::NotSupportedException, "Exploration can only filter initial states.");
            return storm::api::verifyWithExplorationEngine<ValueType>(mpi.env, input.model.get(), storm::api::createTask<ValueType>(formula, true));
        });
}

template<typename ValueType>
void verifyModel(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& sparseModel, SymbolicInput const& input,
                 ModelProcessingInformation const& mpi) {
    auto const& ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    auto verificationCallback = [&sparseModel, &ioSettings, &mpi](std::shared_ptr<storm::logic::Formula const> const& formula,
                                                                  std::shared_ptr<storm::logic::Formula const> const& states) {
        auto createTask = [&ioSettings](auto const& f, bool onlyInitialStates) {
            if constexpr (storm::IsIntervalType<ValueType>) {
                STORM_LOG_THROW(ioSettings.isUncertaintyResolutionModeSet(), storm::exceptions::InvalidSettingsException,
                                "Uncertainty resolution mode required for uncertain (interval) models.");
                return storm::api::createTask<ValueType>(f, storm::solver::convert(ioSettings.getUncertaintyResolutionMode()), onlyInitialStates);
            } else {
                (void)ioSettings;  // suppress unused lambda capture warning. [[maybe_unused]] doesn't work for lambda captures.
                return storm::api::createTask<ValueType>(f, onlyInitialStates);
            }
        };
        bool const filterForInitialStates = states->isInitialFormula();
        auto task = createTask(formula, filterForInitialStates);
        if (ioSettings.isExportSchedulerSet()) {
            task.setProduceSchedulers(true);
        }
        std::unique_ptr<storm::modelchecker::CheckResult> result = storm::api::verifyWithSparseEngine<ValueType>(mpi.env, sparseModel, task);

        std::unique_ptr<storm::modelchecker::CheckResult> filter;
        if (filterForInitialStates) {
            using SolutionType = storm::IntervalBaseType<ValueType>;
            filter = std::make_unique<storm::modelchecker::ExplicitQualitativeCheckResult<SolutionType>>(sparseModel->getInitialStates());
        } else if (!states->isTrueFormula()) {  // No need to apply filter if it is the formula 'true'
            filter = storm::api::verifyWithSparseEngine<ValueType>(mpi.env, sparseModel, createTask(states, false));
        }
        if (result && filter) {
            result->filter(filter->asQualitativeCheckResult());
        }
        return result;
    };
    uint64_t exportCount = 0;  // this number will be prepended to the export file name of schedulers and/or check results in case of multiple properties.
    auto postprocessingCallback = [&sparseModel, &ioSettings, &input, &exportCount](std::unique_ptr<storm::modelchecker::CheckResult> const& result) {
        // Scheduler export
        STORM_LOG_WARN_COND(!ioSettings.isExportSchedulerSet() || result->hasScheduler(), "Scheduler requested but could not be generated.");
        if (ioSettings.isExportSchedulerSet() && result->hasScheduler()) {
            std::filesystem::path schedulerExportPath = ioSettings.getExportSchedulerFilename();
            if (exportCount > 0) {
                STORM_LOG_WARN("Prepending " << exportCount << " to scheduler file name for this property because there are multiple properties.");
                schedulerExportPath.replace_filename(std::to_string(exportCount) + schedulerExportPath.filename().string());
            }
            STORM_PRINT_AND_LOG("Exporting scheduler ... ");
            if (input.model) {
                STORM_LOG_WARN_COND(sparseModel->hasStateValuations(),
                                    "No information of state valuations available. The scheduler output will use internal state ids. You might be "
                                    "interested in building the model with state valuations using --buildstateval.");
                STORM_LOG_WARN_COND(
                    sparseModel->hasChoiceLabeling() || sparseModel->hasChoiceOrigins(),
                    "No symbolic choice information is available. The scheduler output will use internal choice ids. You might be interested in "
                    "building the model with choice labels or choice origins using --buildchoicelab or --buildchoiceorig.");
                STORM_LOG_WARN_COND(sparseModel->hasChoiceLabeling() && !sparseModel->hasChoiceOrigins(),
                                    "Only partial choice information is available. You might want to build the model with choice origins using "
                                    "--buildchoicelab or --buildchoiceorig.");
            }
            if (result->isExplicitQuantitativeCheckResult()) {
                if constexpr (storm::IsIntervalType<ValueType>) {
                    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Scheduler export for interval models is not supported.");
                } else {
                    storm::api::exportScheduler(sparseModel, result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler(),
                                                schedulerExportPath.string());
                }
            } else if (result->isExplicitParetoCurveCheckResult()) {
                if constexpr (std::is_same_v<ValueType, storm::RationalFunction> || storm::IsIntervalType<ValueType>) {
                    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Scheduler export for models of this value type is not supported.");
                } else {
                    auto const& paretoRes = result->template asExplicitParetoCurveCheckResult<ValueType>();
                    storm::api::exportParetoScheduler(sparseModel, paretoRes.getPoints(), paretoRes.getSchedulers(), schedulerExportPath.string());
                }
            } else if (result->isExplicitQualitativeCheckResult()) {
                if constexpr (storm::IsIntervalType<ValueType>) {
                    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Scheduler export for interval models is not supported.");
                } else {
                    storm::api::exportScheduler(sparseModel, result->template asExplicitQualitativeCheckResult<ValueType>().getScheduler(),
                                                schedulerExportPath.string());
                }
            } else {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Scheduler export not supported for this value type.");
            }
        }

        // Result export
        if (ioSettings.isExportCheckResultSet()) {
            if constexpr (storm::IsIntervalType<ValueType>) {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Result export for interval models is not supported.");
            } else {
                std::filesystem::path resultExportPath = ioSettings.getExportCheckResultFilename();
                if (exportCount > 0) {
                    STORM_LOG_WARN("Prepending " << exportCount << " to result file name for this property because there are multiple properties.");
                    resultExportPath.replace_filename(std::to_string(exportCount) + resultExportPath.filename().string());
                }
                STORM_LOG_WARN_COND(sparseModel->hasStateValuations(),
                                    "No information of state valuations available. The result output will use internal state ids. You might be interested in "
                                    "building the model with state valuations using --buildstateval.");
                storm::api::exportCheckResultToJson(sparseModel, result, resultExportPath);
            }
        }
        ++exportCount;
    };
    if (!(ioSettings.isComputeSteadyStateDistributionSet() || ioSettings.isComputeExpectedVisitingTimesSet())) {
        verifyProperties<ValueType>(input, verificationCallback, postprocessingCallback);
    }
    if (ioSettings.isComputeSteadyStateDistributionSet()) {
        if constexpr (storm::IsIntervalType<ValueType>) {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing steady state distribution is not supported for interval models.");
        } else {
            computeStateValues<ValueType>(
                "steady-state probabilities",
                [&mpi, &sparseModel]() { return storm::api::computeSteadyStateDistributionWithSparseEngine<ValueType>(mpi.env, sparseModel); }, input,
                verificationCallback, postprocessingCallback);
        }
    }
    if (ioSettings.isComputeExpectedVisitingTimesSet()) {
        if constexpr (storm::IsIntervalType<ValueType>) {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Computing expected visiting times is not supported for interval models.");
        } else {
            computeStateValues<ValueType>(
                "expected visiting times",
                [&mpi, &sparseModel]() { return storm::api::computeExpectedVisitingTimesWithSparseEngine<ValueType>(mpi.env, sparseModel); }, input,
                verificationCallback, postprocessingCallback);
        }
    }
}

template<storm::dd::DdType DdType, typename ValueType>
void verifyWithHybridEngine(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& symbolicModel, SymbolicInput const& input,
                            ModelProcessingInformation const& mpi) {
    verifyProperties<ValueType>(
        input, [&symbolicModel, &mpi](std::shared_ptr<storm::logic::Formula const> const& formula, std::shared_ptr<storm::logic::Formula const> const& states) {
            bool filterForInitialStates = states->isInitialFormula();
            auto task = storm::api::createTask<ValueType>(formula, filterForInitialStates);

            std::unique_ptr<storm::modelchecker::CheckResult> result = storm::api::verifyWithHybridEngine<DdType, ValueType>(mpi.env, symbolicModel, task);

            std::unique_ptr<storm::modelchecker::CheckResult> filter;
            if (filterForInitialStates) {
                filter = std::make_unique<storm::modelchecker::SymbolicQualitativeCheckResult<DdType>>(symbolicModel->getReachableStates(),
                                                                                                       symbolicModel->getInitialStates());
            } else if (!states->isTrueFormula()) {  // No need to apply filter if it is the formula 'true'
                filter = storm::api::verifyWithHybridEngine<DdType, ValueType>(mpi.env, symbolicModel, storm::api::createTask<ValueType>(states, false));
            }
            if (result && filter) {
                result->filter(filter->asQualitativeCheckResult());
            }
            return result;
        });
}

template<storm::dd::DdType DdType, typename ValueType>
void verifyWithDdEngine(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& symbolicModel, SymbolicInput const& input,
                        ModelProcessingInformation const& mpi) {
    verifyProperties<ValueType>(
        input, [&symbolicModel, &mpi](std::shared_ptr<storm::logic::Formula const> const& formula, std::shared_ptr<storm::logic::Formula const> const& states) {
            bool filterForInitialStates = states->isInitialFormula();
            auto task = storm::api::createTask<ValueType>(formula, filterForInitialStates);

            std::unique_ptr<storm::modelchecker::CheckResult> result =
                storm::api::verifyWithDdEngine<DdType, ValueType>(mpi.env, symbolicModel, storm::api::createTask<ValueType>(formula, true));

            std::unique_ptr<storm::modelchecker::CheckResult> filter;
            if (filterForInitialStates) {
                filter = std::make_unique<storm::modelchecker::SymbolicQualitativeCheckResult<DdType>>(symbolicModel->getReachableStates(),
                                                                                                       symbolicModel->getInitialStates());
            } else if (!states->isTrueFormula()) {  // No need to apply filter if it is the formula 'true'
                filter = storm::api::verifyWithDdEngine<DdType, ValueType>(mpi.env, symbolicModel, storm::api::createTask<ValueType>(states, false));
            }
            if (result && filter) {
                result->filter(filter->asQualitativeCheckResult());
            }
            return result;
        });
}

template<storm::dd::DdType DdType, typename ValueType>
void verifyWithAbstractionRefinementEngine(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& symbolicModel, SymbolicInput const& input,
                                           ModelProcessingInformation const& mpi) {
    verifyProperties<ValueType>(
        input, [&symbolicModel, &mpi](std::shared_ptr<storm::logic::Formula const> const& formula, std::shared_ptr<storm::logic::Formula const> const& states) {
            STORM_LOG_THROW(states->isInitialFormula(), storm::exceptions::NotSupportedException, "Abstraction-refinement can only filter initial states.");
            return storm::gbar::api::verifyWithAbstractionRefinementEngine<DdType, ValueType>(mpi.env, symbolicModel,
                                                                                              storm::api::createTask<ValueType>(formula, true));
        });
}

template<storm::dd::DdType DdType, typename ValueType>
typename std::enable_if<DdType != storm::dd::DdType::CUDD || std::is_same<ValueType, double>::value, void>::type verifyModel(
    std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& symbolicModel, SymbolicInput const& input,
    ModelProcessingInformation const& mpi) {
    if (mpi.engine == storm::utility::Engine::Hybrid) {
        verifyWithHybridEngine<DdType, ValueType>(symbolicModel, input, mpi);
    } else if (mpi.engine == storm::utility::Engine::Dd) {
        verifyWithDdEngine<DdType, ValueType>(symbolicModel, input, mpi);
    } else {
        verifyWithAbstractionRefinementEngine<DdType, ValueType>(symbolicModel, input, mpi);
    }
}

template<storm::dd::DdType DdType, typename ValueType>
typename std::enable_if<DdType == storm::dd::DdType::CUDD && !std::is_same<ValueType, double>::value, void>::type verifySymbolicModel(
    std::shared_ptr<storm::models::ModelBase> const&, SymbolicInput const&, ModelProcessingInformation const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "CUDD does not support the selected data-type.");
}

inline void processInput(SymbolicInput const& input, ModelProcessingInformation const& mpi) {
    auto abstractionSettings = storm::settings::getModule<storm::settings::modules::AbstractionSettings>();
    auto counterexampleSettings = storm::settings::getModule<storm::settings::modules::CounterexampleGeneratorSettings>();

    // For several engines, no model building step is performed, but the verification is started right away.
    if (mpi.engine == storm::utility::Engine::AbstractionRefinement &&
        abstractionSettings.getAbstractionRefinementMethod() == storm::settings::modules::AbstractionSettings::Method::Games) {
        applyDdLibValueType(mpi.ddType, mpi.verificationValueType,
                            [&input, &mpi]<storm::dd::DdType DD, typename VT>() { verifyWithAbstractionRefinementEngine<DD, VT>(input, mpi); });
    } else if (mpi.engine == storm::utility::Engine::Exploration) {
        applyValueType(mpi.verificationValueType, [&input, &mpi]<typename VT>() { verifyWithExplorationEngine<VT>(input, mpi); });
    } else {
        std::shared_ptr<storm::models::ModelBase> model = buildPreprocessExportModel(input, mpi);
        if (model) {
            if (counterexampleSettings.isCounterexampleSet()) {
                castAndApply(model, [&input](auto const& m) { generateCounterexamples(m, input); });
            } else {
                castAndApply(model, [&input, &mpi](auto const& m) { verifyModel(m, input, mpi); });
            }
        }
    }
}
}  // namespace cli
}  // namespace storm
