#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-pars/utility/ModelInstantiator.h"
#include "storm-parsers/api/storm-parsers.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/api/storm.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/Model.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"

class ModelInstantiatorTest : public ::testing::Test {
   protected:
    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }
};

TEST_F(ModelInstantiatorTest, BrpProb) {
    carl::VariablePool::getInstance().clear();

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "P=? [F s=5 ]";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    ASSERT_TRUE(formulas.size() == 1);
    // Parametric model
    storm::generator::NextStateGeneratorOptions options(*formulas.front());
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::builder::ExplicitModelBuilder<storm::RationalFunction>(program, options).build()->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    storm::utility::ModelInstantiator<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::models::sparse::Dtmc<double>> modelInstantiator(*dtmc);
    EXPECT_FALSE(dtmc->hasRewardModel());

    {
        std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> valuation;
        storm::RationalFunctionVariable const& pL = carl::VariablePool::getInstance().findVariableWithName("pL");
        ASSERT_NE(pL, carl::Variable::NO_VARIABLE);
        storm::RationalFunctionVariable const& pK = carl::VariablePool::getInstance().findVariableWithName("pK");
        ASSERT_NE(pK, carl::Variable::NO_VARIABLE);
        valuation.insert(std::make_pair(pL, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.8)));
        valuation.insert(std::make_pair(pK, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.9)));

        storm::models::sparse::Dtmc<double> const& instantiated(modelInstantiator.instantiate(valuation));

        ASSERT_EQ(dtmc->getTransitionMatrix().getRowGroupIndices(), instantiated.getTransitionMatrix().getRowGroupIndices());
        for (std::size_t rowGroup = 0; rowGroup < dtmc->getTransitionMatrix().getRowGroupCount(); ++rowGroup) {
            for (std::size_t row = dtmc->getTransitionMatrix().getRowGroupIndices()[rowGroup];
                 row < dtmc->getTransitionMatrix().getRowGroupIndices()[rowGroup + 1]; ++row) {
                auto instantiatedEntry = instantiated.getTransitionMatrix().getRow(row).begin();
                for (auto const& paramEntry : dtmc->getTransitionMatrix().getRow(row)) {
                    EXPECT_EQ(paramEntry.getColumn(), instantiatedEntry->getColumn());
                    double evaluatedValue = carl::toDouble(paramEntry.getValue().evaluate(valuation));
                    EXPECT_EQ(evaluatedValue, instantiatedEntry->getValue());
                    ++instantiatedEntry;
                }
                EXPECT_EQ(instantiated.getTransitionMatrix().getRow(row).end(), instantiatedEntry);
            }
        }
        EXPECT_EQ(dtmc->getStateLabeling(), instantiated.getStateLabeling());
        EXPECT_EQ(dtmc->getOptionalChoiceLabeling(), instantiated.getOptionalChoiceLabeling());

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<double>> modelchecker(instantiated);
        std::unique_ptr<storm::modelchecker::CheckResult> chkResult = modelchecker.check(*formulas[0]);
        storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeChkResult = chkResult->asExplicitQuantitativeCheckResult<double>();
        EXPECT_NEAR(0.2989278941, quantitativeChkResult[*instantiated.getInitialStates().begin()],
                    storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
    }

    {
        std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> valuation;
        storm::RationalFunctionVariable const& pL = carl::VariablePool::getInstance().findVariableWithName("pL");
        ASSERT_NE(pL, carl::Variable::NO_VARIABLE);
        storm::RationalFunctionVariable const& pK = carl::VariablePool::getInstance().findVariableWithName("pK");
        ASSERT_NE(pK, carl::Variable::NO_VARIABLE);
        valuation.insert(std::make_pair(pL, storm::utility::one<storm::RationalFunctionCoefficient>()));
        valuation.insert(std::make_pair(pK, storm::utility::one<storm::RationalFunctionCoefficient>()));

        storm::models::sparse::Dtmc<double> const& instantiated(modelInstantiator.instantiate(valuation));

        ASSERT_EQ(dtmc->getTransitionMatrix().getRowGroupIndices(), instantiated.getTransitionMatrix().getRowGroupIndices());
        for (std::size_t rowGroup = 0; rowGroup < dtmc->getTransitionMatrix().getRowGroupCount(); ++rowGroup) {
            for (std::size_t row = dtmc->getTransitionMatrix().getRowGroupIndices()[rowGroup];
                 row < dtmc->getTransitionMatrix().getRowGroupIndices()[rowGroup + 1]; ++row) {
                auto instantiatedEntry = instantiated.getTransitionMatrix().getRow(row).begin();
                for (auto const& paramEntry : dtmc->getTransitionMatrix().getRow(row)) {
                    EXPECT_EQ(paramEntry.getColumn(), instantiatedEntry->getColumn());
                    double evaluatedValue = carl::toDouble(paramEntry.getValue().evaluate(valuation));
                    EXPECT_EQ(evaluatedValue, instantiatedEntry->getValue());
                    ++instantiatedEntry;
                }
                EXPECT_EQ(instantiated.getTransitionMatrix().getRow(row).end(), instantiatedEntry);
            }
        }
        EXPECT_EQ(dtmc->getStateLabeling(), instantiated.getStateLabeling());
        EXPECT_EQ(dtmc->getOptionalChoiceLabeling(), instantiated.getOptionalChoiceLabeling());

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<double>> modelchecker(instantiated);
        std::unique_ptr<storm::modelchecker::CheckResult> chkResult = modelchecker.check(*formulas[0]);
        storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeChkResult = chkResult->asExplicitQuantitativeCheckResult<double>();
        EXPECT_EQ(0.0, quantitativeChkResult[*instantiated.getInitialStates().begin()]);
    }

    {
        std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> valuation;
        storm::RationalFunctionVariable const& pL = carl::VariablePool::getInstance().findVariableWithName("pL");
        ASSERT_NE(pL, carl::Variable::NO_VARIABLE);
        storm::RationalFunctionVariable const& pK = carl::VariablePool::getInstance().findVariableWithName("pK");
        ASSERT_NE(pK, carl::Variable::NO_VARIABLE);
        valuation.insert(std::make_pair(pL, storm::utility::one<storm::RationalFunctionCoefficient>()));
        valuation.insert(std::make_pair(pK, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.9)));

        storm::models::sparse::Dtmc<double> const& instantiated(modelInstantiator.instantiate(valuation));

        ASSERT_EQ(dtmc->getTransitionMatrix().getRowGroupIndices(), instantiated.getTransitionMatrix().getRowGroupIndices());
        for (std::size_t rowGroup = 0; rowGroup < dtmc->getTransitionMatrix().getRowGroupCount(); ++rowGroup) {
            for (std::size_t row = dtmc->getTransitionMatrix().getRowGroupIndices()[rowGroup];
                 row < dtmc->getTransitionMatrix().getRowGroupIndices()[rowGroup + 1]; ++row) {
                auto instantiatedEntry = instantiated.getTransitionMatrix().getRow(row).begin();
                for (auto const& paramEntry : dtmc->getTransitionMatrix().getRow(row)) {
                    EXPECT_EQ(paramEntry.getColumn(), instantiatedEntry->getColumn());
                    double evaluatedValue = carl::toDouble(paramEntry.getValue().evaluate(valuation));
                    EXPECT_EQ(evaluatedValue, instantiatedEntry->getValue());
                    ++instantiatedEntry;
                }
                EXPECT_EQ(instantiated.getTransitionMatrix().getRow(row).end(), instantiatedEntry);
            }
        }
        EXPECT_EQ(dtmc->getStateLabeling(), instantiated.getStateLabeling());
        EXPECT_EQ(dtmc->getOptionalChoiceLabeling(), instantiated.getOptionalChoiceLabeling());

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<double>> modelchecker(instantiated);
        std::unique_ptr<storm::modelchecker::CheckResult> chkResult = modelchecker.check(*formulas[0]);
        storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeChkResult = chkResult->asExplicitQuantitativeCheckResult<double>();
        EXPECT_NEAR(0.01588055832, quantitativeChkResult[*instantiated.getInitialStates().begin()],
                    storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
    }
}

TEST_F(ModelInstantiatorTest, Brp_Rew) {
    carl::VariablePool::getInstance().clear();

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pdtmc/brp16_2.pm";
    std::string formulaAsString = "R=? [F ((s=5) | (s=0&srep=3)) ]";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    ASSERT_TRUE(formulas.size() == 1);
    // Parametric model
    storm::generator::NextStateGeneratorOptions options(*formulas.front());
    std::shared_ptr<storm::models::sparse::Dtmc<storm::RationalFunction>> dtmc =
        storm::builder::ExplicitModelBuilder<storm::RationalFunction>(program, options).build()->as<storm::models::sparse::Dtmc<storm::RationalFunction>>();

    storm::utility::ModelInstantiator<storm::models::sparse::Dtmc<storm::RationalFunction>, storm::models::sparse::Dtmc<double>> modelInstantiator(*dtmc);

    {
        std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> valuation;
        storm::RationalFunctionVariable const& pL = carl::VariablePool::getInstance().findVariableWithName("pL");
        ASSERT_NE(pL, carl::Variable::NO_VARIABLE);
        storm::RationalFunctionVariable const& pK = carl::VariablePool::getInstance().findVariableWithName("pK");
        ASSERT_NE(pK, carl::Variable::NO_VARIABLE);
        storm::RationalFunctionVariable const& TOMsg = carl::VariablePool::getInstance().findVariableWithName("TOMsg");
        ASSERT_NE(pK, carl::Variable::NO_VARIABLE);
        storm::RationalFunctionVariable const& TOAck = carl::VariablePool::getInstance().findVariableWithName("TOAck");
        ASSERT_NE(pK, carl::Variable::NO_VARIABLE);
        valuation.insert(std::make_pair(pL, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.9)));
        valuation.insert(std::make_pair(pK, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.3)));
        valuation.insert(std::make_pair(TOMsg, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.3)));
        valuation.insert(std::make_pair(TOAck, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.5)));

        storm::models::sparse::Dtmc<double> const& instantiated(modelInstantiator.instantiate(valuation));

        ASSERT_EQ(dtmc->getTransitionMatrix().getRowGroupIndices(), instantiated.getTransitionMatrix().getRowGroupIndices());
        for (std::size_t rowGroup = 0; rowGroup < dtmc->getTransitionMatrix().getRowGroupCount(); ++rowGroup) {
            for (std::size_t row = dtmc->getTransitionMatrix().getRowGroupIndices()[rowGroup];
                 row < dtmc->getTransitionMatrix().getRowGroupIndices()[rowGroup + 1]; ++row) {
                auto instantiatedEntry = instantiated.getTransitionMatrix().getRow(row).begin();
                for (auto const& paramEntry : dtmc->getTransitionMatrix().getRow(row)) {
                    EXPECT_EQ(paramEntry.getColumn(), instantiatedEntry->getColumn());
                    double evaluatedValue = carl::toDouble(paramEntry.getValue().evaluate(valuation));
                    EXPECT_EQ(evaluatedValue, instantiatedEntry->getValue());
                    ++instantiatedEntry;
                }
                EXPECT_EQ(instantiated.getTransitionMatrix().getRow(row).end(), instantiatedEntry);
            }
        }
        ASSERT_TRUE(instantiated.hasUniqueRewardModel());
        EXPECT_FALSE(instantiated.getUniqueRewardModel().hasStateRewards());
        EXPECT_FALSE(instantiated.getUniqueRewardModel().hasTransitionRewards());
        EXPECT_TRUE(instantiated.getUniqueRewardModel().hasStateActionRewards());
        ASSERT_TRUE(dtmc->getUniqueRewardModel().hasStateActionRewards());
        std::size_t stateActionEntries = dtmc->getUniqueRewardModel().getStateActionRewardVector().size();
        ASSERT_EQ(stateActionEntries, instantiated.getUniqueRewardModel().getStateActionRewardVector().size());
        for (std::size_t i = 0; i < stateActionEntries; ++i) {
            double evaluatedValue = carl::toDouble(dtmc->getUniqueRewardModel().getStateActionRewardVector()[i].evaluate(valuation));
            EXPECT_EQ(evaluatedValue, instantiated.getUniqueRewardModel().getStateActionRewardVector()[i]);
        }
        EXPECT_EQ(dtmc->getStateLabeling(), instantiated.getStateLabeling());
        EXPECT_EQ(dtmc->getOptionalChoiceLabeling(), instantiated.getOptionalChoiceLabeling());

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<double>> modelchecker(instantiated);
        std::unique_ptr<storm::modelchecker::CheckResult> chkResult = modelchecker.check(*formulas[0]);
        storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeChkResult = chkResult->asExplicitQuantitativeCheckResult<double>();
        EXPECT_NEAR(1.308324495, quantitativeChkResult[*instantiated.getInitialStates().begin()],
                    storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
    }
}

TEST_F(ModelInstantiatorTest, Consensus) {
    carl::VariablePool::getInstance().clear();

    std::string programFile = STORM_TEST_RESOURCES_DIR "/pmdp/coin2_2.nm";
    std::string formulaAsString = "Pmin=? [F \"finished\"&\"all_coins_equal_1\" ]";

    // Program and formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program.checkValidity();
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulaAsString, program));
    ASSERT_TRUE(formulas.size() == 1);
    // Parametric model
    storm::generator::NextStateGeneratorOptions options(*formulas.front());
    std::shared_ptr<storm::models::sparse::Mdp<storm::RationalFunction>> mdp =
        storm::builder::ExplicitModelBuilder<storm::RationalFunction>(program, options).build()->as<storm::models::sparse::Mdp<storm::RationalFunction>>();

    storm::utility::ModelInstantiator<storm::models::sparse::Mdp<storm::RationalFunction>, storm::models::sparse::Mdp<double>> modelInstantiator(*mdp);

    std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> valuation;
    storm::RationalFunctionVariable const& p1 = carl::VariablePool::getInstance().findVariableWithName("p1");
    ASSERT_NE(p1, carl::Variable::NO_VARIABLE);
    storm::RationalFunctionVariable const& p2 = carl::VariablePool::getInstance().findVariableWithName("p2");
    ASSERT_NE(p2, carl::Variable::NO_VARIABLE);
    valuation.insert(std::make_pair(p1, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.51)));
    valuation.insert(std::make_pair(p2, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.49)));
    storm::models::sparse::Mdp<double> const& instantiated(modelInstantiator.instantiate(valuation));

    ASSERT_EQ(mdp->getTransitionMatrix().getRowGroupIndices(), instantiated.getTransitionMatrix().getRowGroupIndices());
    for (std::size_t rowGroup = 0; rowGroup < mdp->getTransitionMatrix().getRowGroupCount(); ++rowGroup) {
        for (std::size_t row = mdp->getTransitionMatrix().getRowGroupIndices()[rowGroup]; row < mdp->getTransitionMatrix().getRowGroupIndices()[rowGroup + 1];
             ++row) {
            auto instantiatedEntry = instantiated.getTransitionMatrix().getRow(row).begin();
            for (auto const& paramEntry : mdp->getTransitionMatrix().getRow(row)) {
                EXPECT_EQ(paramEntry.getColumn(), instantiatedEntry->getColumn());
                double evaluatedValue = carl::toDouble(paramEntry.getValue().evaluate(valuation));
                EXPECT_EQ(evaluatedValue, instantiatedEntry->getValue());
                ++instantiatedEntry;
            }
            EXPECT_EQ(instantiated.getTransitionMatrix().getRow(row).end(), instantiatedEntry);
        }
    }
    EXPECT_EQ(mdp->getStateLabeling(), instantiated.getStateLabeling());
    EXPECT_EQ(mdp->getOptionalChoiceLabeling(), instantiated.getOptionalChoiceLabeling());

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<double>> modelchecker(instantiated);
    std::unique_ptr<storm::modelchecker::CheckResult> chkResult = modelchecker.check(*formulas[0]);
    storm::modelchecker::ExplicitQuantitativeCheckResult<double>& quantitativeChkResult = chkResult->asExplicitQuantitativeCheckResult<double>();
    EXPECT_NEAR(0.3526577219, quantitativeChkResult[*instantiated.getInitialStates().begin()],
                storm::settings::getModule<storm::settings::modules::GeneralSettings>().getPrecision());
}
