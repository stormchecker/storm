#pragma once

#include <fstream>
#include <memory>
#include <unordered_map>

#include "storm-permissive/analysis/PermissiveSchedulerComputation.h"
#include "storm-permissive/analysis/PermissiveSchedulers.h"
#include "storm/io/file.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/solver/LpSolver.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/StateActionPair.h"
#include "storm/storage/StateActionTargetTuple.h"
#include "storm/storage/expressions/Variable.h"

namespace storm {
namespace ps {

template<typename RM>
class MilpPermissiveSchedulerComputation : public PermissiveSchedulerComputation<RM> {
   private:
    bool mCalledOptimizer = false;
    storm::solver::LpSolver<double>& solver;
    std::unordered_map<storm::storage::StateActionPair, storm::expressions::Variable> multistrategyVariables;
    std::unordered_map<uint_fast64_t, storm::expressions::Variable> mProbVariables;
    std::unordered_map<uint_fast64_t, storm::expressions::Variable> mAlphaVariables;
    std::unordered_map<storm::storage::StateActionTarget, storm::expressions::Variable> mBetaVariables;
    std::unordered_map<uint_fast64_t, storm::expressions::Variable> mGammaVariables;

   public:
    MilpPermissiveSchedulerComputation(storm::solver::LpSolver<double>& milpsolver, storm::models::sparse::Mdp<double, RM> const& mdp,
                                       storm::storage::BitVector const& goalstates, storm::storage::BitVector const& sinkstates)
        : PermissiveSchedulerComputation<RM>(mdp, goalstates, sinkstates), solver(milpsolver) {}

    void calculatePermissiveScheduler(bool lowerBound, double boundary) override {
        createMILP(lowerBound, boundary, this->mPenalties);
        // STORM_LOG_DEBUG("Calling optimizer");
        solver.optimize();
        // STORM_LOG_DEBUG("Done optimizing.")
        mCalledOptimizer = true;
    }

    bool foundSolution() const override {
        STORM_LOG_ASSERT(mCalledOptimizer, "Optimizer not called.");
        return !solver.isInfeasible();
    }

    SubMDPPermissiveScheduler<RM> getScheduler() const override {
        STORM_LOG_ASSERT(mCalledOptimizer, "Optimizer not called.");
        STORM_LOG_ASSERT(foundSolution(), "Solution not found.");

        SubMDPPermissiveScheduler<RM> result(this->mdp, true);
        for (auto const& entry : multistrategyVariables) {
            if (!solver.getBinaryValue(entry.second)) {
                result.disable(this->mdp.getChoiceIndex(entry.first));
            }
        }
        return result;
    }

    void dumpLpSolutionToFile(std::string const& filename) {
        std::ofstream filestream;
        storm::io::openFile(filename, filestream);
        for (auto const& pVar : mProbVariables) {
            filestream << pVar.second.getName() << "->" << solver.getContinuousValue(pVar.second) << '\n';
        }
        for (auto const& yVar : multistrategyVariables) {
            filestream << yVar.second.getName() << "->" << solver.getBinaryValue(yVar.second) << '\n';
        }
        for (auto const& aVar : mAlphaVariables) {
            filestream << aVar.second.getName() << "->" << solver.getBinaryValue(aVar.second) << '\n';
        }
        for (auto const& betaVar : mBetaVariables) {
            filestream << betaVar.second.getName() << "->" << solver.getBinaryValue(betaVar.second) << '\n';
        }
        for (auto const& gammaVar : mGammaVariables) {
            filestream << gammaVar.second.getName() << "->" << solver.getContinuousValue(gammaVar.second) << '\n';
        }
        storm::io::closeFile(filestream);
    }

    void dumpLpToFile(std::string const& filename) {
        solver.writeModelToFile(filename);
    }

   private:
    /**
     *  Create variables
     */
    void createVariables(PermissiveSchedulerPenalties const& penalties, storm::storage::BitVector const& relevantStates) {
        // We need the unique initial state later, so we get that one before looping.
        STORM_LOG_ASSERT(this->mdp.getInitialStates().getNumberOfSetBits() == 1, "No unique initial state.");
        uint_fast64_t initialStateIndex = this->mdp.getInitialStates().getNextSetIndex(0);

        storm::expressions::Variable var;
        for (uint_fast64_t s : relevantStates) {
            // Create x_s variables
            // Notice that only the initial probability appears in the objective.
            if (s == initialStateIndex) {
                var = solver.addLowerBoundedContinuousVariable("x_" + std::to_string(s), 0.0, -1.0);
            } else {
                var = solver.addLowerBoundedContinuousVariable("x_" + std::to_string(s), 0.0);
            }
            mProbVariables[s] = var;
            // Create alpha_s variables
            var = solver.addBinaryVariable("alp_" + std::to_string(s));
            mAlphaVariables[s] = var;
            // Create gamma_s variables
            var = solver.addBoundedContinuousVariable("gam_" + std::to_string(s), 0.0, 1.0);
            mGammaVariables[s] = var;
            for (uint_fast64_t a = 0; a < this->mdp.getNumberOfChoices(s); ++a) {
                auto stateAndAction = storage::StateActionPair(s, a);

                // Create y_(s,a) variables
                double penalty = penalties.get(stateAndAction);
                var = solver.addBinaryVariable("y_" + std::to_string(s) + "_" + std::to_string(a), -penalty);
                multistrategyVariables[stateAndAction] = var;

                // Create beta_(s,a,t) variables
                // Iterate over successors of s via a.
                for (auto const& entry : this->mdp.getTransitionMatrix().getRow(this->mdp.getNondeterministicChoiceIndices()[s] + a)) {
                    if (entry.getValue() != 0) {
                        storage::StateActionTarget sat = {s, a, entry.getColumn()};
                        var = solver.addBinaryVariable("beta_" + to_string(sat));
                        mBetaVariables[sat] = var;
                    }
                }
            }
        }
        solver.update();
    }

    /**
     * Create constraints
     */
    void createConstraints(bool lowerBound, double boundary, storm::storage::BitVector const& relevantStates) {
        // (5) and (7) are omitted on purpose (-- we currenty do not support controllability of actions -- )

        // (1)
        STORM_LOG_ASSERT(this->mdp.getInitialStates().getNumberOfSetBits() == 1, "No unique initial state.");
        uint_fast64_t initialStateIndex = this->mdp.getInitialStates().getNextSetIndex(0);
        STORM_LOG_ASSERT(relevantStates[initialStateIndex], "Initial state not relevant.");
        if (lowerBound) {
            solver.addConstraint("c1", mProbVariables[initialStateIndex] >= solver.getConstant(boundary));
        } else {
            solver.addConstraint("c1", mProbVariables[initialStateIndex] <= solver.getConstant(boundary));
        }
        for (uint_fast64_t s : relevantStates) {
            std::string stateString = std::to_string(s);
            storm::expressions::Expression expr = solver.getConstant(0.0);
            // (2)
            for (uint_fast64_t a = 0; a < this->mdp.getNumberOfChoices(s); ++a) {
                expr = expr + multistrategyVariables[storage::StateActionPair(s, a)];
            }
            solver.addConstraint("c2-" + stateString, solver.getConstant(1) <= expr);
            // (5)
            solver.addConstraint("c5-" + std::to_string(s), mProbVariables[s] <= mAlphaVariables[s]);

            // (3) For the relevant states.
            for (uint_fast64_t a = 0; a < this->mdp.getNumberOfChoices(s); ++a) {
                std::string sastring(stateString + "_" + std::to_string(a));
                expr = solver.getConstant(0.0);
                for (auto const& entry : this->mdp.getTransitionMatrix().getRow(this->mdp.getNondeterministicChoiceIndices()[s] + a)) {
                    if (entry.getValue() != 0 && relevantStates.get(entry.getColumn())) {
                        expr = expr + solver.getConstant(entry.getValue()) * mProbVariables[entry.getColumn()];
                    } else if (entry.getValue() != 0 && this->mGoals.get(entry.getColumn())) {
                        expr = expr + solver.getConstant(entry.getValue());
                    }
                }
                if (lowerBound) {
                    solver.addConstraint("c3-" + sastring,
                                         mProbVariables[s] <= (solver.getConstant(1) - multistrategyVariables[storage::StateActionPair(s, a)]) + expr);
                } else {
                    solver.addConstraint("c3-" + sastring,
                                         mProbVariables[s] >= (solver.getConstant(1) - multistrategyVariables[storage::StateActionPair(s, a)]) + expr);
                }
            }

            for (uint_fast64_t a = 0; a < this->mdp.getNumberOfChoices(s); ++a) {
                // (6)
                std::string sastring(stateString + "_" + std::to_string(a));
                expr = solver.getConstant(0.0);
                for (auto const& entry : this->mdp.getTransitionMatrix().getRow(this->mdp.getNondeterministicChoiceIndices()[s] + a)) {
                    if (entry.getValue() != 0) {
                        storage::StateActionTarget sat = {s, a, entry.getColumn()};
                        expr = expr + mBetaVariables[sat];
                    }
                }
                solver.addConstraint("c6-" + sastring,
                                     multistrategyVariables[storage::StateActionPair(s, a)] == (solver.getConstant(1) - mAlphaVariables[s]) + expr);

                for (auto const& entry : this->mdp.getTransitionMatrix().getRow(this->mdp.getNondeterministicChoiceIndices()[s] + a)) {
                    if (entry.getValue() != 0) {
                        storage::StateActionTarget sat = {s, a, entry.getColumn()};
                        std::string satstring = to_string(sat);
                        // (8)
                        if (relevantStates[entry.getColumn()]) {
                            STORM_LOG_ASSERT(mGammaVariables.count(entry.getColumn()) > 0, "Entry not found.");
                            STORM_LOG_ASSERT(mGammaVariables.count(s) > 0, "Entry not found.");
                            STORM_LOG_ASSERT(mBetaVariables.count(sat) > 0, "Entry not found.");
                            solver.addConstraint("c8-" + satstring,
                                                 mGammaVariables[entry.getColumn()] < mGammaVariables[s] + (solver.getConstant(1) - mBetaVariables[sat]));
                        }
                    }
                }
            }
        }
    }

    /**
     *
     */
    void createMILP(bool lowerBound, double boundary, PermissiveSchedulerPenalties const& penalties) {
        storm::storage::BitVector irrelevant = this->mGoals | this->mSinks;
        storm::storage::BitVector relevantStates = ~irrelevant;
        // Notice that the separated construction of variables and
        // constraints slows down the construction of the MILP.
        // In the future, we might want to merge this.
        createVariables(penalties, relevantStates);
        createConstraints(lowerBound, boundary, relevantStates);

        solver.setOptimizationDirection(storm::OptimizationDirection::Minimize);
    }
};
}  // namespace ps
}  // namespace storm
