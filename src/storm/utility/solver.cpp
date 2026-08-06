#include "storm/utility/solver.h"

#include "storm/environment/Environment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/CoreSettings.h"
#include "storm/solver/GlpkLpSolver.h"
#include "storm/solver/GurobiLpSolver.h"
#include "storm/solver/MathsatSmtSolver.h"
#include "storm/solver/SoplexLpSolver.h"
#include "storm/solver/Z3LpSolver.h"
#include "storm/solver/Z3SmtSolver.h"
#include "storm/utility/NumberTraits.h"

namespace storm {
namespace utility {
namespace solver {

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType>> GlpkLpSolverFactory<ValueType>::create(storm::Environment const& env, std::string const& name) const {
    return std::unique_ptr<storm::solver::LpSolver<ValueType>>(
        new storm::solver::GlpkLpSolver<ValueType>(env.solver().glpk(), env.solver().isDebugSet(), name));
}

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType, true>> GlpkLpSolverFactory<ValueType>::createRaw(storm::Environment const& env,
                                                                                                    std::string const& name) const {
    return std::unique_ptr<storm::solver::LpSolver<ValueType, true>>(
        new storm::solver::GlpkLpSolver<ValueType, true>(env.solver().glpk(), env.solver().isDebugSet(), name));
}

template<typename ValueType>
std::unique_ptr<LpSolverFactory<ValueType>> GlpkLpSolverFactory<ValueType>::clone() const {
    return std::make_unique<GlpkLpSolverFactory<ValueType>>(*this);
}

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType>> SoplexLpSolverFactory<ValueType>::create(storm::Environment const&, std::string const& name) const {
    return std::unique_ptr<storm::solver::LpSolver<ValueType>>(new storm::solver::SoplexLpSolver<ValueType>(name));
}

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType, true>> SoplexLpSolverFactory<ValueType>::createRaw(storm::Environment const&,
                                                                                                      std::string const& name) const {
    return std::unique_ptr<storm::solver::LpSolver<ValueType, true>>(new storm::solver::SoplexLpSolver<ValueType, true>(name));
}

template<typename ValueType>
std::unique_ptr<LpSolverFactory<ValueType>> SoplexLpSolverFactory<ValueType>::clone() const {
    return std::make_unique<SoplexLpSolverFactory<ValueType>>(*this);
}

template<typename ValueType>
std::shared_ptr<storm::solver::GurobiEnvironment> const& GurobiLpSolverFactory<ValueType>::getOrCreateGurobiEnvironment(storm::Environment const& env) const {
    if (!environment) {
        environment = std::make_shared<storm::solver::GurobiEnvironment>();
        environment->initialize(env.solver().gurobi(), env.solver().isDebugSet());
    }
    return environment;
}

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType>> GurobiLpSolverFactory<ValueType>::create(storm::Environment const& env, std::string const& name) const {
    return std::unique_ptr<storm::solver::LpSolver<ValueType>>(new storm::solver::GurobiLpSolver<ValueType>(getOrCreateGurobiEnvironment(env), name));
}

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType, true>> GurobiLpSolverFactory<ValueType>::createRaw(storm::Environment const& env,
                                                                                                      std::string const& name) const {
    return std::unique_ptr<storm::solver::LpSolver<ValueType, true>>(
        new storm::solver::GurobiLpSolver<ValueType, true>(getOrCreateGurobiEnvironment(env), name));
}

template<typename ValueType>
std::unique_ptr<LpSolverFactory<ValueType>> GurobiLpSolverFactory<ValueType>::clone() const {
    return std::make_unique<GurobiLpSolverFactory<ValueType>>(*this);
}

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType>> Z3LpSolverFactory<ValueType>::create(storm::Environment const&, std::string const& name) const {
    return std::unique_ptr<storm::solver::LpSolver<ValueType>>(new storm::solver::Z3LpSolver<ValueType>(name));
}

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType, true>> Z3LpSolverFactory<ValueType>::createRaw(storm::Environment const&, std::string const& name) const {
    return std::unique_ptr<storm::solver::LpSolver<ValueType, true>>(new storm::solver::Z3LpSolver<ValueType, true>(name));
}

template<typename ValueType>
std::unique_ptr<LpSolverFactory<ValueType>> Z3LpSolverFactory<ValueType>::clone() const {
    return std::make_unique<Z3LpSolverFactory<ValueType>>(*this);
}

template<typename ValueType>
std::unique_ptr<LpSolverFactory<ValueType>> getLpSolverFactory(storm::Environment const& env, storm::solver::LpSolverTypeSelection solvType) {
    storm::solver::LpSolverType t;
    if (solvType == storm::solver::LpSolverTypeSelection::FROMSETTINGS) {
        t = env.solver().getLpSolverType();
        bool useExact = storm::NumberTraits<ValueType>::IsExact || env.solver().isForceExact();
        if (useExact && t != storm::solver::LpSolverType::Z3 && env.solver().isLpSolverTypeSetFromDefaultValue()) {
            t = storm::solver::LpSolverType::Z3;
        }
    } else {
        t = convert(solvType);
    }
    switch (t) {
        case storm::solver::LpSolverType::Gurobi:
            return std::unique_ptr<LpSolverFactory<ValueType>>(new GurobiLpSolverFactory<ValueType>());
        case storm::solver::LpSolverType::Glpk:
            return std::unique_ptr<LpSolverFactory<ValueType>>(new GlpkLpSolverFactory<ValueType>());
        case storm::solver::LpSolverType::Z3:
            return std::unique_ptr<LpSolverFactory<ValueType>>(new Z3LpSolverFactory<ValueType>());
        case storm::solver::LpSolverType::Soplex:
            return std::unique_ptr<LpSolverFactory<ValueType>>(new SoplexLpSolverFactory<ValueType>());
    }
    return nullptr;
}

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType>> getLpSolver(storm::Environment const& env, std::string const& name,
                                                                storm::solver::LpSolverTypeSelection solvType) {
    std::unique_ptr<storm::utility::solver::LpSolverFactory<ValueType>> factory = getLpSolverFactory<ValueType>(env, solvType);
    return factory->create(env, name);
}

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType, true>> getRawLpSolver(storm::Environment const& env, std::string const& name,
                                                                         storm::solver::LpSolverTypeSelection solvType) {
    std::unique_ptr<storm::utility::solver::LpSolverFactory<ValueType>> factory = getLpSolverFactory<ValueType>(env, solvType);
    return factory->createRaw(env, name);
}

std::unique_ptr<storm::solver::SmtSolver> SmtSolverFactory::create(storm::expressions::ExpressionManager& manager) const {
    storm::solver::SmtSolverType smtSolverType;
    if (storm::settings::hasModule<storm::settings::modules::CoreSettings>()) {
        smtSolverType = storm::settings::getModule<storm::settings::modules::CoreSettings>().getSmtSolver();
    } else {
#ifdef STORM_HAVE_Z3
        smtSolverType = storm::solver::SmtSolverType::Z3;
#elif defined STORM_HAVE_MATHSAT
        smtSolverType = storm::solver::SmtSolverType::Mathsat;
#else
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Requested an SMT solver but none was installed.");
#endif
    }
    switch (smtSolverType) {
        case storm::solver::SmtSolverType::Z3:
            return std::unique_ptr<storm::solver::SmtSolver>(new storm::solver::Z3SmtSolver(manager));
        case storm::solver::SmtSolverType::Mathsat:
            return std::unique_ptr<storm::solver::SmtSolver>(new storm::solver::MathsatSmtSolver(manager));
    }
    return nullptr;
}

std::unique_ptr<storm::solver::SmtSolver> Z3SmtSolverFactory::create(storm::expressions::ExpressionManager& manager) const {
    return std::unique_ptr<storm::solver::SmtSolver>(new storm::solver::Z3SmtSolver(manager));
}

std::unique_ptr<storm::solver::SmtSolver> MathsatSmtSolverFactory::create(storm::expressions::ExpressionManager& manager) const {
    return std::unique_ptr<storm::solver::SmtSolver>(new storm::solver::MathsatSmtSolver(manager));
}

std::unique_ptr<storm::solver::SmtSolver> getSmtSolver(storm::expressions::ExpressionManager& manager) {
    std::unique_ptr<storm::utility::solver::SmtSolverFactory> factory = std::make_unique<SmtSolverFactory>();
    return factory->create(manager);
}

template class LpSolverFactory<double>;
template class LpSolverFactory<storm::RationalNumber>;
template class GlpkLpSolverFactory<double>;
template class GlpkLpSolverFactory<storm::RationalNumber>;
template class GurobiLpSolverFactory<double>;
template class GurobiLpSolverFactory<storm::RationalNumber>;
template class Z3LpSolverFactory<double>;
template class Z3LpSolverFactory<storm::RationalNumber>;
template class SoplexLpSolverFactory<double>;
template class SoplexLpSolverFactory<storm::RationalNumber>;

template std::unique_ptr<LpSolverFactory<double>> getLpSolverFactory(storm::Environment const& env, storm::solver::LpSolverTypeSelection solvType);
template std::unique_ptr<LpSolverFactory<storm::RationalNumber>> getLpSolverFactory(storm::Environment const& env,
                                                                                    storm::solver::LpSolverTypeSelection solvType);
template std::unique_ptr<storm::solver::LpSolver<double>> getLpSolver(storm::Environment const& env, std::string const& name,
                                                                      storm::solver::LpSolverTypeSelection solvType);
template std::unique_ptr<storm::solver::LpSolver<storm::RationalNumber>> getLpSolver(storm::Environment const& env, std::string const& name,
                                                                                     storm::solver::LpSolverTypeSelection solvType);
template std::unique_ptr<storm::solver::LpSolver<double, true>> getRawLpSolver(storm::Environment const& env, std::string const& name,
                                                                               storm::solver::LpSolverTypeSelection solvType);
template std::unique_ptr<storm::solver::LpSolver<storm::RationalNumber, true>> getRawLpSolver(storm::Environment const& env, std::string const& name,
                                                                                              storm::solver::LpSolverTypeSelection solvType);
}  // namespace solver
}  // namespace utility
}  // namespace storm
