#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-pars/scp/AffinePMdpDecomposition.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/SparseMatrix.h"

using namespace storm::pars::scp;

namespace {

storm::RationalFunction makeVarFunction(carl::Variable const& var, std::shared_ptr<storm::RawPolynomialCache> const& cache) {
    return storm::RationalFunction(storm::Polynomial(storm::RawPolynomial(var), cache));
}

storm::models::sparse::StateLabeling trivialLabeling(uint64_t numStates) {
    storm::models::sparse::StateLabeling labeling(numStates);
    labeling.addLabel("init");
    labeling.addLabelToState("init", 0);
    return labeling;
}

}  // namespace

TEST(AffinePMdpDecompositionTest, AffineDtmc) {
    carl::VariablePool::getInstance().clear();
    auto cache = std::make_shared<storm::RawPolynomialCache>();
    carl::Variable varP = storm::createRFVariable("p");
    auto p = makeVarFunction(varP, cache);
    auto one = storm::RationalFunction(1);

    // 3-state DTMC: 0 -[p]-> 1, 0 -[1-p]-> 2, 1 -[1]-> 1, 2 -[1]-> 2.
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> builder(3, 3, 4);
    builder.addNextValue(0, 1, p);
    builder.addNextValue(0, 2, one - p);
    builder.addNextValue(1, 1, one);
    builder.addNextValue(2, 2, one);
    auto matrix = builder.build();

    storm::models::sparse::Dtmc<storm::RationalFunction> dtmc(matrix, trivialLabeling(3));

    AffinePMdpDecomposition decomposition(dtmc);
    EXPECT_EQ(3u, decomposition.getNumberOfRows());
    ASSERT_EQ(1u, decomposition.getParameters().size());
    EXPECT_EQ(varP, *decomposition.getParameters().begin());

    auto const& row0 = decomposition.getRow(0);
    ASSERT_EQ(2u, row0.size());
    EXPECT_EQ(1u, row0[0].successor);
    EXPECT_DOUBLE_EQ(0.0, row0[0].constant);
    ASSERT_EQ(1u, row0[0].parameterCoefficients.size());
    EXPECT_EQ(varP, row0[0].parameterCoefficients[0].first);
    EXPECT_DOUBLE_EQ(1.0, row0[0].parameterCoefficients[0].second);

    EXPECT_EQ(2u, row0[1].successor);
    EXPECT_DOUBLE_EQ(1.0, row0[1].constant);
    ASSERT_EQ(1u, row0[1].parameterCoefficients.size());
    EXPECT_EQ(varP, row0[1].parameterCoefficients[0].first);
    EXPECT_DOUBLE_EQ(-1.0, row0[1].parameterCoefficients[0].second);

    auto const& row1 = decomposition.getRow(1);
    ASSERT_EQ(1u, row1.size());
    EXPECT_EQ(1u, row1[0].successor);
    EXPECT_DOUBLE_EQ(1.0, row1[0].constant);
    EXPECT_TRUE(row1[0].parameterCoefficients.empty());
}

TEST(AffinePMdpDecompositionTest, TransitionWithTwoParameters) {
    carl::VariablePool::getInstance().clear();
    auto cache = std::make_shared<storm::RawPolynomialCache>();
    carl::Variable varP = storm::createRFVariable("p");
    carl::Variable varQ = storm::createRFVariable("q");
    auto p = makeVarFunction(varP, cache);
    auto q = makeVarFunction(varQ, cache);
    auto one = storm::RationalFunction(1);

    // 0 -[p]-> 1, 0 -[q]-> 2, 0 -[1-p-q]-> 3; 1,2,3 self-absorbing.
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> builder(4, 4, 6);
    builder.addNextValue(0, 1, p);
    builder.addNextValue(0, 2, q);
    builder.addNextValue(0, 3, one - p - q);
    builder.addNextValue(1, 1, one);
    builder.addNextValue(2, 2, one);
    builder.addNextValue(3, 3, one);
    auto matrix = builder.build();

    storm::models::sparse::Dtmc<storm::RationalFunction> dtmc(matrix, trivialLabeling(4));

    AffinePMdpDecomposition decomposition(dtmc);
    EXPECT_EQ(2u, decomposition.getParameters().size());

    auto const& row0 = decomposition.getRow(0);
    ASSERT_EQ(3u, row0.size());

    // Transition to 3 depends on both parameters, each with coefficient -1.
    auto const& toThree = row0[2];
    EXPECT_EQ(3u, toThree.successor);
    EXPECT_DOUBLE_EQ(1.0, toThree.constant);
    ASSERT_EQ(2u, toThree.parameterCoefficients.size());
    for (auto const& [var, coeff] : toThree.parameterCoefficients) {
        EXPECT_TRUE(var == varP || var == varQ);
        EXPECT_DOUBLE_EQ(-1.0, coeff);
    }
}

TEST(AffinePMdpDecompositionTest, MdpWithTwoChoicesPerState) {
    carl::VariablePool::getInstance().clear();
    auto cache = std::make_shared<storm::RawPolynomialCache>();
    carl::Variable varP = storm::createRFVariable("p");
    carl::Variable varQ = storm::createRFVariable("q");
    auto p = makeVarFunction(varP, cache);
    auto q = makeVarFunction(varQ, cache);
    auto one = storm::RationalFunction(1);

    // State 0 has two choices: action 0 goes to 1 w.p. p / to 2 w.p. 1-p; action 1 goes to 1 w.p. q / to 2 w.p. 1-q.
    // States 1, 2 are self-absorbing with a single choice each.
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> builder(4, 3, 6, true, true, 3);
    builder.newRowGroup(0);
    builder.addNextValue(0, 1, p);
    builder.addNextValue(0, 2, one - p);
    builder.addNextValue(1, 1, q);
    builder.addNextValue(1, 2, one - q);
    builder.newRowGroup(2);
    builder.addNextValue(2, 1, one);
    builder.newRowGroup(3);
    builder.addNextValue(3, 2, one);
    auto matrix = builder.build();

    storm::models::sparse::Mdp<storm::RationalFunction> mdp(matrix, trivialLabeling(3));

    AffinePMdpDecomposition decomposition(mdp);
    EXPECT_EQ(4u, decomposition.getNumberOfRows());
    EXPECT_EQ(2u, decomposition.getParameters().size());

    auto const& choice0 = decomposition.getRow(0);
    ASSERT_EQ(2u, choice0.size());
    EXPECT_EQ(varP, choice0[0].parameterCoefficients.at(0).first);

    auto const& choice1 = decomposition.getRow(1);
    ASSERT_EQ(2u, choice1.size());
    EXPECT_EQ(varQ, choice1[0].parameterCoefficients.at(0).first);
}

TEST(AffinePMdpDecompositionTest, NonAffineTransitionThrows) {
    carl::VariablePool::getInstance().clear();
    auto cache = std::make_shared<storm::RawPolynomialCache>();
    carl::Variable varP = storm::createRFVariable("p");
    carl::Variable varQ = storm::createRFVariable("q");
    auto p = makeVarFunction(varP, cache);
    auto q = makeVarFunction(varQ, cache);
    auto one = storm::RationalFunction(1);

    // 0 -[p*q]-> 1, 0 -[1-p*q]-> 2: the p*q transition has total degree 2, violating the affine precondition.
    storm::storage::SparseMatrixBuilder<storm::RationalFunction> builder(3, 3, 4);
    builder.addNextValue(0, 1, p * q);
    builder.addNextValue(0, 2, one - p * q);
    builder.addNextValue(1, 1, one);
    builder.addNextValue(2, 2, one);
    auto matrix = builder.build();

    storm::models::sparse::Dtmc<storm::RationalFunction> dtmc(matrix, trivialLabeling(3));

    STORM_SILENT_EXPECT_THROW(AffinePMdpDecomposition decomposition(dtmc), storm::exceptions::NotSupportedException);
}

// The reward model's state-action reward at row 0 is "2*c + 1" (parameter c, which occurs nowhere
// in the transition matrix) -- exercises both the reward decomposition itself and that a
// reward-only parameter (not occurring in any transition) is still picked up by getParameters(),
// which SparseScpInstantiationSearcher relies on for region-coverage validation.
TEST(AffinePMdpDecompositionTest, RewardDecomposition_PicksUpRewardOnlyParameter) {
    carl::VariablePool::getInstance().clear();
    auto cache = std::make_shared<storm::RawPolynomialCache>();
    carl::Variable varP = storm::createRFVariable("p");
    carl::Variable varC = storm::createRFVariable("c");
    auto p = makeVarFunction(varP, cache);
    auto c = makeVarFunction(varC, cache);
    auto one = storm::RationalFunction(1);
    auto two = storm::RationalFunction(2);

    storm::storage::SparseMatrixBuilder<storm::RationalFunction> builder(3, 3, 4);
    builder.addNextValue(0, 1, p);
    builder.addNextValue(0, 2, one - p);
    builder.addNextValue(1, 1, one);
    builder.addNextValue(2, 2, one);
    auto matrix = builder.build();
    storm::models::sparse::Dtmc<storm::RationalFunction> dtmc(matrix, trivialLabeling(3));

    std::vector<storm::RationalFunction> stateActionRewards{two * c + one, storm::utility::zero<storm::RationalFunction>(),
                                                            storm::utility::zero<storm::RationalFunction>()};
    storm::models::sparse::StandardRewardModel<storm::RationalFunction> rewardModel(std::nullopt, stateActionRewards);

    AffinePMdpDecomposition decomposition(dtmc, &rewardModel);
    ASSERT_EQ(2u, decomposition.getParameters().size());
    EXPECT_EQ(1u, decomposition.getParameters().count(varC));
    EXPECT_EQ(1u, decomposition.getParameters().count(varP));

    auto const& reward0 = decomposition.getReward(0);
    EXPECT_DOUBLE_EQ(1.0, reward0.constant);
    ASSERT_EQ(1u, reward0.parameterCoefficients.size());
    EXPECT_EQ(varC, reward0.parameterCoefficients[0].first);
    EXPECT_DOUBLE_EQ(2.0, reward0.parameterCoefficients[0].second);

    auto const& reward1 = decomposition.getReward(1);
    EXPECT_DOUBLE_EQ(0.0, reward1.constant);
    EXPECT_TRUE(reward1.parameterCoefficients.empty());
}

// The reward at row 0 is "p*p", degree 2 -- violates the affine precondition just as a non-affine
// transition would, and must be rejected the same way (not silently mishandled, and not only
// checked for the transition matrix while letting a non-affine reward slip through).
TEST(AffinePMdpDecompositionTest, NonAffineRewardThrows) {
    carl::VariablePool::getInstance().clear();
    auto cache = std::make_shared<storm::RawPolynomialCache>();
    carl::Variable varP = storm::createRFVariable("p");
    auto p = makeVarFunction(varP, cache);
    auto one = storm::RationalFunction(1);

    storm::storage::SparseMatrixBuilder<storm::RationalFunction> builder(3, 3, 4);
    builder.addNextValue(0, 1, p);
    builder.addNextValue(0, 2, one - p);
    builder.addNextValue(1, 1, one);
    builder.addNextValue(2, 2, one);
    auto matrix = builder.build();
    storm::models::sparse::Dtmc<storm::RationalFunction> dtmc(matrix, trivialLabeling(3));

    std::vector<storm::RationalFunction> stateActionRewards{p * p, storm::utility::zero<storm::RationalFunction>(),
                                                            storm::utility::zero<storm::RationalFunction>()};
    storm::models::sparse::StandardRewardModel<storm::RationalFunction> rewardModel(std::nullopt, stateActionRewards);

    STORM_SILENT_EXPECT_THROW(AffinePMdpDecomposition decomposition(dtmc, &rewardModel), storm::exceptions::NotSupportedException);
}
