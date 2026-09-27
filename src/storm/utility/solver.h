#pragma once

#include <memory>
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {

class Environment;

namespace solver {

template<typename ValueType, bool RawMode>
class LpSolver;

class GurobiEnvironment;

class SmtSolver;
}  // namespace solver

namespace expressions {
class ExpressionManager;
}  // namespace expressions
}  // namespace storm

namespace storm::utility::solver {
template<typename ValueType>
class LpSolverFactory {
   public:
    virtual ~LpSolverFactory() = default;

    /*!
     * Creates a new linear equation solver instance with the given name.
     *
     * @param name The name of the LP solver.
     * @return A pointer to the newly created solver.
     */
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, false>> create(storm::Environment const& env, std::string const& name) const = 0;
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, true>> createRaw(storm::Environment const& env, std::string const& name) const = 0;
    virtual std::unique_ptr<LpSolverFactory<ValueType>> clone() const = 0;
};

template<typename ValueType>
class GlpkLpSolverFactory : public LpSolverFactory<ValueType> {
   public:
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, false>> create(storm::Environment const& env, std::string const& name) const override;
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, true>> createRaw(storm::Environment const& env, std::string const& name) const override;
    virtual std::unique_ptr<LpSolverFactory<ValueType>> clone() const override;
};

template<typename ValueType>
class SoplexLpSolverFactory : public LpSolverFactory<ValueType> {
   public:
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, false>> create(storm::Environment const& env, std::string const& name) const override;
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, true>> createRaw(storm::Environment const& env, std::string const& name) const override;
    virtual std::unique_ptr<LpSolverFactory<ValueType>> clone() const override;
};

template<typename ValueType>
class HighsLpSolverFactory : public LpSolverFactory<ValueType> {
   public:
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, false>> create(storm::Environment const& env, std::string const& name) const override;
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, true>> createRaw(storm::Environment const& env, std::string const& name) const override;
    virtual std::unique_ptr<LpSolverFactory<ValueType>> clone() const override;
};

template<typename ValueType>
class GurobiLpSolverFactory : public LpSolverFactory<ValueType> {
   public:
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, false>> create(storm::Environment const& env, std::string const& name) const override;
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, true>> createRaw(storm::Environment const& env, std::string const& name) const override;
    virtual std::unique_ptr<LpSolverFactory<ValueType>> clone() const override;

   private:
    std::shared_ptr<storm::solver::GurobiEnvironment> const& getOrCreateGurobiEnvironment(storm::Environment const& env) const;

    mutable std::shared_ptr<storm::solver::GurobiEnvironment> environment;
};

template<typename ValueType>
class Z3LpSolverFactory : public LpSolverFactory<ValueType> {
   public:
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, false>> create(storm::Environment const& env, std::string const& name) const override;
    virtual std::unique_ptr<storm::solver::LpSolver<ValueType, true>> createRaw(storm::Environment const& env, std::string const& name) const override;
    virtual std::unique_ptr<LpSolverFactory<ValueType>> clone() const override;
};

template<typename ValueType>
std::unique_ptr<LpSolverFactory<ValueType>> getLpSolverFactory(
    storm::Environment const& env, storm::solver::LpSolverTypeSelection solvType = storm::solver::LpSolverTypeSelection::FROMSETTINGS);

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType, false>> getLpSolver(
    storm::Environment const& env, std::string const& name, storm::solver::LpSolverTypeSelection solvType = storm::solver::LpSolverTypeSelection::FROMSETTINGS);

template<typename ValueType>
std::unique_ptr<storm::solver::LpSolver<ValueType, true>> getRawLpSolver(
    storm::Environment const& env, std::string const& name, storm::solver::LpSolverTypeSelection solvType = storm::solver::LpSolverTypeSelection::FROMSETTINGS);

class SmtSolverFactory {
   public:
    virtual ~SmtSolverFactory() = default;

    /*!
     * Creates a new SMT solver instance.
     *
     * The SMT solver is the one that was selected at compile time (see the CMake option
     * STORM_DEFAULT_SMT_SOLVER).
     *
     * @param manager The expression manager responsible for the expressions that will be given to the SMT
     * solver.
     * @return A pointer to the newly created solver.
     */
    virtual std::unique_ptr<storm::solver::SmtSolver> create(storm::expressions::ExpressionManager& manager) const;

    /*!
     * Creates a new SMT solver instance, taking the SMT solver selected in the given environment into account.
     *
     * The environment takes precedence: the SMT solver stored in it is used, no matter whether it was
     * selected explicitly or seeded from the ``smtsolver`` core setting.
     *
     * @param env The environment determining the SMT solver to use.
     * @param manager The expression manager responsible for the expressions that will be given to the SMT
     * solver.
     * @return A pointer to the newly created solver.
     */
    virtual std::unique_ptr<storm::solver::SmtSolver> create(storm::Environment const& env, storm::expressions::ExpressionManager& manager) const;
};

// The factory for the compile-time chosen default SMT solver (see the CMake option STORM_DEFAULT_SMT_SOLVER).
using DefaultSmtSolverFactory = SmtSolverFactory;

class Z3SmtSolverFactory : public SmtSolverFactory {
   public:
    virtual std::unique_ptr<storm::solver::SmtSolver> create(storm::expressions::ExpressionManager& manager) const;
    virtual std::unique_ptr<storm::solver::SmtSolver> create(storm::Environment const& env, storm::expressions::ExpressionManager& manager) const;
};

class MathsatSmtSolverFactory : public SmtSolverFactory {
   public:
    virtual std::unique_ptr<storm::solver::SmtSolver> create(storm::expressions::ExpressionManager& manager) const;
    virtual std::unique_ptr<storm::solver::SmtSolver> create(storm::Environment const& env, storm::expressions::ExpressionManager& manager) const;
};

class Cvc5SmtSolverFactory : public SmtSolverFactory {
   public:
    virtual std::unique_ptr<storm::solver::SmtSolver> create(storm::expressions::ExpressionManager& manager) const;
    virtual std::unique_ptr<storm::solver::SmtSolver> create(storm::Environment const& env, storm::expressions::ExpressionManager& manager) const;
};

std::unique_ptr<storm::solver::SmtSolver> getSmtSolver(storm::expressions::ExpressionManager& manager);

/*!
 * Creates a new SMT solver instance, honoring an SMT solver selected in the given environment.
 *
 * @param env The environment determining the SMT solver to use.
 * @param manager The expression manager responsible for the expressions that will be given to the SMT
 * solver.
 * @return A pointer to the newly created solver.
 */
std::unique_ptr<storm::solver::SmtSolver> getSmtSolver(storm::Environment const& env, storm::expressions::ExpressionManager& manager);
}  // namespace storm::utility::solver
