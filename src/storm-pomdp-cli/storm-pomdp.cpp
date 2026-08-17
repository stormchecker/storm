#include <typeinfo>

#include "storm-cli-utilities/cli.h"
#include "storm-cli-utilities/model-handling.h"
#include "storm-pomdp-cli/settings/PomdpSettings.h"
#include "storm-pomdp-cli/settings/modules/BeliefExplorationSettings.h"
#include "storm-pomdp-cli/settings/modules/POMDPSettings.h"
#include "storm-pomdp-cli/settings/modules/QualitativePOMDPAnalysisSettings.h"
#include "storm-pomdp-cli/settings/modules/ToParametricSettings.h"
#include "storm-pomdp/analysis/FormulaInformation.h"
#include "storm-pomdp/analysis/IterativePolicySearch.h"
#include "storm-pomdp/analysis/JaniBeliefSupportMdpGenerator.h"
#include "storm-pomdp/analysis/OneShotPolicySearch.h"
#include "storm-pomdp/analysis/QualitativeAnalysisOnGraphs.h"
#include "storm-pomdp/analysis/UniqueObservationStates.h"
#include "storm-pomdp/modelchecker/BeliefExplorationPomdpModelChecker.h"
#include "storm-pomdp/transformer/ApplyFiniteSchedulerToPomdp.h"
#include "storm-pomdp/transformer/BinaryPomdpTransformer.h"
#include "storm-pomdp/transformer/GlobalPOMDPSelfLoopEliminator.h"
#include "storm-pomdp/transformer/GlobalPomdpMecChoiceEliminator.h"
#include "storm-pomdp/transformer/KnownProbabilityTransformer.h"
#include "storm-pomdp/transformer/PomdpMemoryUnfolder.h"
#include "storm/analysis/GraphConditions.h"
#include "storm/api/storm.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/settings/modules/DebugSettings.h"
#include "storm/settings/modules/GeneralSettings.h"
#include "storm/transformer/MakePOMDPCanonic.h"
#include "storm/utility/NumberTraits.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/Stopwatch.h"

namespace storm {
namespace pomdp {
namespace cli {

/// Perform preprocessings based on the graph structure (if requested or necessary). Return true, if some preprocessing has been done
template<typename ValueType>
bool performPreprocessing(std::shared_ptr<storm::models::sparse::Pomdp<ValueType>>& pomdp, storm::pomdp::analysis::FormulaInformation& formulaInfo,
                          storm::logic::Formula const& formula) {
    auto const& pomdpSettings = storm::settings::getModule<storm::settings::modules::POMDPSettings>();
    bool preprocessingPerformed = false;
    if (pomdpSettings.isSelfloopReductionSet()) {
        storm::transformer::GlobalPOMDPSelfLoopEliminator<ValueType> selfLoopEliminator(*pomdp);
        if (selfLoopEliminator.preservesFormula(formula)) {
            STORM_PRINT_AND_LOG("Eliminating self-loop choices ...");
            uint64_t oldChoiceCount = pomdp->getNumberOfChoices();
            pomdp = selfLoopEliminator.transform();
            STORM_PRINT_AND_LOG(oldChoiceCount - pomdp->getNumberOfChoices() << " choices eliminated through self-loop elimination.\n");
            preprocessingPerformed = true;
        } else {
            STORM_PRINT_AND_LOG("Not eliminating self-loop choices as it does not preserve the formula.\n");
        }
    }
    if (pomdpSettings.isQualitativeReductionSet() && formulaInfo.isNonNestedReachabilityProbability()) {
        storm::analysis::QualitativeAnalysisOnGraphs<ValueType> qualitativeAnalysis(*pomdp);
        STORM_PRINT_AND_LOG("Computing states with probability 0 ...");
        storm::storage::BitVector prob0States = qualitativeAnalysis.analyseProb0(formula.asProbabilityOperatorFormula());
        std::cout << prob0States << '\n';
        STORM_PRINT_AND_LOG(" done. " << prob0States.getNumberOfSetBits() << " states found.\n");
        STORM_PRINT_AND_LOG("Computing states with probability 1 ...");
        storm::storage::BitVector prob1States = qualitativeAnalysis.analyseProb1(formula.asProbabilityOperatorFormula());
        STORM_PRINT_AND_LOG(" done. " << prob1States.getNumberOfSetBits() << " states found.\n");
        storm::pomdp::transformer::KnownProbabilityTransformer<ValueType> kpt = storm::pomdp::transformer::KnownProbabilityTransformer<ValueType>();
        pomdp = kpt.transform(*pomdp, prob0States, prob1States);
        // Update formulaInfo to changes from Preprocessing
        formulaInfo.updateTargetStates(*pomdp, std::move(prob1States));
        formulaInfo.updateSinkStates(*pomdp, std::move(prob0States));
        preprocessingPerformed = true;
    }
    return preprocessingPerformed;
}

template<typename ValueType>
void printResult(ValueType const& lowerBound, ValueType const& upperBound) {
    if (lowerBound == upperBound) {
        if (storm::utility::isInfinity(lowerBound)) {
            STORM_PRINT_AND_LOG("inf");
        } else {
            STORM_PRINT_AND_LOG(lowerBound);
        }
    } else if (storm::utility::isInfinity<ValueType>(-lowerBound)) {
        if (storm::utility::isInfinity(upperBound)) {
            STORM_PRINT_AND_LOG("[-inf, inf] (width=inf)");
        } else {
            // Only upper bound is known
            STORM_PRINT_AND_LOG("≤ " << upperBound);
        }
    } else if (storm::utility::isInfinity(upperBound)) {
        STORM_PRINT_AND_LOG("≥ " << lowerBound);
    } else {
        STORM_PRINT_AND_LOG("[" << lowerBound << ", " << upperBound << "] (width=" << ValueType(upperBound - lowerBound) << ")");
    }
    if (storm::NumberTraits<ValueType>::IsExact) {
        STORM_PRINT_AND_LOG(" (approx. ");
        double roundedLowerBound =
            storm::utility::isInfinity<ValueType>(-lowerBound) ? -storm::utility::infinity<double>() : storm::utility::convertNumber<double>(lowerBound);
        double roundedUpperBound =
            storm::utility::isInfinity(upperBound) ? storm::utility::infinity<double>() : storm::utility::convertNumber<double>(upperBound);
        printResult(roundedLowerBound, roundedUpperBound);
        STORM_PRINT_AND_LOG(")");
    }
}

MemlessSearchOptions fillMemlessSearchOptionsFromSettings() {
    storm::pomdp::MemlessSearchOptions options;
    auto const& qualSettings = storm::settings::getModule<storm::settings::modules::QualitativePOMDPAnalysisSettings>();

    options.onlyDeterministicStrategies = qualSettings.isOnlyDeterministicSet();
    uint64_t loglevel = 0;
    // TODO a big ugly, but we have our own loglevels (for technical reasons)
    if (storm::utility::getLogLevel() == l3pp::LogLevel::INFO) {
        loglevel = 1;
    } else if (storm::utility::getLogLevel() == l3pp::LogLevel::DEBUG) {
        loglevel = 2;
    } else if (storm::utility::getLogLevel() == l3pp::LogLevel::TRACE) {
        loglevel = 3;
    }
    options.setDebugLevel(loglevel);
    options.validateEveryStep = qualSettings.validateIntermediateSteps();
    options.validateResult = qualSettings.validateFinalResult();

    options.pathVariableType = storm::pomdp::pathVariableTypeFromString(qualSettings.getLookaheadType());

    if (qualSettings.isExportSATCallsSet()) {
        options.setExportSATCalls(qualSettings.getExportSATCallsPath());
    }

    return options;
}

template<typename ValueType>
void performQualitativeAnalysis(std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> const& origpomdp,
                                storm::pomdp::analysis::FormulaInformation const& formulaInfo, storm::logic::Formula const& formula) {
    auto const& qualSettings = storm::settings::getModule<storm::settings::modules::QualitativePOMDPAnalysisSettings>();
    auto const& coreSettings = storm::settings::getModule<storm::settings::modules::CoreSettings>();
    std::stringstream sstr;
    origpomdp->printModelInformationToStream(sstr);
    STORM_LOG_INFO(sstr.str());
    STORM_LOG_THROW(formulaInfo.isNonNestedReachabilityProbability(), storm::exceptions::NotSupportedException,
                    "Qualitative memoryless scheduler search is not implemented for this property type.");
    STORM_LOG_TRACE("Run qualitative preprocessing...");
    storm::models::sparse::Pomdp<ValueType> pomdp(*origpomdp);
    storm::analysis::QualitativeAnalysisOnGraphs<ValueType> qualitativeAnalysis(pomdp);
    // After preprocessing, this might be done cheaper.
    storm::storage::BitVector surelyNotAlmostSurelyReachTarget = qualitativeAnalysis.analyseProbSmaller1(formula.asProbabilityOperatorFormula());
    pomdp.getTransitionMatrix().makeRowGroupsAbsorbing(surelyNotAlmostSurelyReachTarget);
    storm::storage::BitVector targetStates = qualitativeAnalysis.analyseProb1(formula.asProbabilityOperatorFormula());
    bool computedSomething = false;
    if (qualSettings.isMemlessSearchSet()) {
        computedSomething = true;
        std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();
        uint64_t lookahead = qualSettings.getLookahead();
        if (lookahead == 0) {
            lookahead = pomdp.getNumberOfStates();
        }
        if (qualSettings.getMemlessSearchMethod() == "one-shot") {
            storm::pomdp::OneShotPolicySearch<ValueType> memlessSearch(pomdp, targetStates, surelyNotAlmostSurelyReachTarget, smtSolverFactory);
            if (qualSettings.isWinningRegionSet()) {
                STORM_LOG_ERROR("Computing winning regions is not supported by the one-shot method.");
            } else {
                bool result = memlessSearch.analyzeForInitialStates(lookahead);
                if (result) {
                    STORM_PRINT_AND_LOG("From initial state, one can almost-surely reach the target.\n");
                } else {
                    STORM_PRINT_AND_LOG("From initial state, one may not almost-surely reach the target .\n");
                }
            }
        } else if (qualSettings.getMemlessSearchMethod() == "iterative") {
            storm::pomdp::MemlessSearchOptions options = fillMemlessSearchOptionsFromSettings();
            storm::pomdp::IterativePolicySearch<ValueType> search(pomdp, targetStates, surelyNotAlmostSurelyReachTarget, smtSolverFactory, options);
            if (qualSettings.isWinningRegionSet()) {
                search.computeWinningRegion(lookahead);
            } else {
                bool result = search.analyzeForInitialStates(lookahead);
                if (result) {
                    STORM_PRINT_AND_LOG("From initial state, one can almost-surely reach the target.");
                } else {
                    // TODO consider adding check for end components to improve this message.
                    STORM_PRINT_AND_LOG("From initial state, one may not almost-surely reach the target.");
                }
            }

            if (qualSettings.isPrintWinningRegionSet()) {
                search.getLastWinningRegion().print();
                std::cout << '\n';
            }
            if (qualSettings.isExportWinningRegionSet()) {
                std::size_t hash = pomdp.hash();
                search.getLastWinningRegion().storeToFile(qualSettings.exportWinningRegionPath(), "model hash: " + std::to_string(hash));
            }

            search.finalizeStatistics();
            if (pomdp.getInitialStates().getNumberOfSetBits() == 1) {
                uint64_t initialState = pomdp.getInitialStates().getNextSetIndex(0);
                uint64_t initialObservation = pomdp.getObservation(initialState);
                // TODO this is inefficient.
                uint64_t offset = 0;
                for (uint64_t state = 0; state < pomdp.getNumberOfStates(); ++state) {
                    if (state == initialState) {
                        break;
                    }
                    if (pomdp.getObservation(state) == initialObservation) {
                        ++offset;
                    }
                }

                if (search.getLastWinningRegion().isWinning(initialObservation, offset)) {
                    STORM_PRINT_AND_LOG("Initial state is safe!\n");
                } else {
                    STORM_PRINT_AND_LOG("Initial state may not be safe.\n");
                }
            } else {
                STORM_LOG_WARN("Output for multiple initial states is incomplete");
            }

            if (coreSettings.isShowStatisticsSet()) {
                STORM_PRINT_AND_LOG("#STATS Number of belief support states: " << search.getLastWinningRegion().beliefSupportStates() << '\n');
                if (qualSettings.computeExpensiveStats()) {
                    auto wbss = search.getLastWinningRegion().computeNrWinningBeliefs();
                    STORM_PRINT_AND_LOG("#STATS Number of winning belief support states: [" << wbss.first << "," << wbss.second << "]");
                }
                search.getStatistics().print();
            }

        } else {
            STORM_LOG_ERROR("This method is not implemented.");
        }
    }
    if (qualSettings.isComputeOnBeliefSupportSet()) {
        computedSomething = true;
        storm::pomdp::qualitative::JaniBeliefSupportMdpGenerator<ValueType> janicreator(pomdp);
        janicreator.generate(targetStates, surelyNotAlmostSurelyReachTarget);
        bool initialOnly = !qualSettings.isWinningRegionSet();
        storm::Environment env;
        STORM_LOG_WARN("Using a default environment (and therefore default settings) for the symbolic analysis.");
        janicreator.verifySymbolic(env, initialOnly);
        STORM_PRINT_AND_LOG("Initial state is safe: " << janicreator.isInitialWinning() << "\n");
    }
    STORM_LOG_THROW(computedSomething, storm::exceptions::InvalidSettingsException, "Nothing to be done, did you forget to set a method?");
}

template<typename ValueType, typename BeliefType = ValueType>
bool performAnalysis(std::shared_ptr<storm::models::sparse::Pomdp<ValueType>> const& pomdp, storm::pomdp::analysis::FormulaInformation const& formulaInfo,
                     storm::logic::Formula const& formula) {
    auto const& pomdpSettings = storm::settings::getModule<storm::settings::modules::POMDPSettings>();
    bool analysisPerformed = false;
    if (pomdpSettings.isBeliefExplorationSet()) {
        STORM_PRINT_AND_LOG("Exploring the belief MDP... \n");
        auto options = storm::pomdp::modelchecker::BeliefExplorationPomdpModelCheckerOptions<ValueType>(pomdpSettings.isBeliefExplorationDiscretizeSet(),
                                                                                                        pomdpSettings.isBeliefExplorationUnfoldSet());
        auto const& beliefExplorationSettings = storm::settings::getModule<storm::settings::modules::BeliefExplorationSettings>();
        beliefExplorationSettings.setValuesInOptionsStruct(options);
        storm::pomdp::modelchecker::BeliefExplorationPomdpModelChecker<storm::models::sparse::Pomdp<ValueType>, BeliefType> checker(pomdp, options);
        auto result = checker.check(formula);
        checker.printStatisticsToStream(std::cout);
        if (storm::utility::resources::isTerminate()) {
            STORM_PRINT_AND_LOG("\nResult till abort: ");
        } else {
            STORM_PRINT_AND_LOG("\nResult: ");
        }
        printResult(result.lowerBound, result.upperBound);
        STORM_PRINT_AND_LOG('\n');
        analysisPerformed = true;
    }
    if (pomdpSettings.isQualitativeAnalysisSet()) {
        performQualitativeAnalysis(pomdp, formulaInfo, formula);
        analysisPerformed = true;
    }
    if (pomdpSettings.isCheckFullyObservableSet()) {
        STORM_PRINT_AND_LOG("Analyzing the formula on the fully observable MDP ... ");
        auto resultPtr = storm::api::verifyWithSparseEngine<ValueType>(pomdp->template as<storm::models::sparse::Mdp<ValueType>>(),
                                                                       storm::api::createTask<ValueType>(formula.asSharedPointer(), true));
        if (resultPtr) {
            auto result = resultPtr->template asExplicitQuantitativeCheckResult<ValueType>();
            result.filter(storm::modelchecker::ExplicitQualitativeCheckResult<ValueType>(pomdp->getInitialStates()));
            if (storm::utility::resources::isTerminate()) {
                STORM_PRINT_AND_LOG("\nResult till abort: ");
            } else {
                STORM_PRINT_AND_LOG("\nResult: ");
            }
            printResult(result.getMin(), result.getMax());
            STORM_PRINT_AND_LOG('\n');
        } else {
            STORM_PRINT_AND_LOG("\nResult: Not available.\n");
        }
        analysisPerformed = true;
    }
    return analysisPerformed;
}

template<typename ValueType>
bool performTransformation(std::shared_ptr<storm::models::sparse::Pomdp<ValueType>>& pomdp, storm::logic::Formula const& formula) {
    auto const& pomdpSettings = storm::settings::getModule<storm::settings::modules::POMDPSettings>();
    auto const& ioSettings = storm::settings::getModule<storm::settings::modules::IOSettings>();
    auto const& transformSettings = storm::settings::getModule<storm::settings::modules::ToParametricSettings>();
    bool transformationPerformed = false;
    bool memoryUnfolded = false;
    if (pomdpSettings.getMemoryBound() > 1) {
        STORM_PRINT_AND_LOG("Computing the unfolding for memory bound " << pomdpSettings.getMemoryBound() << " and memory pattern '"
                                                                        << storm::storage::toString(pomdpSettings.getMemoryPattern()) << "' ...");
        storm::storage::PomdpMemory memory = storm::storage::PomdpMemoryBuilder().build(pomdpSettings.getMemoryPattern(), pomdpSettings.getMemoryBound());
        std::cout << memory.toString() << '\n';
        storm::transformer::PomdpMemoryUnfolder<ValueType> memoryUnfolder(*pomdp, memory);
        pomdp = memoryUnfolder.transform();
        STORM_PRINT_AND_LOG(" done.\n");
        pomdp->printModelInformationToStream(std::cout);
        transformationPerformed = true;
        memoryUnfolded = true;
    }

    // From now on the POMDP is considered memoryless

    if (transformSettings.isMecReductionSet()) {
        STORM_PRINT_AND_LOG("Eliminating mec choices ...");
        // Note: Elimination of mec choices only preserves memoryless schedulers.
        uint64_t oldChoiceCount = pomdp->getNumberOfChoices();
        storm::transformer::GlobalPomdpMecChoiceEliminator<ValueType> mecChoiceEliminator(*pomdp);
        pomdp = mecChoiceEliminator.transform(formula);
        STORM_PRINT_AND_LOG(" done.\n");
        STORM_PRINT_AND_LOG(oldChoiceCount - pomdp->getNumberOfChoices() << " choices eliminated through MEC choice elimination.\n");
        pomdp->printModelInformationToStream(std::cout);
        transformationPerformed = true;
    }

    if (transformSettings.isTransformBinarySet() || transformSettings.isTransformSimpleSet()) {
        if (transformSettings.isTransformSimpleSet()) {
            STORM_PRINT_AND_LOG("Transforming the POMDP to a simple POMDP.");
            pomdp = storm::transformer::BinaryPomdpTransformer<ValueType>().transform(*pomdp, true).transformedPomdp;
        } else {
            STORM_PRINT_AND_LOG("Transforming the POMDP to a binary POMDP.");
            pomdp = storm::transformer::BinaryPomdpTransformer<ValueType>().transform(*pomdp, false).transformedPomdp;
        }
        pomdp->printModelInformationToStream(std::cout);
        STORM_PRINT_AND_LOG(" done.\n");
        transformationPerformed = true;
    }

    if (pomdpSettings.isExportToParametricSet()) {
        STORM_PRINT_AND_LOG("Transforming memoryless POMDP to pMC...");
        storm::transformer::ApplyFiniteSchedulerToPomdp<ValueType> toPMCTransformer(*pomdp);
        std::string transformMode = transformSettings.getFscApplicationTypeString();
        auto pmc = toPMCTransformer.transform(storm::transformer::parsePomdpFscApplicationMode(transformMode));
        STORM_PRINT_AND_LOG(" done.\n");
        if (transformSettings.allowPostSimplifications()) {
            STORM_PRINT_AND_LOG("Simplifying pMC...");
            pmc = storm::api::performBisimulationMinimization<storm::RationalFunction>(pmc->template as<storm::models::sparse::Dtmc<storm::RationalFunction>>(),
                                                                                       {formula.asSharedPointer()}, storm::storage::BisimulationType::Strong)
                      ->template as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
            STORM_PRINT_AND_LOG(" done.\n");
            pmc->printModelInformationToStream(std::cout);
        }
        STORM_PRINT_AND_LOG("Exporting pMC...");
        storm::analysis::ConstraintCollector<storm::RationalFunction> constraints(*pmc);
        auto const& parameterSet = constraints.getVariables();
        std::vector<storm::RationalFunctionVariable> parameters(parameterSet.begin(), parameterSet.end());
        std::vector<std::string> parameterNames;
        for (auto const& parameter : parameters) {
            parameterNames.push_back(parameter.name());
        }
        storm::api::exportSparseModelAsDrn(pmc, pomdpSettings.getExportToParametricFilename(), parameterNames,
                                           !ioSettings.isExplicitExportPlaceholdersDisabled());
        STORM_PRINT_AND_LOG(" done.\n");
        transformationPerformed = true;
    }
    if (transformationPerformed && !memoryUnfolded) {
        STORM_PRINT_AND_LOG("Implicitly assumed restriction to memoryless schedulers for at least one transformation.\n");
    }
    return transformationPerformed;
}

template<typename ValueType>
void processPomdp(std::shared_ptr<storm::models::sparse::Pomdp<ValueType>>& pomdp) {
    auto const& pomdpSettings = storm::settings::getModule<storm::settings::modules::POMDPSettings>();

    if (!pomdpSettings.isNoCanonicSet()) {
        storm::transformer::MakePOMDPCanonic<ValueType> makeCanonic(*pomdp);
        pomdp = makeCanonic.transform();
    }

    if (pomdpSettings.isAnalyzeUniqueObservationsSet()) {
        STORM_PRINT_AND_LOG("Analyzing states with unique observation ...\n");
        storm::analysis::UniqueObservationStates<ValueType> uniqueAnalysis(*pomdp);
        std::cout << uniqueAnalysis.analyse() << '\n';
    }
}

template<typename ValueType>
void processFormula(std::shared_ptr<storm::models::sparse::Pomdp<ValueType>>&& pomdp, std::shared_ptr<storm::logic::Formula const> const& formula) {
    auto formulaInfo = storm::pomdp::analysis::getFormulaInformation(*pomdp, *formula);
    STORM_LOG_THROW(!formulaInfo.isUnsupported(), storm::exceptions::InvalidPropertyException,
                    "The formula '" << *formula << "' is not supported by storm-pomdp.");

    storm::utility::Stopwatch sw(true);
    // Note that formulaInfo contains state-based information which potentially needs to be updated during preprocessing
    if (performPreprocessing(pomdp, formulaInfo, *formula)) {
        sw.stop();
        STORM_PRINT_AND_LOG("Time for graph-based POMDP (pre-)processing: " << sw << ".\n");
        pomdp->printModelInformationToStream(std::cout);
    }

    sw.restart();
    if (performTransformation(pomdp, *formula)) {
        sw.stop();
        STORM_PRINT_AND_LOG("Time for POMDP transformation(s): " << sw << ".\n");
    }

    sw.restart();
    if (performAnalysis(pomdp, formulaInfo, *formula)) {
        sw.stop();
        STORM_PRINT_AND_LOG("Time for POMDP analysis: " << sw << ".\n");
    }
}

template<typename ValueType>
void processPomdpFormula(std::shared_ptr<storm::models::sparse::Pomdp<ValueType>>&& pomdp, std::shared_ptr<storm::logic::Formula const> const& formula) {
    STORM_LOG_ASSERT(pomdp, "No POMDP given or input POMDP is of unexpected type.");
    processPomdp(pomdp);

    if (formula) {
        processFormula(std::move(pomdp), formula);
    } else {
        STORM_LOG_WARN("Nothing to be done. Did you forget to specify a formula?");
    }
}

void processOptions() {
    auto symbolicInput = storm::cli::parseSymbolicInput();
    storm::cli::ModelProcessingInformation mpi;
    std::tie(symbolicInput, mpi) = storm::cli::preprocessSymbolicInput(symbolicInput);
    STORM_LOG_THROW(mpi.buildValueType == storm::cli::ModelProcessingInformation::ValueType::FinitePrecision ||
                        mpi.buildValueType == storm::cli::ModelProcessingInformation::ValueType::Exact,
                    storm::exceptions::UnexpectedException, "Unexpected ValueType for model building.");

    auto model = storm::cli::buildPreprocessExportModel(symbolicInput, mpi);
    if (!model) {
        STORM_PRINT_AND_LOG("No input model given.\n");
        return;
    }
    STORM_LOG_THROW(model->getType() == storm::models::ModelType::Pomdp && model->isSparseModel(), storm::exceptions::WrongFormatException,
                    "Expected a POMDP in sparse representation.");

    std::shared_ptr<storm::logic::Formula const> formula;
    if (!symbolicInput.properties.empty()) {
        formula = symbolicInput.properties.front().getRawFormula();
        STORM_PRINT_AND_LOG("Analyzing property '" << *formula << "'\n");
        STORM_LOG_WARN_COND(symbolicInput.properties.size() == 1,
                            "There is currently no support for multiple properties. All other properties will be ignored.");
    }

    if (model->isExact()) {
        processPomdpFormula(model->template as<storm::models::sparse::Pomdp<storm::RationalNumber>>(), formula);
    } else {
        processPomdpFormula(model->template as<storm::models::sparse::Pomdp<double>>(), formula);
    }
}

}  // namespace cli
}  // namespace pomdp
}  // namespace storm

/*!
 * Entry point for the pomdp backend.
 *
 * @param argc The argc argument of main().
 * @param argv The argv argument of main().
 * @return Return code, 0 if successfull, not 0 otherwise.
 */
int main(const int argc, const char** argv) {
    try {
        return storm::cli::process("Storm-POMDP", "storm-pomdp", storm::settings::initializePomdpSettings, storm::pomdp::cli::processOptions, argc, argv);
    } catch (storm::exceptions::BaseException const& exception) {
        STORM_LOG_ERROR("An exception caused Storm-pomdp to terminate. The message of the exception is: " << exception.what());
        return 1;
    } catch (std::exception const& exception) {
        STORM_LOG_ERROR("An unexpected exception occurred and caused Storm-pomdp to terminate. The message of this exception is: " << exception.what());
        return 2;
    }
}
