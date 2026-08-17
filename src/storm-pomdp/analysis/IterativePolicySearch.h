#pragma once

#include <sstream>
#include <vector>

#include "storm-pomdp/analysis/WinningRegion.h"
#include "storm-pomdp/analysis/WinningRegionQueryInterface.h"
#include "storm/models/sparse/Pomdp.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/expressions/Expressions.h"
#include "storm/utility/Stopwatch.h"
#include "storm/utility/solver.h"

namespace storm {
namespace pomdp {

enum class MemlessSearchPathVariables { BooleanRanking, IntegerRanking, RealRanking };
inline MemlessSearchPathVariables pathVariableTypeFromString(std::string const& in) {
    if (in == "int") {
        return MemlessSearchPathVariables::IntegerRanking;
    } else if (in == "real") {
        return MemlessSearchPathVariables::RealRanking;
    } else {
        STORM_LOG_ASSERT(in == "bool", "Expected 'bool' path variable type.");
        return MemlessSearchPathVariables::BooleanRanking;
    }
}

class MemlessSearchOptions {
   public:
    void setExportSATCalls(std::string const& path) {
        exportSATcalls = path;
    }

    std::string const& getExportSATCallsPath() const {
        return exportSATcalls;
    }

    bool isExportSATSet() const {
        return exportSATcalls != "";
    }

    void setDebugLevel(uint64_t level = 1) {
        debugLevel = level;
    }

    bool computeInfoOutput() const {
        return debugLevel > 0;
    }

    bool computeDebugOutput() const {
        return debugLevel > 1;
    }

    bool computeTraceOutput() const {
        return debugLevel > 2;
    }

    bool onlyDeterministicStrategies = false;
    bool forceLookahead = false;
    bool validateEveryStep = false;
    bool validateResult = false;
    MemlessSearchPathVariables pathVariableType = MemlessSearchPathVariables::RealRanking;
    uint64_t restartAfterNIterations = 250;
    uint64_t extensionCallTimeout = 0u;
    uint64_t localIterationMaximum = 600;

   private:
    std::string exportSATcalls = "";
    uint64_t debugLevel = 0;
};

struct InternalObservationScheduler {
    std::vector<storm::storage::BitVector> actions;
    std::vector<uint64_t> schedulerRef;
    storm::storage::BitVector switchObservations;

    void reset(uint64_t nrObservations, uint64_t nrActions) {
        actions = std::vector<storm::storage::BitVector>(nrObservations, storm::storage::BitVector(nrActions));
        schedulerRef = std::vector<uint64_t>(nrObservations, 0);
        switchObservations.clear();
    }

    bool empty() const {
        return actions.empty();
    }

    void printForObservations(storm::storage::BitVector const& observations, storm::storage::BitVector const& observationsAfterSwitch) const {
        for (uint64_t obs = 0; obs < observations.size(); ++obs) {
            if (observations.get(obs) || observationsAfterSwitch.get(obs)) {
                STORM_LOG_INFO("For observation: " << obs);
            }
            if (observations.get(obs)) {
                std::stringstream ss;
                ss << "actions:";
                for (auto act : actions[obs]) {
                    ss << " " << act;
                }
                if (switchObservations.get(obs)) {
                    ss << " and switch.";
                }
                STORM_LOG_INFO(ss.str());
            }
            if (observationsAfterSwitch.get(obs)) {
                STORM_LOG_INFO("scheduler ref: " << schedulerRef[obs]);
            }
        }
    }
};

template<typename ValueType>
class IterativePolicySearch {
    // Implements an extension to the Chatterjee, Chmelik, Davies (AAAI-16) paper.

   public:
    class Statistics {
       public:
        Statistics() = default;
        void print() const;

        storm::utility::Stopwatch totalTimer;
        storm::utility::Stopwatch smtCheckTimer;
        storm::utility::Stopwatch initializeSolverTimer;
        storm::utility::Stopwatch evaluateExtensionSolverTime;
        storm::utility::Stopwatch encodeExtensionSolverTime;
        storm::utility::Stopwatch updateNewStrategySolverTime;
        storm::utility::Stopwatch graphSearchTime;

        storm::utility::Stopwatch winningRegionUpdatesTimer;

        void incrementOuterIterations() {
            outerIterations++;
        }

        void incrementSmtChecks() {
            satCalls++;
        }

        uint64_t getChecks() {
            return satCalls;
        }

        uint64_t getIterations() {
            return outerIterations;
        }

        uint64_t getGraphBasedwinningObservations() {
            return graphBasedAnalysisWinOb;
        }

        void incrementGraphBasedWinningObservations() {
            graphBasedAnalysisWinOb++;
        }

       private:
        uint64_t satCalls = 0;
        uint64_t outerIterations = 0;
        uint64_t graphBasedAnalysisWinOb = 0;
    };

    IterativePolicySearch(storm::models::sparse::Pomdp<ValueType> const& pomdp, storm::storage::BitVector const& targetStates,
                          storm::storage::BitVector const& surelyReachSinkStates,

                          std::shared_ptr<storm::utility::solver::SmtSolverFactory>& smtSolverFactory, MemlessSearchOptions const& options);

    bool analyzeForInitialStates(uint64_t k) {
        stats.totalTimer.start();
        STORM_LOG_TRACE("Bad states: " << surelyReachSinkStates);
        STORM_LOG_TRACE("Target states: " << targetStates);
        STORM_LOG_TRACE("Questionmark states: " << (~surelyReachSinkStates & ~targetStates));
        bool result = analyze(k, ~surelyReachSinkStates & ~targetStates, pomdp.getInitialStates());
        stats.totalTimer.stop();
        return result;
    }

    void computeWinningRegion(uint64_t k) {
        stats.totalTimer.start();
        analyze(k, ~surelyReachSinkStates & ~targetStates);
        stats.totalTimer.stop();
    }

    WinningRegion const& getLastWinningRegion() const {
        return winningRegion;
    }

    uint64_t getOffsetFromObservation(uint64_t state, uint64_t observation) const;

    bool analyze(uint64_t k, storm::storage::BitVector const& oneOfTheseStates,
                 storm::storage::BitVector const& allOfTheseStates = storm::storage::BitVector());

    Statistics const& getStatistics() const;
    void finalizeStatistics();

   private:
    void reset() {
        STORM_LOG_INFO("Reset solver to restart with current winning region");
        schedulerForObs.clear();
        finalSchedulers.clear();
        smtSolver->reset();
    }
    void printScheduler(std::vector<InternalObservationScheduler> const&);
    void coveredStatesToStream(std::ostream& os, storm::storage::BitVector const& remaining) const;

    bool initialize(uint64_t k);

    bool smtCheck(uint64_t iteration, std::set<storm::expressions::Expression> const& assumptions = {});

    std::unique_ptr<storm::solver::SmtSolver> smtSolver;
    storm::models::sparse::Pomdp<ValueType> const& pomdp;
    std::shared_ptr<storm::expressions::ExpressionManager> expressionManager;
    uint64_t maxK = std::numeric_limits<uint64_t>::max();

    storm::storage::BitVector surelyReachSinkStates;
    storm::storage::BitVector targetStates;
    std::vector<std::vector<uint64_t>> statesPerObservation;

    std::vector<storm::expressions::Variable> schedulerVariables;
    std::vector<storm::expressions::Expression> schedulerVariableExpressions;
    std::vector<std::vector<storm::expressions::Expression>> actionSelectionVarExpressions;  // A_{z,a}
    std::vector<std::vector<storm::expressions::Variable>> actionSelectionVars;              // A_{z,a}

    std::vector<storm::expressions::Variable> reachVars;
    std::vector<storm::expressions::Expression> reachVarExpressions;
    std::vector<std::vector<storm::expressions::Expression>> reachVarExpressionsPerObservation;

    std::vector<storm::expressions::Variable> observationUpdatedVariables;
    std::vector<storm::expressions::Expression> observationUpdatedExpressions;

    std::vector<storm::expressions::Variable> switchVars;
    std::vector<storm::expressions::Expression> switchVarExpressions;
    std::vector<storm::expressions::Variable> followVars;
    std::vector<storm::expressions::Expression> followVarExpressions;
    std::vector<storm::expressions::Variable> continuationVars;
    std::vector<storm::expressions::Expression> continuationVarExpressions;
    std::vector<std::vector<storm::expressions::Variable>> pathVars;
    std::vector<std::vector<storm::expressions::Expression>> pathVarExpressions;

    std::vector<InternalObservationScheduler> finalSchedulers;
    std::vector<uint64_t> schedulerForObs;
    WinningRegion winningRegion;

    MemlessSearchOptions options;
    Statistics stats;

    std::shared_ptr<storm::utility::solver::SmtSolverFactory>& smtSolverFactory;
    std::shared_ptr<WinningRegionQueryInterface<ValueType>> validator;

    mutable bool useFindOffset = false;
};
}  // namespace pomdp
}  // namespace storm
