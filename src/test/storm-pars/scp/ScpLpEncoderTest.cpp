#include "storm-config.h"
#include "test/storm-pars/scp/LpSolverTestUtils.h"
#include "test/storm_gtest.h"

#include <algorithm>

#include "storm-pars/scp/AffinePMdpDecomposition.h"
#include "storm-pars/scp/ScpLpEncoder.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/models/sparse/StateLabeling.h"
#include "storm/storage/BitVector.h"
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

std::vector<uint64_t> toU64(std::vector<uint_fast64_t> const& v) {
    return std::vector<uint64_t>(v.begin(), v.end());
}

}  // namespace

class ScpLpEncoderTest : public storm::pars::scp::testutils::LpSolverTest {};

// 3-state DTMC: 0 -[p]-> 1 (Prob1), 0 -[1-p]-> 2 (Prob0). Exercises a real LP optimization (not a
// degenerate single-point trust region) together with Prob0/Prob1 handling, the threshold
// constraint, and the max-violation objective mode.
//
// Hand-derived optimum: AtLeast maximizes p_0 subject to "p_0 <= v_p + tau_0" (the Pmin-pairing;
// see ScpLpEncoder's class-level doc comment). To let p_0 grow without paying violation, v_p must
// grow to support it, so both are pushed to their trust-region *upper* bound (1.0), where
// tau_0 = 0 is achievable (0.6 - 1.0 < 0) and p_0's bias term is maximal. Both are unique optima,
// not just feasible points: any smaller v_p would cap p_0 below 1.0, which the bias strictly
// disprefers.
TEST_P(ScpLpEncoderTest, RealOptimization_AtLeast_MaxViolationObjective) {
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
    AffinePMdpDecomposition decomposition(dtmc);

    storm::storage::BitVector zeroStates(3), oneStates(3), infiniteStates(3);
    zeroStates.set(2);
    oneStates.set(1);
    storm::storage::BitVector excludedRows(matrix.getRowCount(), false);

    std::map<carl::Variable, std::pair<double, double>> bounds{{varP, {0.0, 1.0}}};
    ScpOptions options;
    options.useMaxViolationObjective = true;

    ScpLpEncoder encoder(decomposition, toU64(matrix.getRowGroupIndices()), zeroStates, oneStates, infiniteStates, excludedRows, /*initialState=*/0, bounds,
                         /*threshold=*/0.6, ThresholdDirection::AtLeast, PropertyKind::Probability, options);

    std::map<carl::Variable, double> valuation{{varP, 0.5}};
    std::vector<double> stateValues{0.5, 0.0, 0.0};  // only index 0 (a free state) is ever read.

    auto lp = encoder.encode(env(), valuation, stateValues, /*trustRegionRadius=*/2.0);
    lp.solver->optimize();
    ASSERT_TRUE(lp.solver->isOptimal());

    EXPECT_NEAR(1.0, lp.solver->getContinuousValue(lp.stateVariables[0]), 1e-6);
    EXPECT_NEAR(1.0, lp.solver->getContinuousValue(lp.parameterVariables.at(varP)), 1e-6);
    EXPECT_NEAR(-1e-4, lp.solver->getObjectiveValue(), 1e-8);
}

// Same model as above, but AtMost direction and the sum-of-violations objective mode. Hand
// derivation gives a unique optimum at v_p = p_0 = 0.25 (both trust-region lower bounds), zero
// violation, objective = 1e-4 * 0.25.
TEST_P(ScpLpEncoderTest, RealOptimization_AtMost_SumViolationObjective) {
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
    AffinePMdpDecomposition decomposition(dtmc);

    storm::storage::BitVector zeroStates(3), oneStates(3), infiniteStates(3);
    zeroStates.set(2);
    oneStates.set(1);
    storm::storage::BitVector excludedRows(matrix.getRowCount(), false);

    std::map<carl::Variable, std::pair<double, double>> bounds{{varP, {0.0, 1.0}}};
    ScpOptions options;
    options.useMaxViolationObjective = false;

    ScpLpEncoder encoder(decomposition, toU64(matrix.getRowGroupIndices()), zeroStates, oneStates, infiniteStates, excludedRows, /*initialState=*/0, bounds,
                         /*threshold=*/0.3, ThresholdDirection::AtMost, PropertyKind::Probability, options);

    std::map<carl::Variable, double> valuation{{varP, 0.5}};
    std::vector<double> stateValues{0.5, 0.0, 0.0};

    auto lp = encoder.encode(env(), valuation, stateValues, /*trustRegionRadius=*/2.0);
    lp.solver->optimize();
    ASSERT_TRUE(lp.solver->isOptimal());

    EXPECT_NEAR(0.25, lp.solver->getContinuousValue(lp.stateVariables[0]), 1e-6);
    EXPECT_NEAR(0.25, lp.solver->getContinuousValue(lp.parameterVariables.at(varP)), 1e-6);
    EXPECT_NEAR(2.5e-5, lp.solver->getObjectiveValue(), 1e-8);
}

// 4-state DTMC: 0 -[p]-> 1 (free), 0 -[q]-> 3 (Prob1), 0 -[1-p-q]-> 2 (Prob0); 1 -[1]-> 1.
// Uses a trust region radius a hair above 1.0 (exactly 1.0 collapses variable bounds to lb==ub,
// which some LP backends -- e.g. Glpk's simplex -- reject as an invalid double-bounded variable
// rather than treating as fixed), which pins every variable to within ~1e-6 of its linearization
// value -- turning the LP into essentially "evaluate the linearized constraints at their own
// center", with no meaningful optimization freedom. This directly checks the bilinear
// Taylor-expansion formula (for the p*state1 term) and the exact/no-approximation handling of the
// Prob1 successor (the q term) against arithmetic computed independently in the test, without
// needing to solve a nontrivial LP by hand.
TEST_P(ScpLpEncoderTest, DegenerateTrustRegion_ChecksLinearizationArithmetic) {
    carl::VariablePool::getInstance().clear();
    auto cache = std::make_shared<storm::RawPolynomialCache>();
    carl::Variable varP = storm::createRFVariable("p");
    carl::Variable varQ = storm::createRFVariable("q");
    auto p = makeVarFunction(varP, cache);
    auto q = makeVarFunction(varQ, cache);
    auto one = storm::RationalFunction(1);

    storm::storage::SparseMatrixBuilder<storm::RationalFunction> builder(4, 4, 6);
    builder.addNextValue(0, 1, p);
    builder.addNextValue(0, 3, q);
    builder.addNextValue(0, 2, one - p - q);
    builder.addNextValue(1, 1, one);
    builder.addNextValue(2, 2, one);
    builder.addNextValue(3, 3, one);
    auto matrix = builder.build();
    storm::models::sparse::Dtmc<storm::RationalFunction> dtmc(matrix, trivialLabeling(4));
    AffinePMdpDecomposition decomposition(dtmc);

    storm::storage::BitVector zeroStates(4), oneStates(4), infiniteStates(4);
    zeroStates.set(2);
    oneStates.set(3);
    storm::storage::BitVector excludedRows(matrix.getRowCount(), false);

    std::map<carl::Variable, std::pair<double, double>> bounds{{varP, {0.0, 1.0}}, {varQ, {0.0, 1.0}}};
    ScpOptions options;
    options.useMaxViolationObjective = true;

    // threshold chosen low enough that it never binds tighter than the Bellman-derived slack.
    ScpLpEncoder encoder(decomposition, toU64(matrix.getRowGroupIndices()), zeroStates, oneStates, infiniteStates, excludedRows, /*initialState=*/0, bounds,
                         /*threshold=*/0.05, ThresholdDirection::AtLeast, PropertyKind::Probability, options);

    double const pHat = 0.3, qHat = 0.2, p0Hat = 0.1, p1Hat = 0.7;
    std::map<carl::Variable, double> valuation{{varP, pHat}, {varQ, qHat}};
    std::vector<double> stateValues{p0Hat, p1Hat, 0.0, 0.0};

    auto lp = encoder.encode(env(), valuation, stateValues, /*trustRegionRadius=*/1.0 + 1e-6);
    lp.solver->optimize();
    ASSERT_TRUE(lp.solver->isOptimal());

    // Every free/parameter variable is pinned to within ~1e-6 (relative) of its hat value by the
    // near-degenerate trust region.
    EXPECT_NEAR(pHat, lp.solver->getContinuousValue(lp.parameterVariables.at(varP)), 1e-5);
    EXPECT_NEAR(qHat, lp.solver->getContinuousValue(lp.parameterVariables.at(varQ)), 1e-5);
    EXPECT_NEAR(p0Hat, lp.solver->getContinuousValue(lp.stateVariables[0]), 1e-5);
    EXPECT_NEAR(p1Hat, lp.solver->getContinuousValue(lp.stateVariables[1]), 1e-5);

    // RHS = pHat*p1Hat (linearized bilinear term, exact at its own center) + qHat (exact Prob1 term).
    // AtLeast constrains p_0 <= RHS + tau_0 (the Pmin-pairing), so tau_0 >= p_0 - RHS.
    double expectedRhs = pHat * p1Hat + qHat;
    double expectedTau0 = std::max(0.0, p0Hat - expectedRhs);
    double expectedObjective = expectedTau0 - 1e-4 * p0Hat;

    EXPECT_NEAR(expectedObjective, lp.solver->getObjectiveValue(), 1e-5);
}

// 3-state DTMC: 0 -[p]-> 1 (free), 0 -[1-p]-> 2 (zeroStates/target). Row 0's reward is "2*p + 3"
// (affine, parametric -- contributes to the RHS directly, with no linearization needed since it
// never multiplies a p_succ variable). Reward-mode state values are on a reward scale (state0's
// hat value is 5.0, well above 1 -- probability mode would clamp its LP variable's upper bound to
// 1.0, which is *below* its near-degenerate lower bound of ~5.0, making the LP infeasible/invalid;
// this only works at all if the unbounded-above domain for reward-mode state variables is wired up
// correctly). Same near-degenerate trust region trick as the probability test above, to make the
// expected LP values checkable by direct arithmetic instead of by solving a nontrivial LP by hand.
TEST_P(ScpLpEncoderTest, Reward_ChecksRewardTermAndUnboundedDomain) {
    carl::VariablePool::getInstance().clear();
    auto cache = std::make_shared<storm::RawPolynomialCache>();
    carl::Variable varP = storm::createRFVariable("p");
    auto p = makeVarFunction(varP, cache);
    auto one = storm::RationalFunction(1);
    auto three = storm::RationalFunction(3);

    storm::storage::SparseMatrixBuilder<storm::RationalFunction> builder(3, 3, 4);
    builder.addNextValue(0, 1, p);
    builder.addNextValue(0, 2, one - p);
    builder.addNextValue(1, 1, one);
    builder.addNextValue(2, 2, one);
    auto matrix = builder.build();
    storm::models::sparse::Dtmc<storm::RationalFunction> dtmc(matrix, trivialLabeling(3));

    std::vector<storm::RationalFunction> stateActionRewards{p + p + three, storm::utility::zero<storm::RationalFunction>(),
                                                            storm::utility::zero<storm::RationalFunction>()};
    storm::models::sparse::StandardRewardModel<storm::RationalFunction> rewardModel(std::nullopt, stateActionRewards);
    AffinePMdpDecomposition decomposition(dtmc, &rewardModel);

    storm::storage::BitVector zeroStates(3), oneStates(3), infiniteStates(3);
    zeroStates.set(2);
    storm::storage::BitVector excludedRows(matrix.getRowCount(), false);

    std::map<carl::Variable, std::pair<double, double>> bounds{{varP, {0.0, 1.0}}};
    ScpOptions options;
    options.useMaxViolationObjective = true;

    // threshold chosen low enough that it never binds tighter than the Bellman-derived slack.
    ScpLpEncoder encoder(decomposition, toU64(matrix.getRowGroupIndices()), zeroStates, oneStates, infiniteStates, excludedRows, /*initialState=*/0, bounds,
                         /*threshold=*/0.5, ThresholdDirection::AtLeast, PropertyKind::Reward, options);

    double const pHat = 0.4, p0Hat = 5.0, p1Hat = 2.0;
    std::map<carl::Variable, double> valuation{{varP, pHat}};
    std::vector<double> stateValues{p0Hat, p1Hat, 0.0};

    auto lp = encoder.encode(env(), valuation, stateValues, /*trustRegionRadius=*/1.0 + 1e-6);
    lp.solver->optimize();
    ASSERT_TRUE(lp.solver->isOptimal());

    EXPECT_NEAR(pHat, lp.solver->getContinuousValue(lp.parameterVariables.at(varP)), 1e-5);
    EXPECT_NEAR(p0Hat, lp.solver->getContinuousValue(lp.stateVariables[0]), 1e-4);
    EXPECT_NEAR(p1Hat, lp.solver->getContinuousValue(lp.stateVariables[1]), 1e-4);

    // RHS = reward(row0) [affine in v_p directly, no linearization] + bilinear(p, state1), each
    // evaluated at its own linearization center since the near-degenerate trust region pins every
    // variable there.
    double expectedReward = 2 * pHat + 3;
    double expectedBilinear = pHat * p1Hat;
    double expectedRhs = expectedReward + expectedBilinear;
    double expectedTau0 = std::max(0.0, p0Hat - expectedRhs);
    ASSERT_GT(expectedTau0, 0.0);  // a nontrivial (non-zero) violation, not a degenerate all-zero check.
    double expectedObjective = expectedTau0 - 1e-4 * p0Hat;

    EXPECT_NEAR(expectedObjective, lp.solver->getObjectiveValue(), 1e-4);
}

INSTANTIATE_TEST_SUITE_P(LpSolvers, ScpLpEncoderTest, testing::ValuesIn(storm::pars::scp::testutils::lpSolversUnderTest()),
                         storm::pars::scp::testutils::LpSolverTest::solverName);
// The suite is empty if no LP solver was compiled in.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ScpLpEncoderTest);
