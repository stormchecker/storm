#include "storm-config.h"
#include "test/storm_gtest.h"

#include <memory>
#include <string>

#include "storm-pars/api/region.h"
#include "storm-pars/modelchecker/region/SparseDtmcParameterLiftingModelChecker.h"
#include "storm-pars/storage/ParameterRegion.h"
#include "storm-pars/transformer/IntervalEndComponentPreserver.h"
#include "storm-pars/transformer/RobustParameterLifter.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/builder.h"
#include "storm/environment/Environment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/reachability/SparseDtmcEliminationModelChecker.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Model.h"
#include "storm/solver/IterativeMinMaxLinearEquationSolver.h"
#include "storm/solver/MinMaxLinearEquationSolver.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/prism/Program.h"
#include "storm/utility/constants.h"
#include "storm/utility/vector.h"

void testModelInterval(std::string programFile, std::string formulaAsString, std::string constantsAsString) {
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalFunction> const checkTask(*formulas[0]);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    storm::modelchecker::SparsePropositionalModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>> propositionalChecker(*dtmc);
    storm::storage::BitVector psiStates =
        propositionalChecker.check(checkTask.getFormula().asProbabilityOperatorFormula().getSubformula().asEventuallyFormula().getSubformula())
            ->template asExplicitQualitativeCheckResult<storm::RationalFunction>()
            .getTruthValuesVector();

    std::vector<storm::RationalFunction> target(model->getNumberOfStates(), storm::utility::zero<storm::RationalFunction>());
    storm::utility::vector::setVectorValues(target, psiStates, storm::utility::one<storm::RationalFunction>());

    storm::storage::BitVector allTrue(model->getNumberOfStates(), true);

    // Lift parameters for region [0,1]
    storm::transformer::RobustParameterLifter<storm::RationalFunction, double> parameterLifter(dtmc->getTransitionMatrix().filterEntries(~psiStates), target,
                                                                                               allTrue, allTrue);

    storm::storage::ParameterRegion<storm::RationalFunction> region = storm::api::createRegion<storm::RationalFunction>("0", *dtmc)[0];

    parameterLifter.specifyRegion(region, storm::solver::OptimizationDirection::Maximize);

    storm::transformer::IntervalEndComponentPreserver preserver;
    auto result = preserver.eliminateMECs(parameterLifter.getMatrix(), parameterLifter.getVector());
    ASSERT_TRUE(result.has_value());
    auto const& withoutMECs = *result;

    auto target2 = parameterLifter.getVector();
    target2.push_back(storm::utility::zero<storm::Interval>());

    auto env = storm::Environment();
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
    env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));

    auto factory = std::make_unique<storm::solver::GeneralMinMaxLinearEquationSolverFactory<storm::Interval, double>>();

    auto const& solver1 = factory->create(env);
    auto x1 = std::vector<double>(parameterLifter.getVector().size(), 0);
    solver1->setMatrix(parameterLifter.getMatrix());

    auto const& solver2 = factory->create(env);
    solver2->setMatrix(withoutMECs);
    auto x2 = std::vector<double>(target2.size(), 0);

    solver1->setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Cooperative);
    solver2->setUncertaintyResolutionMode(storm::UncertaintyResolutionMode::Cooperative);

    // Check that maximize is the same
    solver1->setOptimizationDirection(storm::solver::OptimizationDirection::Maximize);
    solver1->solveEquations(env, x1, parameterLifter.getVector());
    solver2->setOptimizationDirection(storm::solver::OptimizationDirection::Maximize);
    solver2->solveEquations(env, x2, target2);

    for (uint64_t i = 0; i < x1.size(); i++) {
        if (withoutMECs.getRow(i).getNumberOfEntries() > 0) {
            ASSERT_NEAR(x1[i], x2[i], 1e-7);
        }
    }

    // We can't check minimize because we don't know what is happening, and
    // also, minimizing solver1 is what we want to avoid
}

class IntervalEndComponentPreserverCheckTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(IntervalEndComponentPreserverCheckTest, Simple) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/only_p.pm";
    std::string formulaAsString = "P=? [F \"target\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5
    testModelInterval(programFile, formulaAsString, constantsAsString);
}

TEST_F(IntervalEndComponentPreserverCheckTest, BRP) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P=? [F \"error\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5
    testModelInterval(programFile, formulaAsString, constantsAsString);
}

TEST_F(IntervalEndComponentPreserverCheckTest, Crowds) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/crowds3_5.pm";
    std::string formulaAsString = "P=? [F \"observeIGreater1\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5
    testModelInterval(programFile, formulaAsString, constantsAsString);
}

TEST_F(IntervalEndComponentPreserverCheckTest, NAND) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/nand-5-2.pm";
    std::string formulaAsString = "P=? [F \"target\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5
    testModelInterval(programFile, formulaAsString, constantsAsString);
}
