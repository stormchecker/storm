#include "storm-dft/modelchecker/DFTModelChecker.h"

#include "storm-dft/api/storm-dft.h"
#include "storm-dft/builder/ExplicitDFTModelBuilder.h"
#include "storm-dft/settings/modules/DftIOSettings.h"
#include "storm-dft/utility/SymmetryFinder.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/builder/ParallelCompositionBuilder.h"
#include "storm/exceptions/InvalidModelException.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/ModelType.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/utility/bitoperations.h"

namespace storm::dft {
namespace modelchecker {

template<typename ValueType>
typename DFTModelChecker<ValueType>::dft_results DFTModelChecker<ValueType>::check(
    storm::dft::storage::DFT<ValueType> const& origDft, std::vector<std::shared_ptr<const storm::logic::Formula>> const& properties, bool symred,
    bool allowModularisation, storm::dft::utility::RelevantEvents const& relevantEvents, bool allowDCForRelevant, double approximationError,
    storm::dft::builder::ApproximationHeuristic approximationHeuristic, bool eliminateChains, storm::transformer::EliminationLabelBehavior labelBehavior) {
    totalTimer.start();
    dft_results results;

    // Check well-formedness of DFT
    auto wellFormedResult = storm::dft::api::isWellFormed(origDft, true);
    STORM_LOG_THROW(wellFormedResult.first, storm::exceptions::InvalidModelException, "DFT is not well-formed for analysis: " << wellFormedResult.second);

    // Optimizing DFT for modularisation
    storm::dft::storage::DFT<ValueType> dft = origDft;
    if (allowModularisation) {
        dft = origDft.optimize();
    }

    // TODO: check that all paths reach the target state for approximation

    // Checking DFT
    // TODO: distinguish for all properties, not only for first one
    if (properties[0]->isTimeOperatorFormula() && allowModularisation) {
        // Use parallel composition as modularisation approach for expected time
        std::shared_ptr<storm::models::sparse::Model<ValueType>> model =
            buildModelViaComposition(dft, properties, symred, true, relevantEvents, allowDCForRelevant);
        // Model checking
        std::vector<ValueType> resultsValue = checkModel(model, properties);
        for (ValueType result : resultsValue) {
            results.push_back(result);
        }
    } else {
        results = checkHelper(dft, properties, symred, allowModularisation, relevantEvents, allowDCForRelevant, approximationError, approximationHeuristic,
                              eliminateChains, labelBehavior);
    }
    totalTimer.stop();
    return results;
}

template<typename ValueType>
typename DFTModelChecker<ValueType>::dft_results DFTModelChecker<ValueType>::checkHelper(
    storm::dft::storage::DFT<ValueType> const& dft, property_vector const& properties, bool symred, bool allowModularisation,
    storm::dft::utility::RelevantEvents const& relevantEvents, bool allowDCForRelevant, double approximationError,
    storm::dft::builder::ApproximationHeuristic approximationHeuristic, bool eliminateChains, storm::transformer::EliminationLabelBehavior labelBehavior) {
    STORM_LOG_TRACE("Check helper called");
    std::vector<storm::dft::storage::DFT<ValueType>> dfts;
    bool invResults = false;
    size_t nrK = 0;  // K out of M
    size_t nrM = 0;  // K out of M

    // Try modularisation
    if (allowModularisation) {
        switch (dft.getTopLevelType()) {
            case storm::dft::storage::elements::DFTElementType::AND:
                STORM_LOG_TRACE("top modularisation called AND");
                dfts = dft.topModularisation();
                nrK = dfts.size();
                nrM = dfts.size();
                break;
            case storm::dft::storage::elements::DFTElementType::OR:
                STORM_LOG_TRACE("top modularisation called OR");
                dfts = dft.topModularisation();
                nrK = 0;
                nrM = dfts.size();
                invResults = true;
                break;
            case storm::dft::storage::elements::DFTElementType::VOT:
                STORM_LOG_TRACE("top modularisation called VOT");
                dfts = dft.topModularisation();
                nrK = std::static_pointer_cast<storm::dft::storage::elements::DFTVot<ValueType> const>(dft.getTopLevelElement())->threshold();
                nrM = dfts.size();
                if (nrK <= nrM / 2) {
                    nrK -= 1;
                    invResults = true;
                }
                break;
            default:
                // No static gate -> no modularisation applicable
                break;
        }
    }

    // Perform modularisation
    if (dfts.size() > 1) {
        STORM_LOG_DEBUG("Modularisation of " << dft.getTopLevelElement()->name() << " into " << dfts.size() << " submodules.");
        // TODO: compute simultaneously
        dft_results results;
        for (auto property : properties) {
            if (!property->isProbabilityOperatorFormula()) {
                STORM_LOG_WARN("Could not check property: " << *property);
            } else {
                // Recursively call model checking
                std::vector<ValueType> res;
                for (auto const& ft : dfts) {
                    // TODO: allow approximation in modularisation
                    dft_results ftResults = checkHelper(ft, {property}, symred, true, relevantEvents, allowDCForRelevant, 0.0);
                    STORM_LOG_ASSERT(ftResults.size() == 1, "Wrong number of results");
                    res.push_back(boost::get<ValueType>(ftResults[0]));
                }

                // Combine modularisation results
                STORM_LOG_TRACE("Combining all results... K=" << nrK << "; M=" << nrM << "; invResults=" << (invResults ? "On" : "Off"));
                ValueType result = storm::utility::zero<ValueType>();
                int limK = invResults ? -1 : nrM + 1;
                int chK = invResults ? -1 : 1;
                for (int cK = nrK; cK != limK; cK += chK) {
                    STORM_LOG_ASSERT(cK >= 0, "ck negative.");
                    uint64_t permutation = smallestIntWithNBitsSet(static_cast<uint64_t>(cK));
                    do {
                        STORM_LOG_TRACE("Permutation=" << permutation);
                        ValueType permResult = storm::utility::one<ValueType>();
                        for (size_t i = 0; i < res.size(); ++i) {
                            if (permutation & (1ul << i)) {
                                permResult *= res[i];
                            } else {
                                permResult *= storm::utility::one<ValueType>() - res[i];
                            }
                        }
                        STORM_LOG_TRACE("Result for permutation:" << permResult);
                        permutation = nextBitPermutation(permutation);
                        result += permResult;
                    } while (permutation < (1ul << nrM) && permutation != 0);
                }
                if (invResults) {
                    result = storm::utility::one<ValueType>() - result;
                }
                results.push_back(result);
            }
        }
        return results;
    } else {
        // No modularisation was possible
        return checkDFT(dft, properties, symred, relevantEvents, allowDCForRelevant, approximationError, approximationHeuristic, eliminateChains,
                        labelBehavior);
    }
}

template<typename ValueType>
std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> DFTModelChecker<ValueType>::buildModelViaComposition(
    storm::dft::storage::DFT<ValueType> const& dft, property_vector const& properties, bool symred, bool allowModularisation,
    storm::dft::utility::RelevantEvents const& relevantEvents, bool allowDCForRelevant) {
    // TODO: use approximation?
    STORM_LOG_TRACE("Build model via composition");
    std::vector<storm::dft::storage::DFT<ValueType>> dfts;
    bool isAnd = true;

    // Try modularisation
    if (allowModularisation) {
        switch (dft.getTopLevelType()) {
            case storm::dft::storage::elements::DFTElementType::AND:
                STORM_LOG_TRACE("top modularisation called AND");
                dfts = dft.topModularisation();
                STORM_LOG_TRACE("Modularisation into " << dfts.size() << " submodules.");
                isAnd = true;
                break;
            case storm::dft::storage::elements::DFTElementType::OR:
                STORM_LOG_TRACE("top modularisation called OR");
                dfts = dft.topModularisation();
                STORM_LOG_TRACE("Modularsation into " << dfts.size() << " submodules.");
                isAnd = false;
                break;
            case storm::dft::storage::elements::DFTElementType::VOT:
                // TODO enable modularisation for voting gate
                break;
            default:
                // No static gate -> no modularisation applicable
                break;
        }
    }

    // Perform modularisation via parallel composition
    if (dfts.size() > 1) {
        STORM_LOG_TRACE("Recursive CHECK Call");
        bool firstTime = true;
        std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> composedModel;
        for (auto const& ft : dfts) {
            STORM_LOG_DEBUG("Building Model via parallel composition...");
            explorationTimer.start();

            ft.setRelevantEvents(relevantEvents, allowDCForRelevant);
            // Find symmetries
            storm::dft::storage::DftSymmetries symmetries;
            if (symred) {
                symmetries = storm::dft::utility::SymmetryFinder<ValueType>::findSymmetries(ft);
                STORM_LOG_DEBUG("Found " << symmetries.nrSymmetries() << " symmetries.");
                STORM_LOG_TRACE("Symmetries: \n" << symmetries);
            }

            // Build a single CTMC
            STORM_LOG_DEBUG("Building Model from DFT with top level element " << *ft.getElement(ft.getTopLevelIndex()) << " ...");
            storm::dft::builder::ExplicitDFTModelBuilder<ValueType> builder(ft, symmetries);
            builder.buildModel(0, 0.0);
            std::shared_ptr<storm::models::sparse::Model<ValueType>> model = builder.getModel();
            explorationTimer.stop();

            STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Ctmc), storm::exceptions::NotSupportedException,
                            "Parallel composition only applicable for CTMCs");
            std::shared_ptr<storm::models::sparse::Ctmc<ValueType>> ctmc = model->template as<storm::models::sparse::Ctmc<ValueType>>();

            // Apply bisimulation to new CTMC
            bisimulationTimer.start();
            ctmc = storm::api::performDeterministicSparseBisimulationMinimization<storm::models::sparse::Ctmc<ValueType>>(
                       ctmc, properties, storm::storage::BisimulationType::Weak)
                       ->template as<storm::models::sparse::Ctmc<ValueType>>();
            bisimulationTimer.stop();

            if (firstTime) {
                composedModel = ctmc;
                firstTime = false;
            } else {
                composedModel = storm::builder::ParallelCompositionBuilder<ValueType>::compose(composedModel, ctmc, isAnd);
            }

            // Apply bisimulation to parallel composition
            bisimulationTimer.start();
            composedModel = storm::api::performDeterministicSparseBisimulationMinimization<storm::models::sparse::Ctmc<ValueType>>(
                                composedModel, properties, storm::storage::BisimulationType::Weak)
                                ->template as<storm::models::sparse::Ctmc<ValueType>>();
            bisimulationTimer.stop();

            STORM_LOG_DEBUG("No. states (Composed): " << composedModel->getNumberOfStates());
            STORM_LOG_DEBUG("No. transitions (Composed): " << composedModel->getNumberOfTransitions());
            if (composedModel->getNumberOfStates() <= 15) {
                STORM_LOG_TRACE("Transition matrix: \n" << composedModel->getTransitionMatrix());
            } else {
                STORM_LOG_TRACE("Transition matrix: too big to print");
            }
        }
        if (printInfo) {
            composedModel->printModelInformationToStream(std::cout);
        }
        return composedModel;
    } else {
        // No composition was possible
        explorationTimer.start();

        dft.setRelevantEvents(relevantEvents, allowDCForRelevant);

        // Find symmetries
        storm::dft::storage::DftSymmetries symmetries;
        if (symred) {
            symmetries = storm::dft::utility::SymmetryFinder<ValueType>::findSymmetries(dft);
            STORM_LOG_DEBUG("Found " << symmetries.nrSymmetries() << " symmetries.");
            STORM_LOG_TRACE("Symmetries: \n" << symmetries);
        }
        // Build a single CTMC
        STORM_LOG_DEBUG("Building Model...");

        storm::dft::builder::ExplicitDFTModelBuilder<ValueType> builder(dft, symmetries);
        builder.buildModel(0, 0.0);
        std::shared_ptr<storm::models::sparse::Model<ValueType>> model = builder.getModel();
        if (printInfo) {
            model->printModelInformationToStream(std::cout);
        }
        explorationTimer.stop();
        STORM_LOG_THROW(model->isOfType(storm::models::ModelType::Ctmc), storm::exceptions::NotSupportedException,
                        "Parallel composition only applicable for CTMCs");
        return model->template as<storm::models::sparse::Ctmc<ValueType>>();
    }
}

template<typename ValueType>
typename DFTModelChecker<ValueType>::dft_results DFTModelChecker<ValueType>::checkDFT(
    storm::dft::storage::DFT<ValueType> const& dft, property_vector const& properties, bool symred, storm::dft::utility::RelevantEvents const& relevantEvents,
    bool allowDCForRelevant, double approximationError, storm::dft::builder::ApproximationHeuristic approximationHeuristic, bool eliminateChains,
    storm::transformer::EliminationLabelBehavior labelBehavior) {
    explorationTimer.start();
    auto ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    auto dftIOSettings = storm::settings::getModule<storm::dft::settings::modules::DftIOSettings>();

    dft.setRelevantEvents(relevantEvents, allowDCForRelevant);

    // Find symmetries
    storm::dft::storage::DftSymmetries symmetries;
    if (symred) {
        symmetries = storm::dft::utility::SymmetryFinder<ValueType>::findSymmetries(dft);
        STORM_LOG_DEBUG("Found " << symmetries.nrSymmetries() << " symmetries.");
        STORM_LOG_TRACE("Symmetries: \n" << symmetries);
    }

    if (approximationError > 0.0) {
        // Build approximate Markov Automata for lower and upper bound
        approximation_result approxResult = std::make_pair(storm::utility::zero<ValueType>(), storm::utility::zero<ValueType>());
        std::shared_ptr<storm::models::sparse::Model<ValueType>> model;
        std::vector<ValueType> newResult;
        storm::dft::builder::ExplicitDFTModelBuilder<ValueType> builder(dft, symmetries);

        // TODO: compute approximation for all properties simultaneously?
        std::shared_ptr<const storm::logic::Formula> property = properties[0];
        if (properties.size() > 1) {
            STORM_LOG_WARN("Computing approximation only for first property: " << *property);
        }

        bool probabilityFormula = property->isProbabilityOperatorFormula();
        STORM_LOG_ASSERT((property->isTimeOperatorFormula() && !probabilityFormula) || (!property->isTimeOperatorFormula() && probabilityFormula),
                         "Probability formula not initialized correctly");
        size_t iteration = 0;
        do {
            // Iteratively build finer models
            if (iteration > 0) {
                explorationTimer.start();
            }
            STORM_LOG_DEBUG("Building model...");
            // TODO refine model using existing model and MC results
            builder.buildModel(iteration, approximationError, approximationHeuristic);
            explorationTimer.stop();
            buildingTimer.start();

            // TODO: possible to do bisimulation on approximated model and not on concrete one?

            // Build model for lower bound
            STORM_LOG_DEBUG("Getting model for lower bound...");
            model = builder.getModelApproximation(true, !probabilityFormula);
            // We only output the info from the lower bound as the info for the upper bound is the same
            if (printInfo && dftIOSettings.isShowDftStatisticsSet()) {
                std::cout << "Model in iteration " << (iteration + 1) << ":\n";
                model->printModelInformationToStream(std::cout);
            }
            buildingTimer.stop();

            if (ioSettings.isExportExplicitSet()) {
                std::vector<std::string> parameterNames;
                // TODO fill parameter names
                storm::api::exportSparseModelAsDrn(model, ioSettings.getExportExplicitFilename(), parameterNames,
                                                   !ioSettings.isExplicitExportPlaceholdersDisabled());
            }

            // Check lower bounds
            newResult = checkModel(model, {property});
            STORM_LOG_ASSERT(newResult.size() == 1, "Wrong size for result vector.");
            STORM_LOG_ASSERT(iteration == 0 || storm::utility::isNonNegative(newResult[0] - approxResult.first),
                             "New under-approximation " << newResult[0] << " is smaller than old result " << approxResult.first);
            approxResult.first = newResult[0];

            // Build model for upper bound
            STORM_LOG_DEBUG("Getting model for upper bound...");
            buildingTimer.start();
            model = builder.getModelApproximation(false, !probabilityFormula);
            buildingTimer.stop();
            // Check upper bound
            newResult = checkModel(model, {property});
            STORM_LOG_ASSERT(newResult.size() == 1, "Wrong size for result vector.");
            STORM_LOG_ASSERT(iteration == 0 || storm::utility::isNonNegative(approxResult.second - newResult[0]),
                             "New over-approximation " << newResult[0] << " is greater than old result " << approxResult.second);
            approxResult.second = newResult[0];

            STORM_LOG_ASSERT(storm::utility::isNonNegative(approxResult.second - approxResult.first),
                             "Under-approximation " << approxResult.first << " is greater than over-approximation " << approxResult.second);
            totalTimer.stop();
            if (printInfo && dftIOSettings.isShowDftStatisticsSet()) {
                std::cout << "Result after iteration " << (iteration + 1) << ": (" << approxResult.first << ", " << approxResult.second << ")\n";
                printTimings();
                std::cout << '\n';
            } else {
                STORM_LOG_DEBUG("Result after iteration " << (iteration + 1) << ": (" << approxResult.first << ", " << approxResult.second << ")");
            }

            totalTimer.start();
            STORM_LOG_THROW(!storm::utility::isInfinity<ValueType>(approxResult.first) && !storm::utility::isInfinity<ValueType>(approxResult.second),
                            storm::exceptions::NotSupportedException, "Approximation does not work if result might be infinity.");
            ++iteration;
        } while (!isApproximationSufficient(approxResult.first, approxResult.second, approximationError, probabilityFormula));

        // STORM_LOG_INFO("Finished approximation after " << iteration << " iteration" << (iteration > 1 ? "s." : "."));
        if (printInfo) {
            model->printModelInformationToStream(std::cout);
        }
        dft_results results;
        results.push_back(approxResult);
        return results;
    } else {
        // Build a single Markov Automaton
        STORM_LOG_DEBUG("Building Model...");
        storm::dft::builder::ExplicitDFTModelBuilder<ValueType> builder(dft, symmetries);
        builder.buildModel(0, 0.0);
        std::shared_ptr<storm::models::sparse::Model<ValueType>> model = builder.getModel();
        if (eliminateChains && model->isOfType(storm::models::ModelType::MarkovAutomaton)) {
            auto ma = std::static_pointer_cast<storm::models::sparse::MarkovAutomaton<ValueType>>(model);
            model = storm::transformer::NonMarkovianChainTransformer<ValueType>::eliminateNonmarkovianStates(ma, labelBehavior);
        }
        explorationTimer.stop();

        // Print model information
        if (printInfo) {
            model->printModelInformationToStream(std::cout);
        }

        // Export the model if required
        // TODO move this outside of the model checker?
        if (ioSettings.isExportExplicitSet()) {
            std::vector<std::string> parameterNames;
            // TODO fill parameter names
            storm::api::exportSparseModelAsDrn(model, ioSettings.getExportExplicitFilename(), parameterNames,
                                               !ioSettings.isExplicitExportPlaceholdersDisabled());
        }
        if (ioSettings.isExportDotSet()) {
            storm::api::exportSparseModelAsDot(model, ioSettings.getExportDotFilename(), ioSettings.getExportDotMaxWidth());
        }

        // Model checking
        std::vector<ValueType> resultsValue = checkModel(model, properties);
        dft_results results;
        for (ValueType result : resultsValue) {
            results.push_back(result);
        }
        return results;
    }
}

template<typename ValueType>
std::vector<ValueType> DFTModelChecker<ValueType>::checkModel(std::shared_ptr<storm::models::sparse::Model<ValueType>>& model,
                                                              property_vector const& properties) {
    // Bisimulation
    if (model->isOfType(storm::models::ModelType::Ctmc) && storm::settings::getModule<storm::settings::modules::GeneralSettings>().isBisimulationSet()) {
        bisimulationTimer.start();
        STORM_LOG_DEBUG("Bisimulation...");
        model = storm::api::performDeterministicSparseBisimulationMinimization<storm::models::sparse::Ctmc<ValueType>>(
                    model->template as<storm::models::sparse::Ctmc<ValueType>>(), properties, storm::storage::BisimulationType::Weak)
                    ->template as<storm::models::sparse::Ctmc<ValueType>>();
        STORM_LOG_DEBUG("No. states (Bisimulation): " << model->getNumberOfStates());
        STORM_LOG_DEBUG("No. transitions (Bisimulation): " << model->getNumberOfTransitions());
        bisimulationTimer.stop();
    }

    // Check the model
    STORM_LOG_DEBUG("Model checking...");
    modelCheckingTimer.start();
    std::vector<ValueType> results;

    // Check each property
    storm::utility::Stopwatch singleModelCheckingTimer;
    for (auto property : properties) {
        singleModelCheckingTimer.reset();
        singleModelCheckingTimer.start();
        // STORM_PRINT_AND_LOG("Model checking property " << *property << " ...\n");
        std::unique_ptr<storm::modelchecker::CheckResult> result(
            storm::api::verifyWithSparseEngine<ValueType>(model, storm::api::createTask<ValueType>(property, true)));

        if (result) {
            result->filter(storm::modelchecker::ExplicitQualitativeCheckResult<ValueType>(model->getInitialStates()));
            ValueType resultValue = result->asExplicitQuantitativeCheckResult<ValueType>().getValueMap().begin()->second;
            results.push_back(resultValue);
        } else {
            STORM_LOG_WARN("The property '" << *property << "' could not be checked with the current settings.");
            results.push_back(-storm::utility::one<ValueType>());
        }
        // STORM_PRINT_AND_LOG("Result (initial states): " << resultValue << '\n');
        singleModelCheckingTimer.stop();
        // STORM_PRINT_AND_LOG("Time for model checking: " << singleModelCheckingTimer << ".\n");
    }
    modelCheckingTimer.stop();
    STORM_LOG_DEBUG("Model checking done.");
    return results;
}

template<typename ValueType>
bool DFTModelChecker<ValueType>::isApproximationSufficient(ValueType, ValueType, double, bool) {
    STORM_LOG_THROW(false, storm::exceptions::NotImplementedException, "Approximation works only for double.");
}

template<>
bool DFTModelChecker<double>::isApproximationSufficient(double lowerBound, double upperBound, double approximationError, bool relative) {
    STORM_LOG_THROW(!std::isnan(lowerBound) && !std::isnan(upperBound), storm::exceptions::NotSupportedException,
                    "Approximation does not work if result is NaN.");
    if (relative) {
        return upperBound - lowerBound <= approximationError;
    } else {
        return upperBound - lowerBound <= approximationError * (lowerBound + upperBound) / 2;
    }
}

template<typename ValueType>
void DFTModelChecker<ValueType>::printTimings(std::ostream& os) {
    os << "Times:\n";
    os << "Exploration:\t" << explorationTimer << '\n';
    os << "Building:\t" << buildingTimer << '\n';
    os << "Bisimulation:\t" << bisimulationTimer << '\n';
    os << "Modelchecking:\t" << modelCheckingTimer << '\n';
    os << "Total:\t\t" << totalTimer << '\n';
}

template<typename ValueType>
void DFTModelChecker<ValueType>::printResults(dft_results const& results, std::ostream& os) {
    bool first = true;
    os << "Result: [";
    for (auto result : results) {
        if (first) {
            first = false;
        } else {
            os << ", ";
        }
        boost::variant<std::ostream&> stream(os);
        boost::apply_visitor(ResultOutputVisitor(), result, stream);
    }
    os << "]\n";
}

template class DFTModelChecker<double>;
template class DFTModelChecker<storm::RationalFunction>;

}  // namespace modelchecker
}  // namespace storm::dft
