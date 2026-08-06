#include "storm-config.h"
#include "test/storm_gtest.h"

#include <memory>
#include <string>

#include "storm-pars/api/region.h"
#include "storm-pars/modelchecker/instantiation/SparseInstantiationModelChecker.h"
#include "storm-pars/transformer/BigStep.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/bisimulation.h"
#include "storm/api/builder.h"
#include "storm/environment/Environment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Model.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/bisimulation/BisimulationType.h"
#include "storm/storage/prism/Program.h"
#include "storm/utility/constants.h"

void testModel(std::string programFile, std::string formulaAsString, std::string constantsAsString) {
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalFunction> const checkTask(*formulas[0]);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    dtmc = storm::api::performBisimulationMinimization<storm::RationalFunction>(dtmc, formulas, storm::storage::BisimulationType::Strong)
               ->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    storm::transformer::BigStep BigStep;
    auto timeTravelledDtmc = BigStep.bigStep(*dtmc, checkTask).first;

    storm::modelchecker::SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::RationalNumber> modelChecker(*dtmc);
    modelChecker.specifyFormula(checkTask);
    storm::modelchecker::SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::RationalNumber> modelCheckerTT(
        timeTravelledDtmc);
    modelCheckerTT.specifyFormula(checkTask);

    auto parameters = storm::models::sparse::getAllParameters(*dtmc);

    // Check if both DTMCs are equivalent just by sampling.
    std::vector<std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient>> testInstantiations;
    std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> emptyInstantiation;
    testInstantiations.push_back(emptyInstantiation);
    for (auto const& param : parameters) {
        std::vector<std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient>> newInstantiations;
        for (auto point : testInstantiations) {
            for (storm::RationalNumber x = storm::utility::convertNumber<storm::RationalNumber>(1e-5); x <= 1;
                 x += (1 - storm::utility::convertNumber<storm::RationalNumber>(1e-5)) / 10) {
                std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> newMap(point);
                newMap[param] = storm::utility::convertNumber<storm::RationalFunctionCoefficient>(x);
                newInstantiations.push_back(newMap);
            }
        }
        testInstantiations = newInstantiations;
    }

    storm::Environment env;
    storm::Environment envRobust;
    env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
    envRobust.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
    for (auto const& instantiation : testInstantiations) {
        auto result = modelChecker.check(env, instantiation)->asExplicitQuantitativeCheckResult<storm::RationalNumber>();
        auto resultTT = modelCheckerTT.check(env, instantiation)->asExplicitQuantitativeCheckResult<storm::RationalNumber>();

        storm::RationalNumber resA = result[*modelChecker.getOriginalModel().getInitialStates().begin()];
        storm::RationalNumber resB = resultTT[*modelCheckerTT.getOriginalModel().getInitialStates().begin()];
        ASSERT_NEAR(resA, resB, storm::utility::convertNumber<storm::RationalNumber>(1e-6));
    }

    auto region = storm::api::createRegion<storm::RationalFunction>("0.4", *dtmc);

    auto pla =
        storm::api::initializeRegionModelChecker<storm::RationalFunction>(env, dtmc, checkTask, storm::modelchecker::RegionCheckEngine::ParameterLifting);
    auto resultPLA = pla->getBoundAtInitState(env, region[0], storm::OptimizationDirection::Minimize);

    auto sharedDtmc = std::make_shared<storm::models::sparse::Dtmc<storm::RationalFunction>>(timeTravelledDtmc);
    auto plaTT = storm::api::initializeRegionModelChecker<storm::RationalFunction>(env, sharedDtmc, checkTask,
                                                                                   storm::modelchecker::RegionCheckEngine::RobustParameterLifting);
    auto resultPLATT = plaTT->getBoundAtInitState(env, region[0], storm::OptimizationDirection::Minimize);

    ASSERT_TRUE(resultPLA < resultPLATT) << "Time-Travelling did not make bound better";
}

class BigStep : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(BigStep, Crowds) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/crowds3_5.pm";
    std::string formulaAsString = "P=? [F \"observeIGreater1\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5
    testModel(programFile, formulaAsString, constantsAsString);
}
TEST_F(BigStep, Nand) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/nand-5-2.pm";
    std::string formulaAsString = "P=? [F \"target\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5
    testModel(programFile, formulaAsString, constantsAsString);
}
TEST_F(BigStep, Herman) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/herman5_pla.pm";
    std::string formulaAsString = "R=? [F \"stable\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5
    testModel(programFile, formulaAsString, constantsAsString);
}
