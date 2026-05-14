#pragma once

#include <filesystem>
#include <type_traits>

#include "storm/api/storm.h"

#include "AutomaticSettings.h"
#include "storm-counterexamples/api/counterexamples.h"
#include "storm-gamebased-ar/api/verification.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/ExpressionParser.h"
#include "storm/builder/BuilderType.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/OptionParserException.h"
#include "storm/io/file.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/modelchecker/results/SymbolicQualitativeCheckResult.h"
#include "storm/models/ModelBase.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/symbolic/MarkovAutomaton.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/AbstractionSettings.h"
#include "storm/settings/modules/BuildSettings.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/DebugSettings.h"
#include "storm/settings/modules/HintSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/ModelCheckerSettings.h"
#include "storm/settings/modules/MultiObjectiveSettings.h"
#include "storm/settings/modules/ResourceSettings.h"
#include "storm/settings/modules/SylvanSettings.h"
#include "storm/settings/modules/TransformationSettings.h"
#include "storm/storage/Qvbs.h"
#include "storm/storage/SymbolicModelDescription.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/localeliminator/AutomaticAction.h"
#include "storm/storage/jani/localeliminator/JaniLocalEliminator.h"
#include "storm/utility/Engine.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/initialize.h"
#include "storm/utility/macros.h"

namespace storm {
namespace cli {

struct SymbolicInput {
    // The symbolic model description.
    boost::optional<storm::storage::SymbolicModelDescription> model;

    // The original properties to check.
    std::vector<storm::jani::Property> properties;

    // The preprocessed properties to check (in case they needed amendment).
    boost::optional<std::vector<storm::jani::Property>> preprocessedProperties;
};

inline void parseSymbolicModelDescription(storm::settings::modules::IOSettings const& ioSettings, SymbolicInput& input) {
    auto buildSettings = storm::settings::getModule<storm::settings::modules::BuildSettings>();
    if (ioSettings.isPrismOrJaniInputSet()) {
        storm::utility::Stopwatch modelParsingWatch(true);
        if (ioSettings.isPrismInputSet()) {
            input.model =
                storm::api::parseProgram(ioSettings.getPrismInputFilename(), buildSettings.isPrismCompatibilityEnabled(), !buildSettings.isNoSimplifySet());
        } else {
            boost::optional<std::vector<std::string>> propertyFilter;
            if (ioSettings.isJaniPropertiesSet()) {
                if (ioSettings.areJaniPropertiesSelected()) {
                    propertyFilter = ioSettings.getSelectedJaniProperties();
                } else {
                    propertyFilter = boost::none;
                }
            } else {
                propertyFilter = std::vector<std::string>();
            }
            auto janiInput = storm::api::parseJaniModel(ioSettings.getJaniInputFilename(), propertyFilter);
            input.model = std::move(janiInput.first);
            if (ioSettings.isJaniPropertiesSet()) {
                input.properties = std::move(janiInput.second);
            }
        }
        modelParsingWatch.stop();
        STORM_PRINT("Time for model input parsing: " << modelParsingWatch << ".\n\n");
    }
}

inline void parseProperties(storm::settings::modules::IOSettings const& ioSettings, SymbolicInput& input,
                            boost::optional<std::set<std::string>> const& propertyFilter) {
    if (ioSettings.isPropertySet()) {
        std::vector<storm::jani::Property> newProperties;
        if (input.model) {
            newProperties = storm::api::parsePropertiesForSymbolicModelDescription(ioSettings.getProperty(), input.model.get(), propertyFilter);
        } else {
            newProperties = storm::api::parseProperties(ioSettings.getProperty(), propertyFilter);
        }

        input.properties.insert(input.properties.end(), newProperties.begin(), newProperties.end());
    }
}

inline SymbolicInput parseSymbolicInputQvbs(storm::settings::modules::IOSettings const& ioSettings) {
    // Parse the model input
    SymbolicInput input;
    storm::storage::QvbsBenchmark benchmark(ioSettings.getQvbsModelName());
    STORM_PRINT_AND_LOG(benchmark.getInfo(ioSettings.getQvbsInstanceIndex(), ioSettings.getQvbsPropertyFilter()));
    storm::utility::Stopwatch modelParsingWatch(true);
    auto janiInput = storm::api::parseJaniModel(benchmark.getJaniFile(ioSettings.getQvbsInstanceIndex()), ioSettings.getQvbsPropertyFilter());
    input.model = std::move(janiInput.first);
    input.properties = std::move(janiInput.second);
    modelParsingWatch.stop();
    STORM_PRINT("Time for model input parsing: " << modelParsingWatch << ".\n\n");

    // Parse additional properties
    boost::optional<std::set<std::string>> propertyFilter = storm::api::parsePropertyFilter(ioSettings.getPropertyFilter());
    parseProperties(ioSettings, input, propertyFilter);

    // Substitute constant definitions
    auto constantDefinitions = input.model.get().parseConstantDefinitions(benchmark.getConstantDefinition(ioSettings.getQvbsInstanceIndex()));
    input.model = input.model.get().preprocess(constantDefinitions);
    if (!input.properties.empty()) {
        input.properties = storm::api::substituteConstantsInProperties(input.properties, constantDefinitions);
    }

    return input;
}

inline SymbolicInput parseSymbolicInput() {
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    if (ioSettings.isQvbsInputSet()) {
        return parseSymbolicInputQvbs(ioSettings);
    } else {
        // Parse the property filter, if any is given.
        boost::optional<std::set<std::string>> propertyFilter = storm::api::parsePropertyFilter(ioSettings.getPropertyFilter());

        SymbolicInput input;
        parseSymbolicModelDescription(ioSettings, input);
        parseProperties(ioSettings, input, propertyFilter);
        return input;
    }
}

struct ModelProcessingInformation {
    // The engine to use
    storm::utility::Engine engine;

    // If set, bisimulation will be applied.
    bool applyBisimulation;

    // If set, a transformation to Jani will be enforced
    bool transformToJani;

    // Which data type is to be used for numbers ...
    enum class ValueType { FinitePrecision, Exact, Parametric };
    ValueType buildValueType;         // ... during model building
    ValueType verificationValueType;  // ... during model verification

    // The Dd library to be used
    storm::dd::DdType ddType;

    // The environment used during model checking
    storm::Environment env;

    // A flag which is set to true, if the settings were detected to be compatible.
    // If this is false, it could be that the query can not be handled.
    bool isCompatible;
};

inline void getModelProcessingInformationAutomatic(SymbolicInput const& input, ModelProcessingInformation& mpi) {
    auto hints = storm::settings::getModule<storm::settings::modules::HintSettings>();

    STORM_LOG_THROW(input.model.is_initialized(), storm::exceptions::InvalidArgumentException, "Automatic engine requires a JANI input model.");
    STORM_LOG_THROW(input.model->isJaniModel(), storm::exceptions::InvalidArgumentException, "Automatic engine requires a JANI input model.");
    std::vector<storm::jani::Property> const& properties =
        input.preprocessedProperties.is_initialized() ? input.preprocessedProperties.get() : input.properties;
    STORM_LOG_THROW(!properties.empty(), storm::exceptions::InvalidArgumentException, "Automatic engine requires a property.");
    STORM_LOG_WARN_COND(properties.size() == 1,
                        "Automatic engine does not support decisions based on multiple properties. Only the first property will be considered.");

    storm::utility::AutomaticSettings as;
    if (hints.isNumberStatesSet()) {
        as.predict(input.model->asJaniModel(), properties.front(), hints.getNumberStates());
    } else {
        as.predict(input.model->asJaniModel(), properties.front());
    }

    mpi.engine = as.getEngine();
    if (as.enableBisimulation()) {
        mpi.applyBisimulation = true;
    }
    if (as.enableExact() && mpi.verificationValueType == ModelProcessingInformation::ValueType::FinitePrecision) {
        mpi.verificationValueType = ModelProcessingInformation::ValueType::Exact;
    }
    STORM_PRINT_AND_LOG("Automatic engine picked the following settings: \n"
                        << "\tengine=" << mpi.engine << std::boolalpha << "\t bisimulation=" << mpi.applyBisimulation
                        << "\t exact=" << (mpi.verificationValueType != ModelProcessingInformation::ValueType::FinitePrecision) << std::noboolalpha << '\n');
}

/*!
 * Sets the model processing information based on the given input.
 * Finding the right model processing information might require a conversion to jani.
 * In this case, the jani conversion is stored in the transformedJaniInput pointer (unless it is null)
 */
inline ModelProcessingInformation getModelProcessingInformation(SymbolicInput const& input,
                                                                std::shared_ptr<SymbolicInput> const& transformedJaniInput = nullptr) {
    ModelProcessingInformation mpi;
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    auto coreSettings = storm::settings::getModule<storm::settings::modules::CoreSettings>();
    auto generalSettings = storm::settings::getModule<storm::settings::modules::GeneralSettings>();
    auto bisimulationSettings = storm::settings::getModule<storm::settings::modules::BisimulationSettings>();

    // Set the engine.
    mpi.engine = coreSettings.getEngine();

    // Set whether bisimulation is to be used.
    mpi.applyBisimulation = generalSettings.isBisimulationSet();

    // Set the value type used for numeric values
    if (generalSettings.isParametricSet()) {
        mpi.verificationValueType = ModelProcessingInformation::ValueType::Parametric;
    } else if (generalSettings.isExactSet()) {
        mpi.verificationValueType = ModelProcessingInformation::ValueType::Exact;
    } else {
        mpi.verificationValueType = ModelProcessingInformation::ValueType::FinitePrecision;
    }
    auto originalVerificationValueType = mpi.verificationValueType;

    // Since the remaining settings could depend on the ones above, we need apply the automatic engine now.
    bool useAutomatic = input.model.is_initialized() && mpi.engine == storm::utility::Engine::Automatic;
    if (useAutomatic) {
        if (input.model->isJaniModel()) {
            // This can potentially overwrite the settings above, but will not overwrite settings that were explicitly set by the user (e.g. we will not disable
            // bisimulation or disable exact arithmetic)
            getModelProcessingInformationAutomatic(input, mpi);
        } else {
            // Transform Prism to jani first
            STORM_LOG_ASSERT(input.model->isPrismProgram(), "Unexpected type of input.");
            SymbolicInput janiInput;
            janiInput.properties = input.properties;
            storm::prism::Program const& prog = input.model.get().asPrismProgram();
            auto modelAndProperties = prog.toJani(input.preprocessedProperties.is_initialized() ? input.preprocessedProperties.get() : input.properties);
            janiInput.model = modelAndProperties.first;
            if (!modelAndProperties.second.empty()) {
                janiInput.preprocessedProperties = std::move(modelAndProperties.second);
            }
            // This can potentially overwrite the settings above, but will not overwrite settings that were explicitly set by the user (e.g. we will not disable
            // bisimulation or disable exact arithmetic)
            getModelProcessingInformationAutomatic(janiInput, mpi);
            if (transformedJaniInput) {
                // We cache the transformation result.
                *transformedJaniInput = std::move(janiInput);
            }
        }
    }

    // Check whether these settings are compatible with the provided input.
    if (input.model) {
        auto checkCompatibleSettings = [&mpi, &input] {
            switch (mpi.verificationValueType) {
                case ModelProcessingInformation::ValueType::Parametric:
                    return storm::utility::canHandle<storm::RationalFunction>(
                        mpi.engine, input.preprocessedProperties.is_initialized() ? input.preprocessedProperties.get() : input.properties, input.model.get());
                case ModelProcessingInformation::ValueType::Exact:
                    return storm::utility::canHandle<storm::RationalNumber>(
                        mpi.engine, input.preprocessedProperties.is_initialized() ? input.preprocessedProperties.get() : input.properties, input.model.get());
                    break;
                case ModelProcessingInformation::ValueType::FinitePrecision:
                    return storm::utility::canHandle<double>(
                        mpi.engine, input.preprocessedProperties.is_initialized() ? input.preprocessedProperties.get() : input.properties, input.model.get());
            }
            return false;
        };
        mpi.isCompatible = checkCompatibleSettings();
        if (!mpi.isCompatible) {
            if (useAutomatic) {
                bool useExact = mpi.verificationValueType != ModelProcessingInformation::ValueType::FinitePrecision;
                STORM_LOG_WARN("The settings picked by the automatic engine (engine="
                               << mpi.engine << ", bisim=" << mpi.applyBisimulation << ", exact=" << useExact
                               << ") are incompatible with this model. Falling back to default settings.");
                mpi.engine = storm::utility::Engine::Sparse;
                mpi.applyBisimulation = false;
                mpi.verificationValueType = originalVerificationValueType;
                // Retry check with new settings
                mpi.isCompatible = checkCompatibleSettings();
            }
        }
    } else {
        // If there is no input model, nothing has to be done, actually
        mpi.isCompatible = true;
    }

    // Set whether a transformation to jani is required or necessary
    mpi.transformToJani = ioSettings.isPrismToJaniSet();
    if (input.model) {
        auto builderType = storm::utility::getBuilderType(mpi.engine);
        bool transformToJaniForDdMA = (builderType == storm::builder::BuilderType::Dd) &&
                                      (input.model->getModelType() == storm::storage::SymbolicModelDescription::ModelType::MA) && (!input.model->isJaniModel());
        STORM_LOG_WARN_COND(mpi.transformToJani || !transformToJaniForDdMA,
                            "Dd-based model builder for Markov Automata is only available for JANI models, automatically converting the input model.");
        mpi.transformToJani |= transformToJaniForDdMA;
    }

    // Set the Valuetype used during model building
    mpi.buildValueType = mpi.verificationValueType;
    if (bisimulationSettings.useExactArithmeticInDdBisimulation()) {
        if (storm::utility::getBuilderType(mpi.engine) == storm::builder::BuilderType::Dd && mpi.applyBisimulation) {
            if (mpi.buildValueType == ModelProcessingInformation::ValueType::FinitePrecision) {
                mpi.buildValueType = ModelProcessingInformation::ValueType::Exact;
            }
        } else {
            STORM_LOG_WARN("Requested using exact arithmetic in Dd bisimulation but no dd bisimulation is applied.");
        }
    }

    // Set the Dd library
    mpi.ddType = coreSettings.getDdLibraryType();
    if (mpi.ddType == storm::dd::DdType::CUDD && coreSettings.isDdLibraryTypeSetFromDefaultValue()) {
        if (!(mpi.buildValueType == ModelProcessingInformation::ValueType::FinitePrecision &&
              mpi.verificationValueType == ModelProcessingInformation::ValueType::FinitePrecision)) {
            STORM_LOG_INFO("Switching to DD library sylvan to allow for rational arithmetic.");
            mpi.ddType = storm::dd::DdType::Sylvan;
        }
    }
    return mpi;
}

auto castAndApply(std::shared_ptr<storm::models::ModelBase> const& model, auto const& callback) {
    STORM_LOG_ASSERT(model, "Tried to cast a model that does not exist.");

    // Helper to actually perform the cast once value type and model representation type is known
    auto castAndApplyImpl = [&model, &callback]<typename TargetModelType> {
        auto res = model->template as<TargetModelType>();
        STORM_LOG_ASSERT(res, "Casting model pointer failed unexpectedly.");
        return callback(res);
    };

    // Helper to branch on type of model representation
    auto castAndApplyVT = [&]<typename ValueType> {
        if (model->isSparseModel()) {
            return castAndApplyImpl.template operator()<storm::models::sparse::Model<ValueType>>();
        } else {
            auto ddType = model->getDdType();
            STORM_LOG_ASSERT(model->isSymbolicModel() && ddType.has_value(), "Unexpected model representation");
            if constexpr (storm::IsIntervalType<ValueType>) {
                // Avoiding a couple of unnecessary template instantiations
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Symbolic interval models are currently not supported.");
            } else {
                using enum storm::dd::DdType;
                if (*ddType == CUDD) {
                    if constexpr (std::is_same_v<ValueType, double>) {
                        return castAndApplyImpl.template operator()<storm::models::symbolic::Model<CUDD, ValueType>>();
                    }
                }
                STORM_LOG_ASSERT(*ddType == Sylvan, "Unexpected Dd type");
                return castAndApplyImpl.template operator()<storm::models::symbolic::Model<Sylvan, ValueType>>();
            }
        }
    };

    // branch on type of value representation
    if (model->supportsParameters()) {
        return castAndApplyVT.template operator()<storm::RationalFunction>();
    } else if (model->supportsUncertainty()) {
        if (model->isExact()) {
            return castAndApplyVT.template operator()<storm::RationalInterval>();
        } else {
            return castAndApplyVT.template operator()<storm::Interval>();
        }
    } else {
        if (model->isExact()) {
            return castAndApplyVT.template operator()<storm::RationalNumber>();
        } else {
            return castAndApplyVT.template operator()<double>();
        }
    }
}

auto applyValueType(ModelProcessingInformation::ValueType vt, auto const& callback) {
    using enum ModelProcessingInformation::ValueType;
    switch (vt) {
        case FinitePrecision:
            return callback.template operator()<double>();
        case Exact:
            return callback.template operator()<storm::RationalNumber>();
        case Parametric:
            return callback.template operator()<storm::RationalFunction>();
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected value type.");
}

auto applyDdLibValueType(storm::dd::DdType dd, ModelProcessingInformation::ValueType vt, auto const& callback) {
    using enum storm::dd::DdType;
    using enum ModelProcessingInformation::ValueType;
    switch (dd) {
        case CUDD:
            STORM_LOG_THROW(vt == FinitePrecision, storm::exceptions::UnexpectedException, "Unexpected value type for DD library Cudd.");
            return callback.template operator()<CUDD, double>();
        case Sylvan:
            switch (vt) {
                case FinitePrecision:
                    return callback.template operator()<Sylvan, double>();
                case Exact:
                    return callback.template operator()<Sylvan, storm::RationalNumber>();
                case Parametric:
                    return callback.template operator()<Sylvan, storm::RationalFunction>();
            }
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected DDType or value type.");
}

inline void ensureNoUndefinedPropertyConstants(std::vector<storm::jani::Property> const& properties) {
    // Make sure there are no undefined constants remaining in any property.
    for (auto const& property : properties) {
        std::set<storm::expressions::Variable> usedUndefinedConstants = property.getUndefinedConstants();
        if (!usedUndefinedConstants.empty()) {
            std::vector<std::string> undefinedConstantsNames;
            for (auto const& constant : usedUndefinedConstants) {
                undefinedConstantsNames.emplace_back(constant.getName());
            }
            STORM_LOG_THROW(
                false, storm::exceptions::InvalidArgumentException,
                "The property '" << property << " still refers to the undefined constants " << boost::algorithm::join(undefinedConstantsNames, ",") << ".");
        }
    }
}

inline std::pair<SymbolicInput, ModelProcessingInformation> preprocessSymbolicInput(SymbolicInput const& input) {
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();

    SymbolicInput output = input;

    // Preprocess properties (if requested)
    if (ioSettings.isPropertiesAsMultiSet()) {
        STORM_LOG_THROW(!input.properties.empty(), storm::exceptions::InvalidArgumentException,
                        "Can not translate properties to multi-objective formula because no properties were specified.");
        // If we come from storm-pars, the following fails as multiObjectiveSettings are not loaded
        auto multiObjSettings = storm::settings::getModule<storm::settings::modules::MultiObjectiveSettings>();
        output.properties = {storm::api::createMultiObjectiveProperty(output.properties, multiObjSettings.isLexicographicModelCheckingSet())};
    }

    // Substitute constant definitions in symbolic input.
    std::string constantDefinitionString = ioSettings.getConstantDefinitionString();
    std::map<storm::expressions::Variable, storm::expressions::Expression> constantDefinitions;
    if (output.model) {
        constantDefinitions = output.model.get().parseConstantDefinitions(constantDefinitionString);
        output.model = output.model.get().preprocess(constantDefinitions);
    }
    if (!output.properties.empty()) {
        output.properties = storm::api::substituteConstantsInProperties(output.properties, constantDefinitions);
    }
    ensureNoUndefinedPropertyConstants(output.properties);
    auto transformedJani = std::make_shared<SymbolicInput>();
    ModelProcessingInformation mpi = getModelProcessingInformation(output, transformedJani);

    // Check whether conversion for PRISM to JANI is requested or necessary.
    if (output.model && output.model.get().isPrismProgram()) {
        if (mpi.transformToJani) {
            if (transformedJani->model) {
                // Use the cached transformation if possible
                output = std::move(*transformedJani);
            } else {
                storm::prism::Program const& model = output.model.get().asPrismProgram();
                auto modelAndProperties = model.toJani(output.properties);

                output.model = modelAndProperties.first;

                if (!modelAndProperties.second.empty()) {
                    output.preprocessedProperties = std::move(modelAndProperties.second);
                }
            }
        }
    }

    if (output.model && output.model.get().isJaniModel()) {
        storm::jani::ModelFeatures supportedFeatures = storm::api::getSupportedJaniFeatures(storm::utility::getBuilderType(mpi.engine));
        storm::api::simplifyJaniModel(output.model.get().asJaniModel(), output.properties, supportedFeatures);

        const auto& buildSettings = storm::settings::getModule<storm::settings::modules::BuildSettings>();
        if (buildSettings.isLocationEliminationSet()) {
            auto locationHeuristic = buildSettings.getLocationEliminationLocationHeuristic();
            auto edgesHeuristic = buildSettings.getLocationEliminationEdgesHeuristic();
            output.model->setModel(storm::jani::JaniLocalEliminator::eliminateAutomatically(output.model.get().asJaniModel(), output.properties,
                                                                                            locationHeuristic, edgesHeuristic));
        }
    }

    return {output, mpi};
}

inline void exportSymbolicInput(SymbolicInput const& input) {
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    if (input.model && input.model.get().isJaniModel()) {
        storm::storage::SymbolicModelDescription const& model = input.model.get();
        if (ioSettings.isExportJaniDotSet()) {
            storm::api::exportJaniModelAsDot(model.asJaniModel(), ioSettings.getExportJaniDotFilename());
        }
    }
}

inline std::vector<std::shared_ptr<storm::logic::Formula const>> createFormulasToRespect(std::vector<storm::jani::Property> const& properties) {
    std::vector<std::shared_ptr<storm::logic::Formula const>> result = storm::api::extractFormulasFromProperties(properties);

    for (auto const& property : properties) {
        if (!property.getFilter().getStatesFormula()->isInitialFormula()) {
            result.push_back(property.getFilter().getStatesFormula());
        }
    }

    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
std::shared_ptr<storm::models::ModelBase> buildModelDd(SymbolicInput const& input) {
    if (DdType == storm::dd::DdType::Sylvan) {
        auto numThreads = storm::settings::getModule<storm::settings::modules::SylvanSettings>().getNumberOfThreads();
        STORM_PRINT_AND_LOG("Using Sylvan with " << numThreads << " parallel threads.\n");
    }
    auto buildSettings = storm::settings::getModule<storm::settings::modules::BuildSettings>();
    return storm::api::buildSymbolicModel<DdType, ValueType>(input.model.get(), createFormulasToRespect(input.properties), buildSettings.isBuildFullModelSet(),
                                                             !buildSettings.isApplyNoMaximumProgressAssumptionSet());
}

inline storm::builder::BuilderOptions createBuildOptionsSparseFromSettings(SymbolicInput const& input) {
    auto buildSettings = storm::settings::getModule<storm::settings::modules::BuildSettings>();
    storm::builder::BuilderOptions options(createFormulasToRespect(input.properties), input.model.get());
    options.setBuildChoiceLabels(options.isBuildChoiceLabelsSet() || buildSettings.isBuildChoiceLabelsSet());
    options.setBuildStateValuations(options.isBuildStateValuationsSet() || buildSettings.isBuildStateValuationsSet());
    options.setBuildAllLabels(options.isBuildAllLabelsSet() || buildSettings.isBuildAllLabelsSet());
    options.setBuildObservationValuations(options.isBuildObservationValuationsSet() || buildSettings.isBuildObservationValuationsSet());
    bool buildChoiceOrigins = options.isBuildChoiceOriginsSet() || buildSettings.isBuildChoiceOriginsSet();
    if (storm::settings::manager().hasModule(storm::settings::modules::CounterexampleGeneratorSettings::moduleName)) {
        auto counterexampleGeneratorSettings = storm::settings::getModule<storm::settings::modules::CounterexampleGeneratorSettings>();
        if (counterexampleGeneratorSettings.isCounterexampleSet()) {
            buildChoiceOrigins |= counterexampleGeneratorSettings.isMinimalCommandSetGenerationSet();
        }
    }
    options.setBuildChoiceOrigins(buildChoiceOrigins);

    if (buildSettings.isApplyNoMaximumProgressAssumptionSet()) {
        options.setApplyMaximalProgressAssumption(false);
    }

    if (buildSettings.isExplorationChecksSet()) {
        options.setExplorationChecks();
    }
    options.setReservedBitsForUnboundedVariables(buildSettings.getBitsForUnboundedVariables());

    options.setAddOutOfBoundsState(buildSettings.isBuildOutOfBoundsStateSet());
    if (buildSettings.isBuildFullModelSet()) {
        options.clearTerminalStates();
        options.setApplyMaximalProgressAssumption(false);
        options.setBuildAllLabels(true);
        options.setBuildAllRewardModels(true);
    }

    if (buildSettings.isAddOverlappingGuardsLabelSet()) {
        options.setAddOverlappingGuardsLabel(true);
    }

    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    if (ioSettings.isComputeExpectedVisitingTimesSet() || ioSettings.isComputeSteadyStateDistributionSet()) {
        options.clearTerminalStates();
    }
    return options;
}

template<typename ValueType>
std::shared_ptr<storm::models::ModelBase> buildModelSparse(SymbolicInput const& input, storm::builder::BuilderOptions const& options) {
    // If the input is an interval model, we might need to change the ValueType to an interval type.
    if (!storm::IsIntervalType<ValueType> && input.model.is_initialized() && input.model->isPrismProgram() &&
        input.model->asPrismProgram().hasIntervalUpdates()) {
        // Get the right interval type for the given ValueType
        bool constexpr IsDoubleInterval = std::is_same_v<ValueType, storm::IntervalBaseType<storm::Interval>>;
        bool constexpr IsRationalInterval = std::is_same_v<ValueType, storm::IntervalBaseType<storm::RationalInterval>>;
        STORM_LOG_THROW(IsDoubleInterval || IsRationalInterval, storm::exceptions::NotSupportedException,
                        "Can not build interval model for the provided value type.");
        using IntervalType = std::conditional_t<IsDoubleInterval, storm::Interval, storm::RationalInterval>;
        return storm::api::buildSparseModel<IntervalType>(input.model.get(), options);
    } else {
        return storm::api::buildSparseModel<ValueType>(input.model.get(), options);
    }
}

template<typename ValueType>
std::shared_ptr<storm::models::ModelBase> buildModelExplicit(storm::settings::modules::IOSettings const& ioSettings,
                                                             storm::settings::modules::BuildSettings const& buildSettings) {
    std::shared_ptr<storm::models::ModelBase> result;
    if (ioSettings.isExplicitSet()) {
        result = storm::api::buildExplicitModel<ValueType>(
            ioSettings.getTransitionFilename(), ioSettings.getLabelingFilename(),
            ioSettings.isStateRewardsSet() ? boost::optional<std::string>(ioSettings.getStateRewardsFilename()) : boost::none,
            ioSettings.isTransitionRewardsSet() ? boost::optional<std::string>(ioSettings.getTransitionRewardsFilename()) : boost::none,
            ioSettings.isChoiceLabelingSet() ? boost::optional<std::string>(ioSettings.getChoiceLabelingFilename()) : boost::none);
    } else if (ioSettings.isExplicitDRNSet()) {
        storm::parser::DirectEncodingParserOptions options;
        options.buildChoiceLabeling = buildSettings.isBuildChoiceLabelsSet();
        using enum storm::parser::DirectEncodingValueType;
        storm::parser::DirectEncodingValueType valueType{Default};
        if constexpr (std::is_same_v<ValueType, double>) {
            valueType = Double;
        } else if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
            valueType = Rational;
        } else {
            static_assert(std::is_same_v<ValueType, storm::RationalFunction>, "Unexpected value type.");
            valueType = Parametric;
        }
        result = storm::api::buildExplicitDRNModel(ioSettings.getExplicitDRNFilename(), valueType, options);
    } else if (ioSettings.isExplicitUmbSet()) {
        storm::umb::ImportOptions options;
        options.buildChoiceLabeling = buildSettings.isBuildChoiceLabelsSet();
        options.buildStateValuations = buildSettings.isBuildStateValuationsSet();
        if constexpr (std::is_same_v<ValueType, storm::RationalFunction>) {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "RationalFunction currently not supported for UMB models.");
        } else if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
            options.valueType = umb::ImportOptions::ValueType::Rational;
        } else {
            static_assert(std::is_same_v<ValueType, double>, "Unhandled value type.");
            options.valueType = umb::ImportOptions::ValueType::Double;
        }
        result = storm::api::buildExplicitUmbModel(ioSettings.getExplicitUmbFilename(), options);
    } else {
        STORM_LOG_THROW(ioSettings.isExplicitIMCASet(), storm::exceptions::InvalidSettingsException, "Unexpected explicit model input type.");
        result = storm::api::buildExplicitIMCAModel<ValueType>(ioSettings.getExplicitIMCAFilename());
    }
    return result;
}

inline std::shared_ptr<storm::models::ModelBase> buildModel(SymbolicInput const& input, storm::settings::modules::IOSettings const& ioSettings,
                                                            ModelProcessingInformation const& mpi) {
    storm::utility::Stopwatch modelBuildingWatch(true);

    std::shared_ptr<storm::models::ModelBase> result;
    if (input.model) {
        auto builderType = storm::utility::getBuilderType(mpi.engine);
        if (builderType == storm::builder::BuilderType::Dd) {
            result = applyDdLibValueType(mpi.ddType, mpi.buildValueType, [&input]<storm::dd::DdType DD, typename VT>() { return buildModelDd<DD, VT>(input); });
        } else if (builderType == storm::builder::BuilderType::Explicit) {
            result = applyValueType(mpi.buildValueType, [&input]<typename VT>() {
                auto options = createBuildOptionsSparseFromSettings(input);
                return buildModelSparse<VT>(input, options);
            });
        }
    } else if (ioSettings.isExplicitSet() || ioSettings.isExplicitDRNSet() || ioSettings.isExplicitUmbSet() || ioSettings.isExplicitIMCASet()) {
        STORM_LOG_THROW(mpi.engine == storm::utility::Engine::Sparse, storm::exceptions::InvalidSettingsException,
                        "Can only use sparse engine with explicit input.");
        result = applyValueType(mpi.buildValueType, [&ioSettings]<typename VT>() {
            return buildModelExplicit<VT>(ioSettings, storm::settings::getModule<storm::settings::modules::BuildSettings>());
        });
    }

    modelBuildingWatch.stop();
    if (result) {
        STORM_PRINT("Time for model construction: " << modelBuildingWatch << ".\n\n");
    }

    return result;
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> preprocessSparseMarkovAutomaton(
    std::shared_ptr<storm::models::sparse::MarkovAutomaton<ValueType>> const& model) {
    auto transformationSettings = storm::settings::getModule<storm::settings::modules::TransformationSettings>();
    auto debugSettings = storm::settings::getModule<storm::settings::modules::DebugSettings>();

    std::shared_ptr<storm::models::sparse::Model<ValueType>> result = model;
    model->close();
    STORM_LOG_WARN_COND(!debugSettings.isAdditionalChecksSet() || !model->containsZenoCycle(),
                        "MA contains a Zeno cycle. Model checking results cannot be trusted.");

    if (model->isConvertibleToCtmc()) {
        STORM_LOG_WARN_COND(false, "MA is convertible to a CTMC, consider using a CTMC instead.");
        result = model->convertToCtmc();
    }

    if (transformationSettings.isChainEliminationSet()) {
        if constexpr (storm::IsIntervalType<ValueType>) {
            // Currently not enabling this for interval models, as this would require a number of additional template instantiations.
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Chain elimination not supported for interval models.");
        } else {
            // TODO: we should also transform the properties at this point.
            result = storm::api::eliminateNonMarkovianChains(result->template as<storm::models::sparse::MarkovAutomaton<ValueType>>(), {},
                                                             transformationSettings.getLabelBehavior())
                         .first;
        }
    }

    return result;
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType>> preprocessSparseModelBisimulation(
    std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, SymbolicInput const& input,
    storm::settings::modules::BisimulationSettings const& bisimulationSettings, bool graphPreserving = true) {
    storm::storage::BisimulationType bisimType = storm::storage::BisimulationType::Strong;
    if (bisimulationSettings.isWeakBisimulationSet()) {
        bisimType = storm::storage::BisimulationType::Weak;
    }

    STORM_LOG_INFO("Performing bisimulation minimization...");
    return storm::api::performBisimulationMinimization<ValueType>(model, createFormulasToRespect(input.properties), bisimType, graphPreserving);
}

template<typename ValueType>
std::pair<std::shared_ptr<storm::models::ModelBase>, bool> preprocessModel(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                                                                           SymbolicInput const& input, ModelProcessingInformation const& mpi) {
    STORM_LOG_THROW(mpi.buildValueType == mpi.verificationValueType, storm::exceptions::NotSupportedException,
                    "Converting value types for sparse engine is not supported.");
    auto bisimulationSettings = storm::settings::getModule<storm::settings::modules::BisimulationSettings>();
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    auto transformationSettings = storm::settings::getModule<storm::settings::modules::TransformationSettings>();

    std::pair<std::shared_ptr<storm::models::sparse::Model<ValueType>>, bool> result = std::make_pair(model, false);

    if (auto order = transformationSettings.getModelPermutation(); order.has_value()) {
        auto seed = transformationSettings.getModelPermutationSeed();
        STORM_PRINT_AND_LOG("Permuting model states using " << storm::utility::permutation::orderKindtoString(order.value()) << " order"
                                                            << (seed.has_value() ? " with seed " + std::to_string(seed.value()) : "") << ".\n");
        result.first = storm::api::permuteModelStates(result.first, order.value(), seed);
        result.second = true;
        STORM_PRINT_AND_LOG("Transition matrix hash after permuting: " << result.first->getTransitionMatrix().hash() << ".\n");
    }

    if (result.first->isOfType(storm::models::ModelType::MarkovAutomaton)) {
        result.first = preprocessSparseMarkovAutomaton(result.first->template as<storm::models::sparse::MarkovAutomaton<ValueType>>());
        result.second = true;
    }

    if (mpi.applyBisimulation) {
        if constexpr (storm::IsIntervalType<ValueType>) {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Bisimulation not supported for interval models.");
        } else {
            result.first = preprocessSparseModelBisimulation(result.first, input, bisimulationSettings);
            result.second = true;
        }
    }

    if (transformationSettings.isToDiscreteTimeModelSet()) {
        if constexpr (storm::IsIntervalType<ValueType>) {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Transformation to discrete time model not supported for interval models.");
        } else {
            // TODO: we should also transform the properties at this point.
            STORM_LOG_WARN_COND(
                !model->hasRewardModel("_time"),
                "Scheduled transformation to discrete time model, but a reward model named '_time' is already present in this model. We might take "
                "the wrong reward model later.");
            result.first =
                storm::api::transformContinuousToDiscreteTimeSparseModel(std::move(*result.first), storm::api::extractFormulasFromProperties(input.properties))
                    .first;
            result.second = true;
        }
    }

    if (transformationSettings.isToNondeterministicModelSet()) {
        result.first = storm::api::transformToNondeterministicModel<ValueType>(std::move(*result.first));
        result.second = true;
    }

    return result;
}

template<typename ValueType>
void exportModel(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, SymbolicInput const& input) {
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();

    if (ioSettings.isExportBuildSet()) {
        storm::utility::Stopwatch modelExportWatch;
        modelExportWatch.start();
        STORM_PRINT("\nExporting model to '" << ioSettings.getExportBuildFilename() << "'.\n");
        switch (ioSettings.getExportBuildFormat()) {
            case storm::io::ModelExportFormat::Dot:
                storm::api::exportSparseModelAsDot(model, ioSettings.getExportBuildFilename(), ioSettings.getExportDotMaxWidth());
                break;
            case storm::io::ModelExportFormat::Drn: {
                storm::io::DirectEncodingExporterOptions options;
                options.allowPlaceholders = !ioSettings.isExplicitExportPlaceholdersDisabled();
                options.compression = ioSettings.getCompressionMode();
                if (ioSettings.isExportDigitsSet()) {
                    options.outputPrecision = ioSettings.getExportDigits();
                }
                storm::api::exportSparseModelAsDrn(model, ioSettings.getExportBuildFilename(), options,
                                                   input.model ? input.model.get().getParameterNames() : std::vector<std::string>());
                break;
            }
            case storm::io::ModelExportFormat::Json:
                storm::api::exportSparseModelAsJson(model, ioSettings.getExportBuildFilename());
                break;
            case storm::io::ModelExportFormat::Umb: {
                storm::umb::ExportOptions options;
                options.compression = ioSettings.getCompressionMode();
                storm::api::exportSparseModelAsUmb(model, ioSettings.getExportBuildFilename(), options);
                break;
            }
            default:
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                "Exporting sparse models in " << storm::io::toString(ioSettings.getExportBuildFormat()) << " format is not supported.");
        }
        modelExportWatch.stop();
        STORM_PRINT("Time for model export: " << modelExportWatch << ".\n\n");
    }

    // TODO: The following options are depreciated and shall be removed at some point:

    if (ioSettings.isExportExplicitSet()) {
        storm::api::exportSparseModelAsDrn(model, ioSettings.getExportExplicitFilename(),
                                           input.model ? input.model.get().getParameterNames() : std::vector<std::string>(),
                                           !ioSettings.isExplicitExportPlaceholdersDisabled());
    }

    if (ioSettings.isExportDdSet()) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Exporting in drdd format is only supported for DDs.");
    }

    if (ioSettings.isExportDotSet()) {
        storm::api::exportSparseModelAsDot(model, ioSettings.getExportDotFilename(), ioSettings.getExportDotMaxWidth());
    }
}

template<storm::dd::DdType DdType, typename ValueType>
void exportModel(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model, SymbolicInput const&) {
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();

    if (ioSettings.isExportBuildSet()) {
        switch (ioSettings.getExportBuildFormat()) {
            case storm::io::ModelExportFormat::Dot:
                storm::api::exportSymbolicModelAsDot(model, ioSettings.getExportBuildFilename());
                break;
            case storm::io::ModelExportFormat::Drdd:
                storm::api::exportSymbolicModelAsDrdd(model, ioSettings.getExportBuildFilename());
                break;
            default:
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                "Exporting symbolic models in " << storm::io::toString(ioSettings.getExportBuildFormat()) << " format is not supported.");
        }
    }

    // TODO: The following options are depreciated and shall be removed at some point:

    if (ioSettings.isExportExplicitSet()) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Exporting in drn format is only supported for sparse models.");
    }

    if (ioSettings.isExportDdSet()) {
        storm::api::exportSymbolicModelAsDrdd(model, ioSettings.getExportDdFilename());
    }

    if (ioSettings.isExportDotSet()) {
        storm::api::exportSymbolicModelAsDot(model, ioSettings.getExportDotFilename());
    }
}

template<storm::dd::DdType DdType, typename ValueType>
typename std::enable_if<DdType != storm::dd::DdType::Sylvan && !std::is_same<ValueType, double>::value, std::shared_ptr<storm::models::Model<ValueType>>>::type
preprocessDdMarkovAutomaton(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model) {
    return model;
}

template<storm::dd::DdType DdType, typename ValueType>
typename std::enable_if<DdType == storm::dd::DdType::Sylvan || std::is_same<ValueType, double>::value, std::shared_ptr<storm::models::Model<ValueType>>>::type
preprocessDdMarkovAutomaton(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model) {
    auto ma = model->template as<storm::models::symbolic::MarkovAutomaton<DdType, ValueType>>();
    if (!ma->isClosed()) {
        return std::make_shared<storm::models::symbolic::MarkovAutomaton<DdType, ValueType>>(ma->close());
    } else {
        return model;
    }
}

template<storm::dd::DdType DdType, typename ValueType, typename ExportValueType = ValueType>
std::shared_ptr<storm::models::Model<ExportValueType>> preprocessDdModelBisimulation(
    std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model, SymbolicInput const& input,
    storm::settings::modules::BisimulationSettings const& bisimulationSettings, ModelProcessingInformation const& mpi) {
    STORM_LOG_WARN_COND(!bisimulationSettings.isWeakBisimulationSet(),
                        "Weak bisimulation is currently not supported on DDs. Falling back to strong bisimulation.");

    auto quotientFormat = bisimulationSettings.getQuotientFormat();
    if (mpi.engine == storm::utility::Engine::DdSparse && quotientFormat != storm::dd::bisimulation::QuotientFormat::Sparse &&
        bisimulationSettings.isQuotientFormatSetFromDefaultValue()) {
        STORM_LOG_INFO("Setting bisimulation quotient format to 'sparse'.");
        quotientFormat = storm::dd::bisimulation::QuotientFormat::Sparse;
    }
    STORM_LOG_INFO("Performing bisimulation minimization...");
    return storm::api::performBisimulationMinimization<DdType, ValueType, ExportValueType>(
        model, createFormulasToRespect(input.properties), storm::storage::BisimulationType::Strong, bisimulationSettings.getSignatureMode(), quotientFormat);
}

template<typename ExportValueType, storm::dd::DdType DdType, typename ValueType>
std::pair<std::shared_ptr<storm::models::ModelBase>, bool> preprocessDdModelImpl(
    std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model, SymbolicInput const& input, ModelProcessingInformation const& mpi) {
    auto bisimulationSettings = storm::settings::getModule<storm::settings::modules::BisimulationSettings>();
    std::pair<std::shared_ptr<storm::models::Model<ValueType>>, bool> intermediateResult = std::make_pair(model, false);

    if (model->isOfType(storm::models::ModelType::MarkovAutomaton)) {
        intermediateResult.first = preprocessDdMarkovAutomaton(intermediateResult.first->template as<storm::models::symbolic::Model<DdType, ValueType>>());
        intermediateResult.second = true;
    }

    std::unique_ptr<std::pair<std::shared_ptr<storm::models::Model<ExportValueType>>, bool>> result;
    auto symbolicModel = intermediateResult.first->template as<storm::models::symbolic::Model<DdType, ValueType>>();
    if (mpi.applyBisimulation) {
        std::shared_ptr<storm::models::Model<ExportValueType>> newModel =
            preprocessDdModelBisimulation<DdType, ValueType, ExportValueType>(symbolicModel, input, bisimulationSettings, mpi);
        result = std::make_unique<std::pair<std::shared_ptr<storm::models::Model<ExportValueType>>, bool>>(newModel, true);
    } else {
        result = std::make_unique<std::pair<std::shared_ptr<storm::models::Model<ExportValueType>>, bool>>(
            symbolicModel->template toValueType<ExportValueType>(), !std::is_same<ValueType, ExportValueType>::value);
    }

    if (result && result->first->isSymbolicModel() && mpi.engine == storm::utility::Engine::DdSparse) {
        // Mark as changed.
        result->second = true;

        std::shared_ptr<storm::models::symbolic::Model<DdType, ExportValueType>> symbolicModel =
            result->first->template as<storm::models::symbolic::Model<DdType, ExportValueType>>();
        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas;
        for (auto const& property : input.properties) {
            formulas.emplace_back(property.getRawFormula());
        }
        result->first = storm::api::transformSymbolicToSparseModel(symbolicModel, formulas);
        STORM_LOG_THROW(result, storm::exceptions::NotSupportedException, "The translation to a sparse model is not supported for the given model type.");
    }

    return *result;
}

template<storm::dd::DdType DdType, typename ValueType>
std::pair<std::shared_ptr<storm::models::ModelBase>, bool> preprocessModel(std::shared_ptr<storm::models::symbolic::Model<DdType, ValueType>> const& model,
                                                                           SymbolicInput const& input, ModelProcessingInformation const& mpi) {
    return applyValueType(mpi.verificationValueType, [&model, &input, &mpi]<typename VT>() -> std::pair<std::shared_ptr<storm::models::ModelBase>, bool> {
        // To safe a few template instantiations, we only consider those combinations that actually occur in the CLI
        if constexpr (std::is_same_v<ValueType, VT> ||
                      (DdType == storm::dd::DdType::Sylvan && std::is_same_v<ValueType, storm::RationalNumber> && std::is_same_v<VT, double>)) {
            return preprocessDdModelImpl<VT>(model, input, mpi);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                            "Unexpected combination of DD library, build value type, and verification value type.");
        }
    });
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

inline void printModelCheckingProperty(storm::jani::Property const& property) {
    STORM_PRINT("\nModel checking property \"" << property.getName() << "\": " << *property.getRawFormula() << " ...\n");
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

inline std::shared_ptr<storm::models::ModelBase> buildPreprocessModel(SymbolicInput const& input, ModelProcessingInformation const& mpi) {
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    auto buildSettings = storm::settings::getModule<storm::settings::modules::BuildSettings>();

    std::shared_ptr<storm::models::ModelBase> model;
    if (!buildSettings.isNoBuildModelSet()) {
        model = buildModel(input, ioSettings, mpi);
    }
    if (!model) {
        STORM_LOG_THROW(input.properties.empty(), storm::exceptions::InvalidSettingsException, "No input model.");
        return nullptr;
    }
    model->printModelInformationToStream(std::cout);

    storm::utility::Stopwatch preprocessingWatch(true);
    auto preprocessingResult = castAndApply(model, [&input, &mpi](auto const& m) { return preprocessModel(m, input, mpi); });
    preprocessingWatch.stop();
    if (preprocessingResult.second) {
        STORM_PRINT("\nTime for model preprocessing: " << preprocessingWatch << ".\n\n");
        model = preprocessingResult.first;
        model->printModelInformationToStream(std::cout);
    }

    return model;
}

inline std::shared_ptr<storm::models::ModelBase> buildPreprocessExportModel(SymbolicInput const& input, ModelProcessingInformation const& mpi) {
    auto model = buildPreprocessModel(input, mpi);
    if (model) {
        castAndApply(model, [&input](auto const& m) { exportModel(m, input); });
    }
    return model;
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
