#include "storm/modelchecker/multiobjective/MultiObjectiveModelChecking.h"

#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/exceptions/IllegalFunctionCallException.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/InvalidEnvironmentException.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/modelchecker/multiobjective/MultiObjectivePostprocessing.h"
#include "storm/modelchecker/multiobjective/constraintbased/SparseCbAchievabilityQuery.h"
#include "storm/modelchecker/multiobjective/deterministicScheds/DeterministicSchedsAchievabilityChecker.h"
#include "storm/modelchecker/multiobjective/deterministicScheds/DeterministicSchedsParetoExplorer.h"
#include "storm/modelchecker/multiobjective/pcaa/SparsePcaaQuery.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectivePreprocessor.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace multiobjective {

template<typename SparseModelType>
std::unique_ptr<CheckResult> performMultiObjectiveModelChecking(Environment const& env, SparseModelType const& model,
                                                                storm::logic::MultiObjectiveFormula const& formula, bool produceScheduler) {
    storm::utility::Stopwatch swTotal(true);
    storm::utility::Stopwatch swPreprocessing(true);
    STORM_LOG_THROW(model.getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::NotSupportedException,
                    "Multi-objective Model checking on model with multiple initial states is not supported.");
    STORM_LOG_THROW(formula.isTradeoff(), storm::exceptions::IllegalFunctionCallException,
                    "Invoked multi-objective model checking with a non-tradeoff formula " << formula << ".");

    // If we consider an MA, ensure that it is closed
    if (model.isOfType(storm::models::ModelType::MarkovAutomaton)) {
        STORM_LOG_THROW(dynamic_cast<storm::models::sparse::MarkovAutomaton<typename SparseModelType::ValueType> const*>(&model)->isClosed(),
                        storm::exceptions::InvalidArgumentException, "Unable to check multi-objective formula on non-closed Markov automaton.");
    }

    // Preprocess the model
    auto preprocessorResult = preprocessing::SparseMultiObjectivePreprocessor<SparseModelType>::preprocess(env, model, formula, produceScheduler);
    swPreprocessing.stop();
    STORM_LOG_INFO("Preprocessing done in " << swPreprocessing << " seconds.\n"
                                            << " Result: " << preprocessorResult << '\n');

    // Invoke the analysis
    storm::utility::Stopwatch swAnalysis(true);
    std::unique_ptr<CheckResult> result;
    MultiObjectiveMethod method = env.modelchecker().multi().getMethod();
    switch (method) {
        case MultiObjectiveMethod::Pcaa: {
            if (env.modelchecker().multi().isSchedulerRestrictionSet()) {
                STORM_LOG_THROW(!produceScheduler, storm::exceptions::NotImplementedException,
                                "Scheduler computation is not implement for queries with restricted scheduler classes.");
                if (preprocessorResult.queryType == preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>::QueryType::Achievability ||
                    preprocessorResult.queryType == preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>::QueryType::Quantitative) {
                    auto achChecker = DeterministicSchedsAchievabilityChecker<SparseModelType, storm::RationalNumber>(preprocessorResult);
                    result = achChecker.check(env);
                } else {
                    STORM_LOG_THROW(preprocessorResult.queryType == preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>::QueryType::Pareto,
                                    storm::exceptions::NotImplementedException, "The query type is not implemented with scheduler restrictions.");
                    auto explorer = DeterministicSchedsParetoExplorer<SparseModelType, storm::RationalNumber>(preprocessorResult);
                    result = explorer.check(env);
                    if (env.modelchecker().multi().isExportPlotSet()) {
                        explorer.exportPlotOfCurrentApproximation(env);
                    }
                }
            } else {
                SparsePcaaQuery<SparseModelType, storm::RationalNumber> query(preprocessorResult);
                // Adapt environment for the query
                auto subEnv = env;
                if (storm::NumberTraits<typename SparseModelType::ValueType>::IsExact) {
                    subEnv.solver().setForceExact(true);
                }
                // Solve the query
                result = query.check(subEnv, produceScheduler);
                if (produceScheduler) {
                    STORM_LOG_THROW(result->isExplicitParetoCurveCheckResult(), storm::exceptions::UnexpectedException,
                                    "Scheduler computation is not implement for the produced result type.");
                    auto& paretoRes = result->template asExplicitParetoCurveCheckResult<typename SparseModelType::ValueType>();
                    STORM_LOG_ASSERT(paretoRes.hasScheduler(), "Scheduler requested but none was produced.");
                    if (preprocessorResult.memoryIncorporationReverseData) {
                        // we have information to post-process schedulers
                        transformObjectiveSchedulersToOriginal(preprocessorResult.memoryIncorporationReverseData.value(), paretoRes.getSchedulers());
                    }
                }
            }
            break;
        }
        case MultiObjectiveMethod::ConstraintBased: {
            STORM_LOG_THROW(!env.modelchecker().multi().isSchedulerRestrictionSet(), storm::exceptions::InvalidEnvironmentException,
                            "The selected multi-objective model checking method does not support scheduler restrictions.");
            STORM_LOG_THROW(!produceScheduler, storm::exceptions::NotImplementedException,
                            "Scheduler computation is not implement for constraint-based multi objective solving.");
            std::unique_ptr<SparseCbQuery<SparseModelType>> query;
            switch (preprocessorResult.queryType) {
                case preprocessing::SparseMultiObjectivePreprocessorResult<SparseModelType>::QueryType::Achievability:
                    query = std::unique_ptr<SparseCbQuery<SparseModelType>>(new SparseCbAchievabilityQuery<SparseModelType>(preprocessorResult));
                    break;
                default:
                    STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                                    "The multi-objective query type is not supported for the selected solution method '" << toString(method) << "'.");
                    break;
            }

            result = query->check(env);

            if (env.modelchecker().multi().isExportPlotSet()) {
                STORM_LOG_ERROR("Can not export plot for the constrained based solver.");
            }
            break;
        }
        default:
            STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                            "The multi-objective solution method '" << toString(method) << "' is not supported.");
    }
    swAnalysis.stop();

    swTotal.stop();
    if (storm::settings::getModule<storm::settings::modules::CoreSettings>().isShowStatisticsSet()) {
        STORM_PRINT_AND_LOG("Solving multi-objective query took " << swTotal << " seconds (consisting of " << swPreprocessing
                                                                  << " seconds for preprocessing and " << swAnalysis
                                                                  << " seconds for analyzing the preprocessed model).\n");
    }

    return result;
}

template std::unique_ptr<CheckResult> performMultiObjectiveModelChecking<storm::models::sparse::Mdp<double>>(Environment const& env,
                                                                                                             storm::models::sparse::Mdp<double> const& model,
                                                                                                             storm::logic::MultiObjectiveFormula const& formula,
                                                                                                             bool produceScheduler);
template std::unique_ptr<CheckResult> performMultiObjectiveModelChecking<storm::models::sparse::MarkovAutomaton<double>>(
    Environment const& env, storm::models::sparse::MarkovAutomaton<double> const& model, storm::logic::MultiObjectiveFormula const& formula,
    bool produceScheduler);
template std::unique_ptr<CheckResult> performMultiObjectiveModelChecking<storm::models::sparse::Mdp<storm::RationalNumber>>(
    Environment const& env, storm::models::sparse::Mdp<storm::RationalNumber> const& model, storm::logic::MultiObjectiveFormula const& formula,
    bool produceScheduler);
template std::unique_ptr<CheckResult> performMultiObjectiveModelChecking<storm::models::sparse::MarkovAutomaton<storm::RationalNumber>>(
    Environment const& env, storm::models::sparse::MarkovAutomaton<storm::RationalNumber> const& model, storm::logic::MultiObjectiveFormula const& formula,
    bool produceScheduler);

}  // namespace multiobjective
}  // namespace modelchecker
}  // namespace storm
