#pragma once

#include <boost/optional.hpp>
#include <memory>

#include "storm/adapters/RationalNumberForward.h"
#include "storm/environment/Environment.h"
#include "storm/environment/SubEnvironment.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {

// Forward declare subenvironments
class EigenSolverEnvironment;
class EliminationSolverEnvironment;
class GameSolverEnvironment;
class GlpkSolverEnvironment;
class GmmxxSolverEnvironment;
class GurobiSolverEnvironment;
class LongRunAverageSolverEnvironment;
class MinMaxSolverEnvironment;
class MultiplierEnvironment;
class NativeSolverEnvironment;
class OviSolverEnvironment;
class TimeBoundedSolverEnvironment;
class TopologicalSolverEnvironment;

// Explicitly instantiated once in SubEnvironment.cpp
// Avoids redundant re-instantiation elsewhere
extern template class SubEnvironment<EigenSolverEnvironment>;
extern template class SubEnvironment<EliminationSolverEnvironment>;
extern template class SubEnvironment<GameSolverEnvironment>;
extern template class SubEnvironment<GlpkSolverEnvironment>;
extern template class SubEnvironment<GmmxxSolverEnvironment>;
extern template class SubEnvironment<GurobiSolverEnvironment>;
extern template class SubEnvironment<LongRunAverageSolverEnvironment>;
extern template class SubEnvironment<MinMaxSolverEnvironment>;
extern template class SubEnvironment<MultiplierEnvironment>;
extern template class SubEnvironment<NativeSolverEnvironment>;
extern template class SubEnvironment<OviSolverEnvironment>;
extern template class SubEnvironment<TimeBoundedSolverEnvironment>;
extern template class SubEnvironment<TopologicalSolverEnvironment>;

class SolverEnvironment {
   public:
    SolverEnvironment();
    ~SolverEnvironment();

    EigenSolverEnvironment& eigen();
    EigenSolverEnvironment const& eigen() const;

    EliminationSolverEnvironment& elimination();
    EliminationSolverEnvironment const& elimination() const;

    GameSolverEnvironment& game();
    GameSolverEnvironment const& game() const;

    GlpkSolverEnvironment& glpk();
    GlpkSolverEnvironment const& glpk() const;

    GmmxxSolverEnvironment& gmmxx();
    GmmxxSolverEnvironment const& gmmxx() const;

    GurobiSolverEnvironment& gurobi();
    GurobiSolverEnvironment const& gurobi() const;

    LongRunAverageSolverEnvironment& lra();
    LongRunAverageSolverEnvironment const& lra() const;

    MinMaxSolverEnvironment& minMax();
    MinMaxSolverEnvironment const& minMax() const;

    MultiplierEnvironment& multiplier();
    MultiplierEnvironment const& multiplier() const;

    NativeSolverEnvironment& native();
    NativeSolverEnvironment const& native() const;

    OviSolverEnvironment& ovi();
    OviSolverEnvironment const& ovi() const;

    TimeBoundedSolverEnvironment& timeBounded();
    TimeBoundedSolverEnvironment const& timeBounded() const;

    TopologicalSolverEnvironment& topological();
    TopologicalSolverEnvironment const& topological() const;

    storm::solver::EquationSolverType const& getLinearEquationSolverType() const;
    void setLinearEquationSolverType(storm::solver::EquationSolverType const& value, bool isSetFromDefault = false);
    bool isLinearEquationSolverTypeSetFromDefaultValue() const;

    storm::solver::LpSolverType const& getLpSolverType() const;
    void setLpSolverType(storm::solver::LpSolverType const& value, bool isSetFromDefault = false);
    bool isLpSolverTypeSetFromDefaultValue() const;

    std::pair<boost::optional<storm::RationalNumber>, boost::optional<bool>> getPrecisionOfLinearEquationSolver(
        storm::solver::EquationSolverType const& solverType) const;
    void setLinearEquationSolverPrecision(boost::optional<storm::RationalNumber> const& newPrecision,
                                          boost::optional<bool> const& relativePrecision = boost::none);

    bool isForceSoundness() const;
    void setForceSoundness(bool value);
    bool isForceExact() const;
    void setForceExact(bool value);
    bool isDebugSet() const;
    void setDebug(bool value);
    bool isVerboseSet() const;
    void setVerbose(bool value);
    uint64_t getShowProgressDelay() const;
    void setShowProgressDelay(uint64_t delay);

   private:
    SubEnvironment<EigenSolverEnvironment> eigenSolverEnvironment;
    SubEnvironment<EliminationSolverEnvironment> eliminationSolverEnvironment;
    SubEnvironment<GameSolverEnvironment> gameSolverEnvironment;
    SubEnvironment<GlpkSolverEnvironment> glpkSolverEnvironment;
    SubEnvironment<GmmxxSolverEnvironment> gmmxxSolverEnvironment;
    SubEnvironment<GurobiSolverEnvironment> gurobiSolverEnvironment;
    SubEnvironment<LongRunAverageSolverEnvironment> longRunAverageSolverEnvironment;
    SubEnvironment<MinMaxSolverEnvironment> minMaxSolverEnvironment;
    SubEnvironment<MultiplierEnvironment> multiplierEnvironment;
    SubEnvironment<NativeSolverEnvironment> nativeSolverEnvironment;
    SubEnvironment<OviSolverEnvironment> oviSolverEnvironment;
    SubEnvironment<TimeBoundedSolverEnvironment> timeBoundedSolverEnvironment;
    SubEnvironment<TopologicalSolverEnvironment> topologicalSolverEnvironment;

    storm::solver::EquationSolverType linearEquationSolverType;
    bool linearEquationSolverTypeSetFromDefault;
    storm::solver::LpSolverType lpSolverType;
    bool lpSolverTypeSetFromDefault;
    bool forceSoundness;
    bool forceExact;
    bool debug;
    bool verbose;
    uint64_t showProgressDelay;
};
}  // namespace storm
