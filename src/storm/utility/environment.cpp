#include "environment.h"

#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/ModelCheckerEnvironment.h"
#include "storm/environment/solver/AllSolverEnvironments.h"
#include "storm/environment/solver/SolverEnvironment.h"

namespace storm::utility::environment {

storm::Environment EnvironmentFactory::create() const {
    return storm::Environment();
}

std::unique_ptr<EnvironmentFactory> EnvironmentFactory::clone() const {
    return std::make_unique<EnvironmentFactory>(*this);
}

storm::Environment SoundEnvironmentFactory::create() const {
    storm::Environment env = EnvironmentFactory::create();
    storm::SolverEnvironment& solver = env.solver();
    solver.setForceSoundness(true);  // The same as --sound from the CLI

    // The linear equation solver type and the min-max method default to topological, which is sound and thus kept.
    // Instead, the solvers underlying the topological solver are set.
    solver.topological().setUnderlyingEquationSolverType(storm::solver::EquationSolverType::Native, true);
    solver.topological().setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod::OptimisticValueIteration, true);
    solver.native().setMethod(storm::solver::NativeLinearEquationSolverMethod::OptimisticValueIteration, true);
    solver.eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU, true);
    solver.game().setMethod(storm::solver::GameMethod::PolicyIteration, true);
    solver.lra().setDetLraMethod(storm::solver::LraMethod::ValueIteration, true);
    solver.lra().setNondetLraMethod(storm::solver::LraMethod::ValueIteration, true);
    // Corresponds to the choice for the automatic steady state distribution algorithm
    env.modelchecker().setSteadyStateDistributionAlgorithm(storm::SteadyStateDistributionAlgorithm::ExpectedVisitingTimes);
    return env;
}

std::unique_ptr<EnvironmentFactory> SoundEnvironmentFactory::clone() const {
    return std::make_unique<SoundEnvironmentFactory>(*this);
}

storm::Environment getSoundEnvironment() {
    return SoundEnvironmentFactory().create();
}

storm::Environment ExactEnvironmentFactory::create() const {
    storm::Environment env = EnvironmentFactory::create();
    storm::SolverEnvironment& solver = env.solver();
    solver.setForceExact(true);  // The same as --exact from the CLI

    // The linear equation solver type and the min-max method default to topological, which supports exact computations and is thus kept.
    // Instead, the solvers underlying the topological solver are set.
    solver.topological().setUnderlyingEquationSolverType(storm::solver::EquationSolverType::Eigen, true);
    solver.topological().setUnderlyingMinMaxMethod(storm::solver::MinMaxMethod::PolicyIteration, true);
    solver.native().setMethod(storm::solver::NativeLinearEquationSolverMethod::RationalSearch, true);
    solver.eigen().setMethod(storm::solver::EigenLinearEquationSolverMethod::SparseLU, true);
    solver.game().setMethod(storm::solver::GameMethod::PolicyIteration, true);
    solver.lra().setDetLraMethod(storm::solver::LraMethod::GainBiasEquations, true);
    solver.lra().setNondetLraMethod(storm::solver::LraMethod::LinearProgramming, true);
    // Exact LP solving
    solver.setLpSolverType(storm::solver::LpSolverType::Z3, true);
    return env;
}

std::unique_ptr<EnvironmentFactory> ExactEnvironmentFactory::clone() const {
    return std::make_unique<ExactEnvironmentFactory>(*this);
}

storm::Environment getExactEnvironment() {
    return ExactEnvironmentFactory().create();
}

}  // namespace storm::utility::environment
