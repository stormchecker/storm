
#pragma once
#include "storm-cli-utilities/cli.h"
#include "storm-cli-utilities/model-handling.h"

#include "storm-pars-cli/print.h"

#include "storm-pars/api/region.h"
#include "storm-pars/api/storm-pars.h"
#include "storm-pars/modelchecker/region/monotonicity/MonotonicityHelper.h"

#include "storm-pars/derivative/GradientDescentInstantiationSearcher.h"
#include "storm-pars/derivative/SparseDerivativeInstantiationModelChecker.h"
#include "storm-pars/modelchecker/instantiation/SparseCtmcInstantiationModelChecker.h"
#include "storm-pars/modelchecker/region/SparseDtmcParameterLiftingModelChecker.h"
#include "storm-pars/modelchecker/region/SparseParameterLiftingModelChecker.h"

#include "storm-pars/settings/ParsSettings.h"
#include "storm-pars/settings/modules/DerivativeSettings.h"
#include "storm-pars/settings/modules/MonotonicitySettings.h"
#include "storm-pars/settings/modules/ParametricSettings.h"
#include "storm-pars/settings/modules/RegionSettings.h"

#include "storm-pars/derivative/GradientDescentMethod.h"
#include "storm-pars/transformer/SparseParametricDtmcSimplifier.h"
#include "storm-pars/transformer/SparseParametricMdpSimplifier.h"

#include "storm-pars/utility/parametric.h"

#include "storm-parsers/parser/KeyValueParser.h"
#include "storm/api/storm.h"

#include "storm/exceptions/BaseException.h"
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/exceptions/NotSupportedException.h"

#include "storm/models/ModelBase.h"

#include "storm/settings/SettingsManager.h"

#include "storm/solver/stateelimination/NondeterministicModelStateEliminator.h"

#include "storm/storage/StronglyConnectedComponentDecomposition.h"
#include "storm/storage/SymbolicModelDescription.h"

#include "storm/io/file.h"
#include "storm/utility/Engine.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/initialize.h"
#include "storm/utility/macros.h"

#include "storm/settings/modules/BisimulationSettings.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/settings/modules/TransformationSettings.h"

namespace storm::pars {

template<typename ValueType>
struct SampleInformation {
    SampleInformation(bool graphPreserving = false, bool exact = false) : graphPreserving(graphPreserving), exact(exact) {
        // Intentionally left empty.
    }

    bool empty() const {
        return cartesianProducts.empty();
    }

    std::vector<std::map<typename storm::utility::parametric::VariableType<ValueType>::type,
                         std::vector<typename storm::utility::parametric::CoefficientType<ValueType>::type>>>
        cartesianProducts;
    bool graphPreserving;
    bool exact;
};

template<template<typename, typename> class ModelCheckerType, typename ModelType, typename ValueType, typename SolveValueType = double>
void verifyPropertiesAtSamplePointsDerivative(ModelType const& model, cli::SymbolicInput const& input, SampleInformation<ValueType> const& samples) {
    // When samples are provided, we create an instantiation model checker.
    ModelCheckerType<ValueType, SolveValueType> modelchecker(model);

    for (auto const& property : input.properties) {
        storm::cli::printModelCheckingProperty(property);

        modelchecker.specifyFormula(Environment(), storm::api::createTask<ValueType>(property.getRawFormula(), true));

        storm::utility::parametric::Valuation<ValueType> valuation;

        std::vector<typename storm::utility::parametric::VariableType<ValueType>::type> parameters;
        std::vector<typename std::vector<typename storm::utility::parametric::CoefficientType<ValueType>::type>::const_iterator> iterators;
        std::vector<typename std::vector<typename storm::utility::parametric::CoefficientType<ValueType>::type>::const_iterator> iteratorEnds;

        storm::utility::Stopwatch watch(true);
        for (auto const& product : samples.cartesianProducts) {
            parameters.clear();
            iterators.clear();
            iteratorEnds.clear();

            for (auto const& entry : product) {
                parameters.push_back(entry.first);
                iterators.push_back(entry.second.cbegin());
                iteratorEnds.push_back(entry.second.cend());
            }

            bool done = false;
            while (!done) {
                // Read off valuation.
                for (uint64_t i = 0; i < parameters.size(); ++i) {
                    valuation[parameters[i]] = *iterators[i];
                }

                for (auto const& parameter : parameters) {
                    storm::utility::Stopwatch valuationWatch(true);
                    std::unique_ptr<storm::modelchecker::CheckResult> result = modelchecker.check(Environment(), valuation, parameter);
                    valuationWatch.stop();

                    if (result) {
                        result->filter(storm::modelchecker::ExplicitQualitativeCheckResult(model.getInitialStates()));
                    }
                    STORM_PRINT_AND_LOG("Derivative w.r.t. " << parameter << ":\n");
                    printInitialStatesResult<ValueType>(result, &valuationWatch, &valuation);
                }

                for (uint64_t i = 0; i < parameters.size(); ++i) {
                    ++iterators[i];
                    if (iterators[i] == iteratorEnds[i]) {
                        // Reset iterator and proceed to move next iterator.
                        iterators[i] = product.at(parameters[i]).cbegin();

                        // If the last iterator was removed, we are done.
                        if (i == parameters.size() - 1) {
                            done = true;
                        }
                    } else {
                        // If an iterator was moved but not reset, we have another valuation to check.
                        break;
                    }
                }
            }
        }

        watch.stop();
        STORM_PRINT_AND_LOG("Overall time for sampling all instances: " << watch << "\n\n");
    }
}

template<template<typename, typename> class ModelCheckerType, typename ModelType, typename ValueType, typename SolveValueType = double, bool Derivative = false>
void verifyPropertiesAtSamplePoints(ModelType const& model, cli::SymbolicInput const& input, SampleInformation<ValueType> const& samples) {
    // When samples are provided, we create an instantiation model checker.
    ModelCheckerType<ModelType, SolveValueType> modelchecker(model);

    for (auto const& property : input.properties) {
        storm::cli::printModelCheckingProperty(property);

        modelchecker.specifyFormula(storm::api::createTask<ValueType>(property.getRawFormula(), true));
        modelchecker.setInstantiationsAreGraphPreserving(samples.graphPreserving);

        storm::utility::parametric::Valuation<ValueType> valuation;

        std::vector<typename storm::utility::parametric::VariableType<ValueType>::type> parameters;
        std::vector<typename std::vector<typename storm::utility::parametric::CoefficientType<ValueType>::type>::const_iterator> iterators;
        std::vector<typename std::vector<typename storm::utility::parametric::CoefficientType<ValueType>::type>::const_iterator> iteratorEnds;

        storm::utility::Stopwatch watch(true);
        for (auto const& product : samples.cartesianProducts) {
            parameters.clear();
            iterators.clear();
            iteratorEnds.clear();

            for (auto const& entry : product) {
                parameters.push_back(entry.first);
                iterators.push_back(entry.second.cbegin());
                iteratorEnds.push_back(entry.second.cend());
            }

            bool done = false;
            while (!done) {
                // Read off valuation.
                for (uint64_t i = 0; i < parameters.size(); ++i) {
                    valuation[parameters[i]] = *iterators[i];
                }

                storm::utility::Stopwatch valuationWatch(true);
                std::unique_ptr<storm::modelchecker::CheckResult> result = modelchecker.check(Environment(), valuation);
                valuationWatch.stop();

                if (result) {
                    result->filter(storm::modelchecker::ExplicitQualitativeCheckResult<SolveValueType>(model.getInitialStates()));
                }
                printInitialStatesResult<ValueType>(result, &valuationWatch, &valuation);

                for (uint64_t i = 0; i < parameters.size(); ++i) {
                    ++iterators[i];
                    if (iterators[i] == iteratorEnds[i]) {
                        // Reset iterator and proceed to move next iterator.
                        iterators[i] = product.at(parameters[i]).cbegin();

                        // If the last iterator was removed, we are done.
                        if (i == parameters.size() - 1) {
                            done = true;
                        }
                    } else {
                        // If an iterator was moved but not reset, we have another valuation to check.
                        break;
                    }
                }
            }
        }

        watch.stop();
        STORM_PRINT_AND_LOG("Overall time for sampling all instances: " << watch << "\n\n");
    }
}

template<typename ValueType, typename SolveValueType = double>
void verifyPropertiesAtSamplePointsWithSparseEngineDerivatives(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                                                               cli::SymbolicInput const& input, SampleInformation<ValueType> const& samples) {
    if (model->isOfType(storm::models::ModelType::Dtmc)) {
        verifyPropertiesAtSamplePointsDerivative<storm::derivative::SparseDerivativeInstantiationModelChecker, storm::models::sparse::Dtmc<ValueType>,
                                                 ValueType, SolveValueType>(*model->template as<storm::models::sparse::Dtmc<ValueType>>(), input, samples);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Sampling the derivative is currently only supported for DTMCs.");
    }
}

template<typename ValueType, typename SolveValueType = double>
void verifyPropertiesAtSamplePointsWithSparseEngine(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, cli::SymbolicInput const& input,
                                                    SampleInformation<ValueType> const& samples) {
    if (model->isOfType(storm::models::ModelType::Dtmc)) {
        verifyPropertiesAtSamplePoints<storm::modelchecker::SparseDtmcInstantiationModelChecker, storm::models::sparse::Dtmc<ValueType>, ValueType,
                                       SolveValueType>(*model->template as<storm::models::sparse::Dtmc<ValueType>>(), input, samples);
    } else if (model->isOfType(storm::models::ModelType::Ctmc)) {
        verifyPropertiesAtSamplePoints<storm::modelchecker::SparseCtmcInstantiationModelChecker, storm::models::sparse::Ctmc<ValueType>, ValueType,
                                       SolveValueType>(*model->template as<storm::models::sparse::Ctmc<ValueType>>(), input, samples);
    } else if (model->isOfType(storm::models::ModelType::Mdp)) {
        verifyPropertiesAtSamplePoints<storm::modelchecker::SparseMdpInstantiationModelChecker, storm::models::sparse::Mdp<ValueType>, ValueType,
                                       SolveValueType>(*model->template as<storm::models::sparse::Mdp<ValueType>>(), input, samples);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Sampling is currently only supported for DTMCs, CTMCs and MDPs.");
    }
}

template<typename ValueType>
SampleInformation<ValueType> parseSamples(std::shared_ptr<storm::models::ModelBase> const& model, std::string const& sampleString, bool graphPreserving) {
    STORM_LOG_THROW(!model || model->isSparseModel(), storm::exceptions::NotSupportedException, "Sampling is only supported for sparse models.");

    SampleInformation<ValueType> sampleInfo(graphPreserving);
    if (sampleString.empty()) {
        return sampleInfo;
    }

    // Get all parameters from the model.
    std::set<typename storm::utility::parametric::VariableType<ValueType>::type> modelParameters;
    auto const& sparseModel = *model->as<storm::models::sparse::Model<ValueType>>();
    modelParameters = storm::models::sparse::getProbabilityParameters(sparseModel);
    auto rewParameters = storm::models::sparse::getRewardParameters(sparseModel);
    modelParameters.insert(rewParameters.begin(), rewParameters.end());

    std::vector<std::string> cartesianProducts;
    boost::split(cartesianProducts, sampleString, boost::is_any_of(";"));
    for (auto& product : cartesianProducts) {
        boost::trim(product);

        // Get the values string for each variable.
        std::vector<std::string> valuesForVariables;
        boost::split(valuesForVariables, product, boost::is_any_of(","));
        for (auto& values : valuesForVariables) {
            boost::trim(values);
        }

        std::set<typename storm::utility::parametric::VariableType<ValueType>::type> encounteredParameters;
        sampleInfo.cartesianProducts.emplace_back();
        auto& newCartesianProduct = sampleInfo.cartesianProducts.back();
        for (auto const& varValues : valuesForVariables) {
            auto equalsPosition = varValues.find("=");
            STORM_LOG_THROW(equalsPosition != varValues.npos, storm::exceptions::WrongFormatException, "Incorrect format of samples.");
            std::string variableName = varValues.substr(0, equalsPosition);
            boost::trim(variableName);
            std::string values = varValues.substr(equalsPosition + 1);
            boost::trim(values);

            bool foundParameter = false;
            typename storm::utility::parametric::VariableType<ValueType>::type theParameter;
            for (auto const& parameter : modelParameters) {
                std::stringstream parameterStream;
                parameterStream << parameter;
                if (parameterStream.str() == variableName) {
                    foundParameter = true;
                    theParameter = parameter;
                    encounteredParameters.insert(parameter);
                }
            }
            STORM_LOG_THROW(foundParameter, storm::exceptions::WrongFormatException, "Unknown parameter '" << variableName << "'.");

            std::vector<std::string> splitValues;
            boost::split(splitValues, values, boost::is_any_of(":"));
            STORM_LOG_THROW(!splitValues.empty(), storm::exceptions::WrongFormatException, "Expecting at least one value per parameter.");

            auto& list = newCartesianProduct[theParameter];

            for (auto& value : splitValues) {
                boost::trim(value);
                list.push_back(storm::utility::convertNumber<typename storm::utility::parametric::CoefficientType<ValueType>::type>(value));
            }
        }

        STORM_LOG_THROW(encounteredParameters == modelParameters, storm::exceptions::WrongFormatException,
                        "Variables for all parameters are required when providing samples.");
    }

    return sampleInfo;
}

template<typename ValueType>
void sampleDerivatives(std::shared_ptr<storm::models::sparse::Model<ValueType>> model, cli::SymbolicInput const& input,
                       std::string const& instantiationString) {
    STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Dtmc), storm::exceptions::NotSupportedException,
                    "Gradient descent is currently only supported for DTMCs.");
    std::shared_ptr<storm::models::sparse::Dtmc<ValueType>> dtmc = model->template as<storm::models::sparse::Dtmc<ValueType>>();

    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas = storm::api::extractFormulasFromProperties(input.properties);
    auto formula = formulas[0];

    STORM_LOG_THROW(formula->isProbabilityOperatorFormula() || formula->isRewardOperatorFormula(), storm::exceptions::NotSupportedException,
                    "Input formula needs to be either a probability operator formula or a reward operator formula.");

    auto vars = storm::models::sparse::getProbabilityParameters(*dtmc);

    boost::optional<std::string> rewardModel = boost::none;
    if (formula->isRewardOperatorFormula()) {
        if (formula->asRewardOperatorFormula().hasRewardModelName()) {
            rewardModel = std::string(formula->asRewardOperatorFormula().getRewardModelName());
        } else {
            rewardModel = std::string("");
        }
        for (auto const& rewardParameter : storm::models::sparse::getRewardParameters(*dtmc)) {
            vars.insert(rewardParameter);
        }
    }
    // Use the SparseDerivativeInstantiationModelChecker to retrieve the derivative at an instantiation that is input by the user

    std::unordered_map<std::string, std::string> keyValue = storm::parser::parseKeyValueString(instantiationString);
    std::map<typename storm::utility::parametric::VariableType<ValueType>::type, typename storm::utility::parametric::CoefficientType<ValueType>::type>
        instantiation;
    for (auto const& pair : keyValue) {
        auto variable = carl::VariablePool::getInstance().findVariableWithName(pair.first);
        auto value = storm::utility::convertNumber<typename storm::utility::parametric::CoefficientType<ValueType>::type>(pair.second);
        instantiation.emplace(variable, value);
    }

    derivative::SparseDerivativeInstantiationModelChecker<ValueType, storm::RationalNumber> modelChecker(*dtmc);

    storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalNumber> referenceCheckTask(*formula);
    std::shared_ptr<storm::logic::Formula> formulaWithoutBound;
    if (!referenceCheckTask.isRewardModelSet()) {
        formulaWithoutBound = std::make_shared<storm::logic::ProbabilityOperatorFormula>(
            formulas[0]->asProbabilityOperatorFormula().getSubformula().asSharedPointer(), storm::logic::OperatorInformation(boost::none, boost::none));
    } else {
        // No worries, this works as intended, the API is just weird.
        formulaWithoutBound = std::make_shared<storm::logic::RewardOperatorFormula>(formulas[0]->asRewardOperatorFormula().getSubformula().asSharedPointer());
    }
    const storm::modelchecker::CheckTask<storm::logic::Formula, ValueType> checkTask =
        storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>(*formulaWithoutBound);
    modelChecker.specifyFormula(Environment(), checkTask);

    for (auto const& parameter : vars) {
        std::cout << "Derivative w.r.t. " << parameter << ": ";

        auto result = modelChecker.check(Environment(), instantiation, parameter);
        std::cout << *result << '\n';
    }
}
}  // namespace storm::pars