#include "storm-counterexamples/api/counterexamples.h"

#include "storm/environment/Environment.h"
#include "storm/utility/shortestPaths.h"

namespace storm {
namespace api {

std::shared_ptr<storm::counterexamples::Counterexample> computeHighLevelCounterexampleMilp(storm::storage::SymbolicModelDescription const& symbolicModel,
                                                                                           std::shared_ptr<storm::models::sparse::Mdp<double>> mdp,
                                                                                           std::shared_ptr<storm::logic::Formula const> const& formula) {
    Environment env;
    return storm::counterexamples::MILPMinimalLabelSetGenerator<double>::computeCounterexample(env, symbolicModel, *mdp, formula);
}

std::shared_ptr<storm::counterexamples::Counterexample> computeHighLevelCounterexampleMaxSmt(storm::storage::SymbolicModelDescription const& symbolicModel,
                                                                                             std::shared_ptr<storm::models::sparse::Model<double>> model,
                                                                                             std::shared_ptr<storm::logic::Formula const> const& formula) {
    Environment env;
    return storm::counterexamples::SMTMinimalLabelSetGenerator<double>::computeCounterexample(env, symbolicModel, *model, formula);
}

std::shared_ptr<storm::counterexamples::Counterexample> computeKShortestPathCounterexample(std::shared_ptr<storm::models::sparse::Model<double>> model,
                                                                                           std::shared_ptr<storm::logic::Formula const> const& formula,
                                                                                           size_t maxK) {
    // Only accept formulas of the form "P </<= x [F target]
    STORM_LOG_THROW(formula->isProbabilityOperatorFormula(), storm::exceptions::InvalidPropertyException,
                    "Counterexample generation does not support this kind of formula. Expecting a probability operator as the outermost formula element.");
    storm::logic::ProbabilityOperatorFormula const& probabilityOperator = formula->asProbabilityOperatorFormula();
    STORM_LOG_THROW(probabilityOperator.hasBound(), storm::exceptions::InvalidPropertyException, "Counterexample generation only supports bounded formulas.");
    STORM_LOG_THROW(!storm::logic::isLowerBound(probabilityOperator.getComparisonType()), storm::exceptions::InvalidPropertyException,
                    "Counterexample generation only supports upper bounds.");
    double threshold = probabilityOperator.getThresholdAs<double>();
    storm::logic::Formula const& subformula = formula->asOperatorFormula().getSubformula();
    STORM_LOG_THROW(subformula.isEventuallyFormula(), storm::exceptions::InvalidPropertyException,
                    "Path formula is required to be of the form 'F psi' for counterexample generation.");
    bool strictBound = (probabilityOperator.getComparisonType() == storm::logic::ComparisonType::Less);
    STORM_LOG_THROW(model->getInitialStates().getNumberOfSetBits() == 1, storm::exceptions::NotSupportedException,
                    "K-shortest paths is only supported for models with a unique initial state.");
    size_t initialState = *(model->getInitialStates().begin());

    // Perform model checking to get target states
    Environment env;
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Model<double>> modelchecker(*model);

    storm::logic::EventuallyFormula const& eventuallyFormula = subformula.asEventuallyFormula();
    std::unique_ptr<storm::modelchecker::CheckResult> subResult = modelchecker.check(env, eventuallyFormula.getSubformula());
    storm::modelchecker::ExplicitQualitativeCheckResult<double> const& subQualitativeResult = subResult->template asExplicitQualitativeCheckResult<double>();

    // Check if counterexample is even possible
    storm::storage::BitVector phiStates(model->getNumberOfStates(), true);
    auto results = storm::modelchecker::helper::SparseDtmcPrctlHelper<double>::computeUntilProbabilities(
        env, false, model->getTransitionMatrix(), model->getBackwardTransitions(), phiStates, subQualitativeResult.getTruthValuesVector(), true);
    double reachProb = results.at(initialState);
    STORM_LOG_THROW((reachProb > threshold) || (strictBound && reachProb >= threshold), storm::exceptions::InvalidArgumentException,
                    "Given probability threshold " << threshold << " cannot be " << (strictBound ? "achieved" : "exceeded")
                                                   << " in model with maximal reachability probability of " << reachProb << ".");

    auto generator = storm::utility::ksp::ShortestPathsGenerator<double>(*model, subQualitativeResult.getTruthValuesVector());
    storm::counterexamples::PathCounterexample<double> cex(model);
    double probability = 0;
    bool thresholdExceeded = false;
    for (size_t k = 1; k <= maxK; ++k) {
        cex.addPath(generator.getPathAsList(k), k);
        probability += generator.getDistance(k);
        // Check if accumulated probability mass is already enough
        if ((probability > threshold) || (strictBound && probability >= threshold)) {
            thresholdExceeded = true;
            break;
        }
    }
    STORM_LOG_WARN_COND(thresholdExceeded, "Aborted computation because maximal number of paths was reached. Probability threshold is not yet exceeded.");

    return std::make_shared<storm::counterexamples::PathCounterexample<double>>(cex);
}

}  // namespace api
}  // namespace storm
