
#include "storm-permissive/analysis/PermissiveSchedulers.h"

#include "storm-permissive/analysis/MILPPermissiveSchedulers.h"
#include "storm-permissive/analysis/SmtBasedPermissiveSchedulers.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/NotImplementedException.h"
#include "storm/modelchecker/propositional/SparsePropositionalModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/graph.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"

namespace storm {
namespace ps {

template<typename RM>
boost::optional<SubMDPPermissiveScheduler<RM>> computePermissiveSchedulerViaMILP(storm::Environment const& env,
                                                                                 storm::models::sparse::Mdp<double, RM> const& mdp,
                                                                                 storm::logic::ProbabilityOperatorFormula const& safeProp) {
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Mdp<double, RM>> propMC(mdp);
    STORM_LOG_ASSERT(safeProp.getSubformula().isEventuallyFormula(), "No eventually formula.");
    auto backwardTransitions = mdp.getBackwardTransitions();
    storm::storage::BitVector goalstates = propMC.check(safeProp.getSubformula().asEventuallyFormula().getSubformula())
                                               ->template asExplicitQualitativeCheckResult<double>()
                                               .getTruthValuesVector();
    goalstates = storm::utility::graph::performProb1A(mdp, backwardTransitions, storm::storage::BitVector(goalstates.size(), true), goalstates);
    storm::storage::BitVector sinkstates =
        storm::utility::graph::performProb0A(backwardTransitions, storm::storage::BitVector(goalstates.size(), true), goalstates);

    auto solver = storm::utility::solver::getLpSolver<double>(env, "Gurobi", storm::solver::LpSolverTypeSelection::Gurobi);
    MilpPermissiveSchedulerComputation<storm::models::sparse::StandardRewardModel<double>> comp(*solver, mdp, goalstates, sinkstates);
    STORM_LOG_THROW(!storm::logic::isStrict(safeProp.getComparisonType()), storm::exceptions::NotImplementedException, "Strict bounds are not supported");
    comp.calculatePermissiveScheduler(storm::logic::isLowerBound(safeProp.getComparisonType()), safeProp.getThresholdAs<double>());
    // comp.dumpLpToFile("milpdump.lp");
    std::cout << "Found Solution: " << (comp.foundSolution() ? "yes" : "no") << '\n';
    if (comp.foundSolution()) {
        return boost::optional<SubMDPPermissiveScheduler<RM>>(comp.getScheduler());
    } else {
        return boost::optional<SubMDPPermissiveScheduler<RM>>();
    }
}

template<typename RM>
boost::optional<SubMDPPermissiveScheduler<RM>> computePermissiveSchedulerViaSMT(storm::models::sparse::Mdp<double, RM> const& mdp,
                                                                                storm::logic::ProbabilityOperatorFormula const& safeProp) {
    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Mdp<double, RM>> propMC(mdp);
    STORM_LOG_ASSERT(safeProp.getSubformula().isEventuallyFormula(), "No eventually formula.");
    auto backwardTransitions = mdp.getBackwardTransitions();
    storm::storage::BitVector goalstates = propMC.check(safeProp.getSubformula().asEventuallyFormula().getSubformula())
                                               ->template asExplicitQualitativeCheckResult<double>()
                                               .getTruthValuesVector();
    goalstates = storm::utility::graph::performProb1A(mdp, backwardTransitions, storm::storage::BitVector(goalstates.size(), true), goalstates);
    storm::storage::BitVector sinkstates =
        storm::utility::graph::performProb0A(backwardTransitions, storm::storage::BitVector(goalstates.size(), true), goalstates);

    std::shared_ptr<storm::expressions::ExpressionManager> expressionManager = std::make_shared<storm::expressions::ExpressionManager>();
    auto solver = storm::utility::solver::getSmtSolver(*expressionManager);
    SmtPermissiveSchedulerComputation<storm::models::sparse::StandardRewardModel<double>> comp(*solver, mdp, goalstates, sinkstates);
    STORM_LOG_THROW(!storm::logic::isStrict(safeProp.getComparisonType()), storm::exceptions::NotImplementedException, "Strict bounds are not supported");
    comp.calculatePermissiveScheduler(storm::logic::isLowerBound(safeProp.getComparisonType()), safeProp.getThresholdAs<double>());
    if (comp.foundSolution()) {
        return boost::optional<SubMDPPermissiveScheduler<RM>>(comp.getScheduler());
    } else {
        return boost::optional<SubMDPPermissiveScheduler<RM>>();
    }
    return boost::none;
}

template boost::optional<SubMDPPermissiveScheduler<>> computePermissiveSchedulerViaMILP(storm::Environment const& env,
                                                                                        storm::models::sparse::Mdp<double> const& mdp,
                                                                                        storm::logic::ProbabilityOperatorFormula const& safeProp);
template boost::optional<SubMDPPermissiveScheduler<>> computePermissiveSchedulerViaSMT(storm::models::sparse::Mdp<double> const& mdp,
                                                                                       storm::logic::ProbabilityOperatorFormula const& safeProp);

}  // namespace ps
}  // namespace storm
