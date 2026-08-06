#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-conv/api/storm-conv.h"
#include "storm-parsers/api/model_descriptions.h"
#include "storm-parsers/api/properties.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/solver/LongRunAverageSolverEnvironment.h"
#include "storm/environment/solver/MinMaxSolverEnvironment.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/prctl/SparseDtmcPrctlModelChecker.h"
#include "storm/modelchecker/prctl/SparseMdpPrctlModelChecker.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/GeneralSettings.h"

namespace {
class DoubleViEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().lra().setNondetLraMethod(storm::solver::LraMethod::ValueIteration);
        return env;
    }
};
class DoubleSoundViEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::ValueIteration);
        env.solver().setForceSoundness(true);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-6));
        env.solver().lra().setNondetLraMethod(storm::solver::LraMethod::ValueIteration);
        return env;
    }
};
class DoublePIEnvironment {
   public:
    typedef double ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        env.solver().minMax().setPrecision(storm::utility::convertNumber<storm::RationalNumber>(1e-8));
        env.solver().lra().setNondetLraMethod(storm::solver::LraMethod::ValueIteration);
        return env;
    }
};
class RationalPIEnvironment {
   public:
    typedef storm::RationalNumber ValueType;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().minMax().setMethod(storm::solver::MinMaxMethod::PolicyIteration);
        return env;
    }
};
//   class RationalRationalSearchEnvironment {
//   public:
//       typedef storm::RationalNumber ValueType;
//       static storm::Environment createEnvironment() {
//           storm::Environment env;
//           env.solver().minMax().setMethod(storm::solver::MinMaxMethod::RationalSearch);
//           return env;
//       }
//   };

template<typename TestType>
class SchedulerGenerationMdpPrctlModelCheckerTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    SchedulerGenerationMdpPrctlModelCheckerTest() : _environment(TestType::createEnvironment()) {}

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }

    storm::Environment const& env() const {
        return _environment;
    }

    std::pair<std::shared_ptr<storm::models::sparse::Mdp<ValueType>>, std::vector<std::shared_ptr<storm::logic::Formula const>>> buildModelFormulas(
        std::string const& pathToPrismFile, std::string const& formulasAsString, std::string const& constantDefinitionString = "") const {
        std::pair<std::shared_ptr<storm::models::sparse::Mdp<ValueType>>, std::vector<std::shared_ptr<storm::logic::Formula const>>> result;
        storm::prism::Program program = storm::api::parseProgram(pathToPrismFile);
        program = program.preprocess(constantDefinitionString);
        result.second = storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
        result.first = storm::api::buildSparseModel<ValueType>(program, result.second)->template as<storm::models::sparse::Mdp<ValueType>>();
        return result;
    }

    std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>> getTasks(
        std::vector<std::shared_ptr<storm::logic::Formula const>> const& formulas) const {
        std::vector<storm::modelchecker::CheckTask<storm::logic::Formula, ValueType>> result;
        for (auto const& f : formulas) {
            result.emplace_back(*f);
            result.back().setProduceSchedulers(true);
        }
        return result;
    }

    ValueType parseNumber(std::string const& input) const {
        return storm::utility::convertNumber<ValueType>(input);
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<DoubleViEnvironment, DoubleSoundViEnvironment, DoublePIEnvironment, RationalPIEnvironment
                         // RationalRationalSearchEnvironment
                         >
    TestingTypes;

TYPED_TEST_SUITE(SchedulerGenerationMdpPrctlModelCheckerTest, TestingTypes, );

TYPED_TEST(SchedulerGenerationMdpPrctlModelCheckerTest, reachability) {
    typedef typename TestFixture::ValueType ValueType;

    std::string formulasString = "Pmin=? [F \"target\"]; Pmax=? [F \"target\"];";
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/scheduler_generation.nm", formulasString);
    auto mdp = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(4ull, mdp->getNumberOfStates());
    EXPECT_EQ(11ull, mdp->getNumberOfTransitions());
    ASSERT_EQ(mdp->getType(), storm::models::ModelType::Mdp);
    EXPECT_EQ(7ull, mdp->getNumberOfChoices());

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    auto result = checker.check(this->env(), tasks[0]);
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
    storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();
    EXPECT_EQ(0ull, scheduler.getChoice(0).getDeterministicChoice());
    EXPECT_EQ(1ull, scheduler.getChoice(1).getDeterministicChoice());
    EXPECT_EQ(0ull, scheduler.getChoice(2).getDeterministicChoice());
    EXPECT_EQ(0ull, scheduler.getChoice(3).getDeterministicChoice());

    result = checker.check(tasks[1]);
    ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
    ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
    storm::storage::Scheduler<ValueType> const& scheduler2 = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();
    EXPECT_EQ(1ull, scheduler2.getChoice(0).getDeterministicChoice());
    EXPECT_EQ(2ull, scheduler2.getChoice(1).getDeterministicChoice());
    EXPECT_EQ(0ull, scheduler2.getChoice(2).getDeterministicChoice());
    EXPECT_EQ(0ull, scheduler2.getChoice(3).getDeterministicChoice());
}

TYPED_TEST(SchedulerGenerationMdpPrctlModelCheckerTest, total_reward) {
    typedef typename TestFixture::ValueType ValueType;

    std::string formulasString = "Rmin=? [ C ];";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards.nm", formulasString);
    auto mdp = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(3ul, mdp->getNumberOfStates());
    EXPECT_EQ(4ul, mdp->getNumberOfTransitions());
    EXPECT_EQ(4ul, mdp->getNumberOfChoices());
    ASSERT_EQ(mdp->getType(), storm::models::ModelType::Mdp);
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    {
        auto result = checker.check(this->env(), tasks[0]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("0"), result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();
        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());

        auto inducedModel = mdp->applyScheduler(scheduler);
        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedDtmc = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedDtmc->getNumberOfChoices(), inducedDtmc->getNumberOfStates());
        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedDtmc);
        auto inducedResult = inducedChecker.check(this->env(), tasks[0]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("0"), inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
    }
}

TYPED_TEST(SchedulerGenerationMdpPrctlModelCheckerTest, reach_reward) {
    // Test case taken from https://github.com/stormchecker/storm/issues/297
    typedef typename TestFixture::ValueType ValueType;

    std::string formulasString = "Rmax=? [ F \"goal\" ];";

    auto [mdp, formulas] = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards2.nm", formulasString);
    auto tasks = this->getTasks(formulas);
    EXPECT_EQ(4ul, mdp->getNumberOfStates());
    EXPECT_EQ(7ul, mdp->getNumberOfChoices());
    EXPECT_EQ(12ul, mdp->getNumberOfTransitions());
    ASSERT_EQ(mdp->getType(), storm::models::ModelType::Mdp);
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    {
        auto result = checker.check(this->env(), tasks[0]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        ASSERT_TRUE(storm::utility::isInfinity(result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()]));
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();
        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());

        auto inducedModel = mdp->applyScheduler(scheduler);
        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedDtmc = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedDtmc->getNumberOfChoices(), inducedDtmc->getNumberOfStates());
        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedDtmc);
        auto inducedResult = inducedChecker.check(this->env(), tasks[0]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());
        EXPECT_TRUE(storm::utility::isInfinity(inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()]));
    }
}

TYPED_TEST(SchedulerGenerationMdpPrctlModelCheckerTest, reach_reward2) {
    // Test case taken from https://github.com/stormchecker/storm/issues/683
    typedef typename TestFixture::ValueType ValueType;

    std::string formulasString = "Rmax=? [ F \"goal\" ];";

    auto [mdp, formulas] = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/tiny_rewards3.nm", formulasString);
    auto tasks = this->getTasks(formulas);
    EXPECT_EQ(4ul, mdp->getNumberOfStates());
    EXPECT_EQ(5ul, mdp->getNumberOfChoices());
    EXPECT_EQ(6ul, mdp->getNumberOfTransitions());
    ASSERT_EQ(mdp->getType(), storm::models::ModelType::Mdp);
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    {
        auto result = checker.check(this->env(), tasks[0]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        ASSERT_TRUE(storm::utility::isInfinity(result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()]));
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();
        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());

        auto inducedModel = mdp->applyScheduler(scheduler);
        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedDtmc = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedDtmc->getNumberOfChoices(), inducedDtmc->getNumberOfStates());
        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedDtmc);
        auto inducedResult = inducedChecker.check(this->env(), tasks[0]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());
        EXPECT_TRUE(storm::utility::isInfinity(inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()]));
    }
}

TYPED_TEST(SchedulerGenerationMdpPrctlModelCheckerTest, lra) {
    typedef typename TestFixture::ValueType ValueType;

    std::string formulasString = "R{\"grants\"}max=? [ MP ]; R{\"grants\"}min=? [ MP ];";

    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/cs_nfail3.nm", formulasString);
    auto mdp = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(184ul, mdp->getNumberOfStates());
    EXPECT_EQ(541ul, mdp->getNumberOfTransitions());
    EXPECT_EQ(439ul, mdp->getNumberOfChoices());
    ASSERT_EQ(mdp->getType(), storm::models::ModelType::Mdp);
    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    if (!std::is_same<ValueType, double>::value) {
        GTEST_SKIP() << "Lra scheduler extraction not supported for LP based method";
    }
    {
        auto result = checker.check(this->env(), tasks[0]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("333/1000"), result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().lra().getPrecision()));
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();
        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());

        auto inducedModel = mdp->applyScheduler(scheduler);
        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedMdp = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedMdp->getNumberOfChoices(), inducedMdp->getNumberOfStates());
        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedMdp);
        auto inducedResult = inducedChecker.check(this->env(), tasks[0]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("333/1000"), inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().lra().getPrecision()));
    }
    {
        auto result = checker.check(this->env(), tasks[1]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("0"), result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().lra().getPrecision()));
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();
        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());
        auto inducedModel = mdp->applyScheduler(scheduler);
        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedMdp = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedMdp->getNumberOfChoices(), inducedMdp->getNumberOfStates());
        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedMdp);
        auto inducedResult = inducedChecker.check(this->env(), tasks[1]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("0"), inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().lra().getPrecision()));
    }
}

TYPED_TEST(SchedulerGenerationMdpPrctlModelCheckerTest, ltl) {
    typedef typename TestFixture::ValueType ValueType;

#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    std::string formulasString = "Pmax=? [X X s=0]; Pmin=? [G F \"target\"];  Pmax=? [(G F s>2) & (F G !(s=3))];";
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/scheduler_generation.nm", formulasString);
    auto mdp = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(4ull, mdp->getNumberOfStates());
    EXPECT_EQ(11ull, mdp->getNumberOfTransitions());
    ASSERT_EQ(mdp->getType(), storm::models::ModelType::Mdp);
    EXPECT_EQ(7ull, mdp->getNumberOfChoices());

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    {
        tasks[0].setOnlyInitialStatesRelevant(true);
        auto result = checker.check(this->env(), tasks[0]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("81/100"), result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();

        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(!scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());
        auto inducedModel = mdp->applyScheduler(scheduler);

        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedDtmc = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedDtmc->getNumberOfChoices(), inducedDtmc->getNumberOfStates());

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedDtmc);
        auto inducedResult = inducedChecker.check(this->env(), tasks[0]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("81/100"),
                    inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[*inducedDtmc->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
    }
    {
        tasks[1].setOnlyInitialStatesRelevant(true);
        auto result = checker.check(this->env(), tasks[1]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("1/2"), result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();

        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(!scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());
        auto inducedModel = mdp->applyScheduler(scheduler);

        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedDtmc = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedDtmc->getNumberOfChoices(), inducedDtmc->getNumberOfStates());

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedDtmc);
        auto inducedResult = inducedChecker.check(this->env(), tasks[1]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());

        EXPECT_NEAR(this->parseNumber("1/2"), inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[*inducedDtmc->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
    }
    {
        tasks[2].setOnlyInitialStatesRelevant(false);
        auto result = checker.check(this->env(), tasks[2]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("1/2"), result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();

        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(!scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());
        auto inducedModel = mdp->applyScheduler(scheduler);

        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedDtmc = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedDtmc->getNumberOfChoices(), inducedDtmc->getNumberOfStates());

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedDtmc);
        auto inducedResult = inducedChecker.check(this->env(), tasks[2]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());

        auto test = inducedResult->template asExplicitQuantitativeCheckResult<ValueType>().getValueVector();
        EXPECT_NEAR(this->parseNumber("1/2"), inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[0],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
        EXPECT_NEAR(this->parseNumber("1"), inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[1],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
        EXPECT_NEAR(this->parseNumber("0"), inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[2],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
    }
#else
    GTEST_SKIP();
#endif
}

TYPED_TEST(SchedulerGenerationMdpPrctlModelCheckerTest, ltlNondetChoice) {
    typedef typename TestFixture::ValueType ValueType;
#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    // Nondeterministic choice in an accepting EC.
    std::string formulasString = "Pmax=? [X X !(x=0)];";
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/prism-mec-example1.nm", formulasString);

    auto mdp = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(3ull, mdp->getNumberOfStates());
    EXPECT_EQ(5ull, mdp->getNumberOfTransitions());
    ASSERT_EQ(mdp->getType(), storm::models::ModelType::Mdp);
    EXPECT_EQ(4ull, mdp->getNumberOfChoices());

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    {
        tasks[0].setOnlyInitialStatesRelevant(true);
        auto result = checker.check(this->env(), tasks[0]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("1"), result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();

        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(!scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());
        auto inducedModel = mdp->applyScheduler(scheduler);

        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedDtmc = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedDtmc->getNumberOfChoices(), inducedDtmc->getNumberOfStates());

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedDtmc);
        auto inducedResult = inducedChecker.check(this->env(), tasks[0]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("1"), inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[*inducedDtmc->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
    }
#else
    GTEST_SKIP();
#endif
}

TYPED_TEST(SchedulerGenerationMdpPrctlModelCheckerTest, ltlUnsat) {
    typedef typename TestFixture::ValueType ValueType;
#ifdef STORM_HAVE_LTL_MODELCHECKING_SUPPORT
    // Nondeterministic choice in an accepting EC, Pmax unsatisfiable, Pmin tautology (compute 1-Pmax(!phi))
    std::string formulasString = "Pmax=? [(X X !(s=0)) & (X X (s=0))]; Pmin=? [(X X !(s=0)) | (X X (s=0))];";
    auto modelFormulas = this->buildModelFormulas(STORM_TEST_RESOURCES_DIR "/mdp/scheduler_generation.nm", formulasString);

    auto mdp = std::move(modelFormulas.first);
    auto tasks = this->getTasks(modelFormulas.second);
    EXPECT_EQ(4ull, mdp->getNumberOfStates());
    EXPECT_EQ(11ull, mdp->getNumberOfTransitions());
    ASSERT_EQ(mdp->getType(), storm::models::ModelType::Mdp);
    EXPECT_EQ(7ull, mdp->getNumberOfChoices());

    storm::modelchecker::SparseMdpPrctlModelChecker<storm::models::sparse::Mdp<ValueType>> checker(*mdp);

    {
        tasks[0].setOnlyInitialStatesRelevant(true);
        auto result = checker.check(this->env(), tasks[0]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("0"), result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();

        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());
        auto inducedModel = mdp->applyScheduler(scheduler);

        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedMdp = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedMdp->getNumberOfChoices(), inducedMdp->getNumberOfStates());

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedMdp);
        auto inducedResult = inducedChecker.check(this->env(), tasks[0]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("0"), inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[*inducedMdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
    }

    {
        tasks[1].setOnlyInitialStatesRelevant(true);
        auto result = checker.check(this->env(), tasks[1]);
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        ASSERT_TRUE(result->template asExplicitQuantitativeCheckResult<ValueType>().hasScheduler());
        EXPECT_NEAR(this->parseNumber("1"), result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
        storm::storage::Scheduler<ValueType> const& scheduler = result->template asExplicitQuantitativeCheckResult<ValueType>().getScheduler();

        EXPECT_TRUE(scheduler.isDeterministicScheduler());
        EXPECT_TRUE(scheduler.isMemorylessScheduler());
        EXPECT_TRUE(!scheduler.isPartialScheduler());
        auto inducedModel = mdp->applyScheduler(scheduler);

        ASSERT_EQ(inducedModel->getType(), storm::models::ModelType::Dtmc);
        auto const& inducedMdp = inducedModel->template as<storm::models::sparse::Dtmc<ValueType>>();
        EXPECT_EQ(inducedMdp->getNumberOfChoices(), inducedMdp->getNumberOfStates());

        storm::modelchecker::SparseDtmcPrctlModelChecker<storm::models::sparse::Dtmc<ValueType>> inducedChecker(*inducedMdp);
        auto inducedResult = inducedChecker.check(this->env(), tasks[1]);
        ASSERT_TRUE(inducedResult->isExplicitQuantitativeCheckResult());
        EXPECT_NEAR(this->parseNumber("1"), inducedResult->template asExplicitQuantitativeCheckResult<ValueType>()[*inducedMdp->getInitialStates().begin()],
                    storm::utility::convertNumber<ValueType>(this->env().solver().minMax().getPrecision()));
    }
#else
    GTEST_SKIP();
#endif
}
}  // namespace
