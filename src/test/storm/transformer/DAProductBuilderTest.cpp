#include "storm-config.h"
#include "test/storm_gtest.h"

#include <memory>
#include <sstream>
#include <string>

#include "storm-parsers/parser/PrismParser.h"
#include "storm/automata/DeterministicAutomaton.h"
#include "storm/builder/ExplicitModelBuilder.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/MarkovAutomaton.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/storage/BitVector.h"
#include "storm/transformer/DAProductBuilder.h"

TEST(DAProductBuilderTest_aUb, Dtmc) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(program).build();
    auto dtmc = std::dynamic_pointer_cast<storm::models::sparse::Dtmc<double>>(model);

    std::string aUb =
        "HOA: v1\n"
        "States: 3\n"
        "Start: 0\n"
        "acc-name: Rabin 1\n"
        "Acceptance: 2 (Fin(0) & Inf(1))\n"
        "AP: 2 \"a\" \"b\""
        "--BODY--\n"
        "State: 0 \"a U b\" \n { 0 }\n"
        "  2  /* !a  & !b */\n"
        "  0  /*  a  & !b */\n"
        "  1  /* !a  &  b */\n"
        "  1  /*  a  &  b */\n"
        "State: 1 { 1 }\n"
        "  1 1 1 1       /* four transitions on one line */\n"
        "State: 2 \"sink state\" { 0 }\n"
        "  2 2 2 2\n"
        "--END--\n";

    std::istringstream in = std::istringstream(aUb);
    storm::automata::DeterministicAutomaton::ptr da;
    ASSERT_NO_THROW(da = storm::automata::DeterministicAutomaton::parse(in));

    std::vector<storm::storage::BitVector> apLabels;
    storm::storage::BitVector apA(dtmc->getNumberOfStates(), true);
    apA.set(2, false);
    storm::storage::BitVector apB(dtmc->getNumberOfStates(), false);
    apB.set(7);

    // std::cout << "apA: " << apA << "\n";
    // std::cout << "apB: " << apB << "\n";
    apLabels.push_back(apA);
    apLabels.push_back(apB);

    storm::transformer::DAProductBuilder productBuilder(*da, apLabels);
    auto product = productBuilder.build(*dtmc, dtmc->getInitialStates());

    // std::ofstream modelDot;
    // storm::io::openFile("model.dot", modelDot);
    // dtmc->writeDotToStream(modelDot);
    // storm::io::closeFile(modelDot);

    // std::ofstream productDot;
    // storm::io::openFile("product.dot", modelDot);
    // product->getProductModel().writeDotToStream(productDot);
    // storm::io::closeFile(productDot);

    // product->printMapping(std::cout);

    // for (unsigned int i = 0; i < product->getAcceptance()->getNumberOfAcceptanceSets(); i++) {
    //   std::cout << i << ": " << product->getAcceptance()->getAcceptanceSet(i) << "\n";
    // }

    storm::storage::StateBlock scc;
    scc.insert(7);
    ASSERT_EQ(product->getAcceptance()->isAccepting(scc), 1);
    scc.insert(8);
    ASSERT_EQ(product->getAcceptance()->isAccepting(scc), false);
    scc.insert(12);
    ASSERT_EQ(product->getAcceptance()->isAccepting(scc), false);
}

TEST(DAProductBuilderTest_aWb, Dtmc) {
#ifndef STORM_HAVE_Z3
    GTEST_SKIP() << "Z3 not available.";
#endif
    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");

    std::shared_ptr<storm::models::sparse::Model<double>> model = storm::builder::ExplicitModelBuilder<double>(program).build();
    auto dtmc = std::dynamic_pointer_cast<storm::models::sparse::Dtmc<double>>(model);

    std::string aUb =
        "HOA: v1\n"
        "States: 3\n"
        "Start: 0\n"
        "acc-name: Rabin 1\n"
        "Acceptance: 2 (Fin(0) & Inf(1))\n"
        "AP: 2 \"a\" \"b\""
        "--BODY--\n"
        "State: 0 \"a U b\" \n"
        "  2  /* !a  & !b */\n"
        "  0  /*  a  & !b */\n"
        "  1  /* !a  &  b */\n"
        "  1  /*  a  &  b */\n"
        "State: 1 { 1 }\n"
        "  1 1 1 1       /* four transitions on one line */\n"
        "State: 2 \"sink state\" { 0 }\n"
        "  2 2 2 2\n"
        "--END--\n";

    std::istringstream in = std::istringstream(aUb);
    storm::automata::DeterministicAutomaton::ptr da;
    ASSERT_NO_THROW(da = storm::automata::DeterministicAutomaton::parse(in));

    // NOTE: the following relies on the fact that the exploration order of the
    // states in the transformation from PRISM program to explicit model is fixed...
    // Would be better to add labels and get the corresponding states from those labels

    std::vector<storm::storage::BitVector> apLabels;
    storm::storage::BitVector apA(dtmc->getNumberOfStates(), true);
    apA.set(2, false);
    storm::storage::BitVector apB(dtmc->getNumberOfStates(), false);
    apB.set(7);

    apLabels.push_back(apA);
    apLabels.push_back(apB);

    storm::transformer::DAProductBuilder productBuilder(*da, apLabels);
    auto product = productBuilder.build(*dtmc, dtmc->getInitialStates());

    storm::storage::StateBlock scc;
    scc.insert(7);
    ASSERT_EQ(product->getAcceptance()->isAccepting(scc), true);
    scc.insert(8);
    ASSERT_EQ(product->getAcceptance()->isAccepting(scc), true);
    scc.insert(12);
    ASSERT_EQ(product->getAcceptance()->isAccepting(scc), false);
}
