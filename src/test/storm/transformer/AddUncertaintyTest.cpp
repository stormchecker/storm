#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/adapters/RationalNumberForward.h"
#include "storm/api/storm.h"
#include "storm/transformer/AddUncertainty.h"

TEST(AddUncertaintyTransformerTest, BrpTest) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm");
    std::string formulasString = "P=? [ F \"target\"]";
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasString, program));
    auto model = storm::api::buildSparseModel<double>(program, formulas);

    auto transformer = storm::transformer::AddUncertainty(model);
    auto uncertainModel = transformer.transform(0.01);
    EXPECT_EQ(uncertainModel->getNumberOfStates(), model->getNumberOfStates());
    EXPECT_EQ(uncertainModel->getNumberOfTransitions(), model->getNumberOfTransitions());
    EXPECT_TRUE(uncertainModel->hasUncertainty());
}

TEST(AddUncertaintyTransformerTest, Coin22Test) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm");
    std::string formulasString = "Pmax=? [ F \"all_coins_equal_1\"]";
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasString, program));
    auto model = storm::api::buildSparseModel<double>(program, formulas);

    auto transformer = storm::transformer::AddUncertainty(model);
    auto uncertainModel = transformer.transform(0.01);
    EXPECT_EQ(uncertainModel->getNumberOfStates(), model->getNumberOfStates());
    EXPECT_EQ(uncertainModel->getNumberOfTransitions(), model->getNumberOfTransitions());
    EXPECT_TRUE(uncertainModel->hasUncertainty());
}

TEST(AddUncertaintyTransformerTest, BrpTestRational) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm");
    std::string formulasString = "P=? [ F \"target\"]";
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasString, program));
    auto model = storm::api::buildSparseModel<storm::RationalNumber>(program, formulas);

    auto transformer = storm::transformer::AddUncertainty<storm::RationalNumber>(model);
    auto uncertainModel = transformer.transform(storm::RationalNumber("1/100"));
    EXPECT_EQ(uncertainModel->getNumberOfStates(), model->getNumberOfStates());
    EXPECT_EQ(uncertainModel->getNumberOfTransitions(), model->getNumberOfTransitions());
    EXPECT_TRUE(uncertainModel->hasUncertainty());
}

TEST(AddUncertaintyTransformerTest, Coin22TestRational) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2.nm");
    std::string formulasString = "Pmax=? [ F \"all_coins_equal_1\"]";
    auto formulas = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasString, program));
    auto model = storm::api::buildSparseModel<storm::RationalNumber>(program, formulas);

    auto transformer = storm::transformer::AddUncertainty<storm::RationalNumber>(model);
    auto uncertainModel = transformer.transform(storm::RationalNumber("1/100"));
    EXPECT_EQ(uncertainModel->getNumberOfStates(), model->getNumberOfStates());
    EXPECT_EQ(uncertainModel->getNumberOfTransitions(), model->getNumberOfTransitions());
    EXPECT_TRUE(uncertainModel->hasUncertainty());
}
