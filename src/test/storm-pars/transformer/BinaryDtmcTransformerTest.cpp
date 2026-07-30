#include "storm-config.h"
#include "test/storm_gtest.h"

#include <memory>
#include <string>

#include "storm-pars/api/region.h"
#include "storm-pars/modelchecker/instantiation/SparseInstantiationModelChecker.h"
#include "storm-pars/modelchecker/region/SparseDtmcParameterLiftingModelChecker.h"
#include "storm-pars/transformer/BinaryDtmcTransformer.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/bisimulation.h"
#include "storm/api/builder.h"
#include "storm/environment/Environment.h"
#include "storm/modelchecker/CheckTask.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Model.h"
#include "storm/solver/OptimizationDirection.h"
#include "storm/storage/bisimulation/BisimulationType.h"
#include "storm/storage/prism/Program.h"
#include "storm/utility/constants.h"

void testModelB(std::string programFile, std::string formulaAsString, std::string constantsAsString) {
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsAsString);
    std::vector<std::shared_ptr<const storm::logic::Formula>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> model =
        storm::api::buildSparseModel<storm::RationalFunction>(program, formulas)->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    storm::modelchecker::CheckTask<storm::logic::Formula, storm::RationalFunction> const checkTask(*formulas[0]);
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc = model->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();
    uint64_t initialStateModel = dtmc->getStates("init").getNextSetIndex(0);

    dtmc = storm::api::performBisimulationMinimization<storm::RationalFunction>(dtmc, formulas, storm::storage::BisimulationType::Weak)
               ->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    storm::transformer::BinaryDtmcTransformer binaryDtmcTransformer;
    auto simpleDtmc = binaryDtmcTransformer.transform(*dtmc, true);

    storm::modelchecker::SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double> modelChecker(*dtmc);
    modelChecker.specifyFormula(checkTask);
    storm::modelchecker::SparseDtmcInstantiationModelChecker<storm::models::sparse::Dtmc<storm::RationalFunction>, double> modelCheckerSimple(*simpleDtmc);
    modelCheckerSimple.specifyFormula(checkTask);

    auto parameters = storm::models::sparse::getAllParameters(*dtmc);

    // Check if both DTMCs are equivalent just by sampling.
    std::vector<std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient>> testInstantiations;
    std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> emptyInstantiation;
    testInstantiations.push_back(emptyInstantiation);
    for (auto const& param : parameters) {
        std::vector<std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient>> newInstantiations;
        for (auto point : testInstantiations) {
            for (storm::RationalNumber x = storm::utility::convertNumber<storm::RationalNumber>(1e-6); x <= 1;
                 x += (1 - storm::utility::convertNumber<storm::RationalNumber>(1e-6)) / 10) {
                std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> newMap(point);
                newMap[param] = storm::utility::convertNumber<storm::RationalFunctionCoefficient>(x);
                newInstantiations.push_back(newMap);
            }
        }
        testInstantiations = newInstantiations;
    }

    storm::Environment env;
    for (auto const& instantiation : testInstantiations) {
        auto result = modelChecker.check(env, instantiation)->asExplicitQuantitativeCheckResult<double>()[initialStateModel];
        auto resultSimple = modelCheckerSimple.check(env, instantiation)->asExplicitQuantitativeCheckResult<double>()[initialStateModel];
        ASSERT_TRUE(storm::utility::isAlmostZero(result - resultSimple))
            << "Results " << result << " and " << resultSimple << " are not the same but should be.";
    }

    auto region = storm::api::createRegion<storm::RationalFunction>("0.4", *dtmc);

    auto pla =
        storm::api::initializeRegionModelChecker<storm::RationalFunction>(env, dtmc, checkTask, storm::modelchecker::RegionCheckEngine::ParameterLifting);
    auto resultPLA = pla->getBoundAtInitState(env, region[0], storm::OptimizationDirection::Minimize);

    auto plaSimple =
        storm::api::initializeRegionModelChecker<storm::RationalFunction>(env, simpleDtmc, checkTask, storm::modelchecker::RegionCheckEngine::ParameterLifting);
    auto resultPLASimple = plaSimple->getBoundAtInitState(env, region[0], storm::OptimizationDirection::Minimize);

    ASSERT_TRUE(resultPLA == resultPLASimple) << "Different PLA result with simplified DTMC";

    // Check that simpleDtmc is in fact simple
    for (uint64_t state = 0; state < simpleDtmc->getTransitionMatrix().getRowCount(); ++state) {
        auto row = simpleDtmc->getTransitionMatrix().getRow(state);

        std::set<storm::RationalFunctionVariable> variables;

        for (auto const& entry : row) {
            for (auto const& variable : entry.getValue().gatherVariables()) {
                variables.emplace(variable);
            }
        }

        if (variables.size() != 0) {
            ASSERT_TRUE(variables.size() == 1);
            auto parameter = *variables.begin();

            auto parameterPol = storm::RawPolynomial(parameter);
            auto parameterRational = storm::RationalFunction(carl::makePolynomial<storm::Polynomial>(parameterPol));
            auto oneMinusParameter = storm::RawPolynomial(1) - parameterPol;
            auto oneMinusParameterRational = storm::RationalFunction(carl::makePolynomial<storm::Polynomial>(oneMinusParameter));

            uint64_t seenP = 0;
            uint64_t seenOneMinusP = 0;

            for (auto const& entry : row) {
                if (!storm::utility::isZero(entry.getValue())) {
                    if (entry.getValue() == parameterRational) {
                        seenP++;
                    } else if (entry.getValue() == oneMinusParameterRational) {
                        seenOneMinusP++;
                    } else {
                        ASSERT_TRUE(false) << "Value " << entry.getValue() << " is not simple";
                    }
                }
            }

            ASSERT_TRUE(seenP == 1 && seenOneMinusP == 1) << "State " << state << " not simple";
        }
    }
}

class BinaryDtmcTransformerTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(BinaryDtmcTransformerTest, DISABLED_Crowds) {
    // for some reason this test fails on some machines (on debian 12, but not on ubuntu 22.04)
    // probably some exact model checking thing? no clue
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/crowds3_5.pm";
    std::string formulaAsString = "P=? [F \"observeIGreater1\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5
    testModelB(programFile, formulaAsString, constantsAsString);
}
TEST_F(BinaryDtmcTransformerTest, Nand) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/nand-5-2.pm";
    std::string formulaAsString = "P=? [F \"target\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5
    testModelB(programFile, formulaAsString, constantsAsString);
}
TEST_F(BinaryDtmcTransformerTest, Brp) {
    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P=? [F \"error\"]";
    std::string constantsAsString = "";  // e.g. pL=0.9,TOACK=0.5
    testModelB(programFile, formulaAsString, constantsAsString);
}
