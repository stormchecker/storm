#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "storm-permissive/analysis/PermissiveSchedulerComputation.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/StateActionPair.h"
#include "storm/storage/StateActionTargetTuple.h"
#include "storm/utility/macros.h"

namespace storm {
namespace ps {

template<typename RM>
class SmtPermissiveSchedulerComputation : public PermissiveSchedulerComputation<RM> {
   private:
    bool mPerformedSmtLoop = false;
    bool mFoundSolution = false;
    storm::solver::SmtSolver& solver;
    storm::expressions::ExpressionManager& manager;
    std::unordered_map<storm::storage::StateActionPair, storm::expressions::Variable> multistrategyVariables;
    std::unordered_map<storm::expressions::Variable, bool> multistrategyVariablesToTakenMap;
    std::unordered_map<uint_fast64_t, storm::expressions::Variable> mProbVariables;
    std::unordered_map<uint_fast64_t, storm::expressions::Variable> mAlphaVariables;
    std::unordered_map<storm::storage::StateActionTarget, storm::expressions::Variable> mBetaVariables;
    std::unordered_map<uint_fast64_t, storm::expressions::Variable> mGammaVariables;

   public:
    SmtPermissiveSchedulerComputation(storm::solver::SmtSolver& smtSolver, storm::models::sparse::Mdp<double, RM> const& mdp,
                                      storm::storage::BitVector const& goalstates, storm::storage::BitVector const& sinkstates)
        : PermissiveSchedulerComputation<RM>(mdp, goalstates, sinkstates),
          mPerformedSmtLoop(false),
          mFoundSolution(false),
          solver(smtSolver),
          manager(solver.getManager()) {}

    void calculatePermissiveScheduler(bool lowerBound, double boundary) override {
        performSmtLoop(lowerBound, boundary, this->mPenalties);
        mPerformedSmtLoop = true;
    }

    bool foundSolution() const override {
        STORM_LOG_ASSERT(mPerformedSmtLoop, "SMT loop not performed.");
        return mFoundSolution;
    }

    SubMDPPermissiveScheduler<RM> getScheduler() const override {
        STORM_LOG_ASSERT(foundSolution(), "Solution not found.");
        SubMDPPermissiveScheduler<RM> result(this->mdp, true);
        for (auto const& entry : multistrategyVariables) {
            if (!multistrategyVariablesToTakenMap.at(entry.second)) {
                result.disable(this->mdp.getChoiceIndex(entry.first));
            }
        }
        return result;
    }

   private:
    /**
     *  Create variables
     */
    void createVariables(storm::storage::BitVector const& relevantStates) {
        storm::expressions::Variable var;
        for (uint_fast64_t s : relevantStates) {
            // Create x_s variables
            var = manager.declareRationalVariable("x_" + std::to_string(s));
            solver.add(var >= manager.rational(0));
            solver.add(var <= manager.rational(1));
            mProbVariables[s] = var;
            // Create alpha_s variables
            var = manager.declareBooleanVariable("alp_" + std::to_string(s));
            mAlphaVariables[s] = var;
            // Create gamma_s variables
            var = manager.declareRationalVariable("gam_" + std::to_string(s));
            solver.add(var >= manager.rational(0));
            solver.add(var <= manager.rational(1));
            mGammaVariables[s] = var;
            for (uint_fast64_t a = 0; a < this->mdp.getNumberOfChoices(s); ++a) {
                auto stateAndAction = storage::StateActionPair(s, a);

                // Create y_(s,a) variables
                var = manager.declareBooleanVariable("y_" + std::to_string(s) + "_" + std::to_string(a));
                multistrategyVariables[stateAndAction] = var;
                multistrategyVariablesToTakenMap[var] = false;

                // Create beta_(s,a,t) variables
                // Iterate over successors of s via a.
                for (auto const& entry : this->mdp.getTransitionMatrix().getRow(this->mdp.getNondeterministicChoiceIndices()[s] + a)) {
                    if (entry.getValue() != 0) {
                        storage::StateActionTarget sat = {s, a, entry.getColumn()};
                        var = manager.declareBooleanVariable("beta_" + to_string(sat));
                        mBetaVariables[sat] = var;
                    }
                }
            }
        }
    }

    /**
     * Create constraints
     */
    void createConstraints(bool lowerBound, double boundary, storm::storage::BitVector const& relevantStates) {
        // (4) and (7) are omitted on purpose (-- we currenty do not support controllability of actions -- )

        // (1)
        STORM_LOG_ASSERT(this->mdp.getInitialStates().getNumberOfSetBits() == 1, "No unique initial state.");
        uint_fast64_t initialStateIndex = this->mdp.getInitialStates().getNextSetIndex(0);
        STORM_LOG_ASSERT(relevantStates[initialStateIndex], "Initial state not relevant.");
        if (lowerBound) {
            solver.add(mProbVariables[initialStateIndex] >= manager.rational(boundary));
        } else {
            solver.add(mProbVariables[initialStateIndex] <= manager.rational(boundary));
        }
        for (uint_fast64_t s : relevantStates) {
            std::string stateString = std::to_string(s);
            std::vector<storm::expressions::Expression> expressions;
            // (2)
            for (uint_fast64_t a = 0; a < this->mdp.getNumberOfChoices(s); ++a) {
                expressions.push_back(multistrategyVariables[storage::StateActionPair(s, a)]);
            }
            solver.add(storm::expressions::disjunction(expressions));
            expressions.clear();

            // (5) These constraints are only necessary for lower-bounded properties.
            if (lowerBound) {
                //                        TODO
                //                        solver.addConstraint("c5-" + std::to_string(s), mProbVariables[s] <= mAlphaVariables[s]);
            }

            // (3) For the relevant states.
            for (uint_fast64_t a = 0; a < this->mdp.getNumberOfChoices(s); ++a) {
                std::string sastring(stateString + "_" + std::to_string(a));

                for (auto const& entry : this->mdp.getTransitionMatrix().getRow(this->mdp.getNondeterministicChoiceIndices()[s] + a)) {
                    if (entry.getValue() != 0 && relevantStates.get(entry.getColumn())) {
                        expressions.push_back(manager.rational(entry.getValue()) * mProbVariables[entry.getColumn()]);
                    } else if (entry.getValue() != 0 && this->mGoals.get(entry.getColumn())) {
                        expressions.push_back(manager.rational(entry.getValue()));
                    }
                }
                if (lowerBound) {
                    solver.add(storm::expressions::implies(multistrategyVariables[storage::StateActionPair(s, a)],
                                                           mProbVariables[s] <= storm::expressions::sum(expressions)));
                } else {
                    solver.add(storm::expressions::implies(multistrategyVariables[storage::StateActionPair(s, a)],
                                                           mProbVariables[s] >= storm::expressions::sum(expressions)));
                }
                expressions.clear();
            }

            // (6) and (8) are only necessary for lower-bounded properties.
            if (lowerBound) {
                //                        TODO
                //                        for(uint_fast64_t a = 0; a < this->mdp.getNumberOfChoices(s); ++a) {
                //                            // (6)
                //                            std::string sastring(stateString + "_" + std::to_string(a));
                //                            expr = solver.getConstant(0.0);
                //                            for(auto const& entry : this->mdp.getTransitionMatrix().getRow(this->mdp.getNondeterministicChoiceIndices()[s]+a))
                //                            {
                //                                if(entry.getValue() != 0) {
                //                                    storage::StateActionTarget sat = {s,a,entry.getColumn()};
                //                                    expr = expr + mBetaVariables[sat];
                //                                }
                //                            }
                //                            solver.addConstraint("c6-" + sastring, multistrategyVariables[storage::StateActionPair(s,a)] ==
                //                            (solver.getConstant(1) - mAlphaVariables[s]) + expr);
                //
                //                            for(auto const& entry : this->mdp.getTransitionMatrix().getRow(this->mdp.getNondeterministicChoiceIndices()[s]+a))
                //                            {
                //                                if(entry.getValue() != 0) {
                //                                    storage::StateActionTarget sat = {s,a,entry.getColumn()};
                //                                    std::string satstring = to_string(sat);
                //                                    // (8)
                //                                    if(relevantStates[entry.getColumn()]) {
                //                                        STORM_LOG_ASSERT(mGammaVariables.count(entry.getColumn()) > 0, "Entry not found.");
                //                                        STORM_LOG_ASSERT(mGammaVariables.count(s) > 0, "Entry not found.");
                //                                        STORM_LOG_ASSERT(mBetaVariables.count(sat) > 0, "Entry not found.");
                //                                        solver.addConstraint("c8-" + satstring, mGammaVariables[entry.getColumn()] < mGammaVariables[s] +
                //                                        (solver.getConstant(1) - mBetaVariables[sat]) + mProbVariables[s]); // With rewards, we have to change
                //                                        this.
                //                                    }
                //                                }
                //                            }
                //                        }
            }
        }
    }

    /**
     *
     */
    void performSmtLoop(bool lowerBound, double boundary, PermissiveSchedulerPenalties const& penalties) {
        storm::storage::BitVector irrelevant = this->mGoals | this->mSinks;
        storm::storage::BitVector relevantStates = ~irrelevant;
        createVariables(relevantStates);
        createConstraints(lowerBound, boundary, relevantStates);

        // Find the initial solution (if possible).
        storm::solver::SmtSolver::CheckResult result = solver.check();

        if (result == storm::solver::SmtSolver::CheckResult::Sat) {
            // Extract the solution from the multi-strategy variables and track all state-action pairs that were
            // not taken. Also, we assert all decided choices, so they are not altered anymore.
            std::shared_ptr<storm::solver::SmtSolver::ModelReference> model = solver.getModel();

            std::vector<storage::StateActionPair> availableStateActionPairs;
            for (uint_fast64_t s : relevantStates) {
                for (uint_fast64_t a = 0; a < this->mdp.getNumberOfChoices(s); ++a) {
                    auto stateAndAction = storage::StateActionPair(s, a);

                    auto multistrategyVariable = multistrategyVariables.at(stateAndAction);
                    if (model->getBooleanValue(multistrategyVariable)) {
                        multistrategyVariablesToTakenMap[multistrategyVariable] = true;
                        solver.add(multistrategyVariable);
                    } else {
                        availableStateActionPairs.push_back(stateAndAction);
                    }
                }
            }

            // Now we sort the available state-action pairs in decending penalty order, so we can try taking more
            // and more actions from the back (and discard them if not).
            std::sort(availableStateActionPairs.begin(), availableStateActionPairs.end(),
                      [&penalties](storage::StateActionPair const& first, storage::StateActionPair const& second) {
                          return penalties.get(first) < penalties.get(second);
                      });

            do {
                auto multistrategyVariable = multistrategyVariables.at(availableStateActionPairs.back());

                result = solver.checkWithAssumptions({multistrategyVariable});

                if (result == storm::solver::SmtSolver::CheckResult::Sat) {
                    model = solver.getModel();
                    if (model->getBooleanValue(multistrategyVariable)) {
                        solver.add(multistrategyVariable);
                        multistrategyVariablesToTakenMap[multistrategyVariable] = true;
                    }
                }
                availableStateActionPairs.pop_back();

            } while (!availableStateActionPairs.empty());

            mFoundSolution = true;
        } else {
            mFoundSolution = false;
        }
    }
};

}  // namespace ps
}  // namespace storm
