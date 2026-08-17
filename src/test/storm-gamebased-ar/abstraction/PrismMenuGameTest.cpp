#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-gamebased-ar/abstraction/MenuGameRefiner.h"
#include "storm-gamebased-ar/abstraction/prism/PrismMenuGameAbstractor.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/environment/Environment.h"
#include "storm/models/symbolic/StandardRewardModel.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/AbstractionSettings.h"
#include "storm/storage/dd/Bdd.h"
#include "storm/storage/expressions/Expression.h"
#include "storm/utility/solver.h"

class Cudd {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_CUDD
        GTEST_SKIP() << "Library CUDD not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::CUDD;
};

class Sylvan {
   public:
    static void checkLibraryAvailable() {
#ifndef STORM_HAVE_SYLVAN
        GTEST_SKIP() << "Library Sylvan not available.";
#endif
    }

    static const storm::dd::DdType DdType = storm::dd::DdType::Sylvan;
};

template<typename TestType>
class PrismMenuGame : public ::testing::Test {
   public:
    static const storm::dd::DdType DdType = TestType::DdType;

   protected:
    void SetUp() override {
#ifndef STORM_HAVE_MATHSAT
        GTEST_SKIP() << "MathSAT not available.";
#endif
        TestType::checkLibraryAvailable();
    }
};

typedef ::testing::Types<Cudd, Sylvan> TestingTypes;
TYPED_TEST_SUITE(PrismMenuGame, TestingTypes, );

TYPED_TEST(PrismMenuGame, DieAbstractionTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("s") < manager.integer(3));
    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(26ull, game.getNumberOfTransitions());
    EXPECT_EQ(4ull, game.getNumberOfStates());
    EXPECT_EQ(2ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}

TYPED_TEST(PrismMenuGame, DieAbstractionAndRefinementTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("s") < manager.integer(3));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    ASSERT_NO_THROW(refiner.refine({manager.getVariableExpression("s") == manager.integer(7)}));

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(24ull, game.getNumberOfTransitions());
    EXPECT_EQ(5ull, game.getNumberOfStates());
    EXPECT_EQ(2ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}

TYPED_TEST(PrismMenuGame, DieFullAbstractionTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm");

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("s") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("s") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("s") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("s") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("s") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("s") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("s") == manager.integer(7));

    initialPredicates.push_back(manager.getVariableExpression("d") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("d") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("d") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("d") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("d") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("d") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("d") == manager.integer(6));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(20ull, game.getNumberOfTransitions());
    EXPECT_EQ(13ull, game.getNumberOfStates());
    EXPECT_EQ(0ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}

TYPED_TEST(PrismMenuGame, CrowdsAbstractionTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.pm");
    program = program.substituteConstantsFormulas();

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("phase") < manager.integer(3));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(31ull, game.getNumberOfTransitions());
    EXPECT_EQ(4ull, game.getNumberOfStates());
    EXPECT_EQ(2ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}

TYPED_TEST(PrismMenuGame, CrowdsAbstractionAndRefinementTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.pm");
    program = program.substituteConstantsFormulas();

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("phase") < manager.integer(3));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    ASSERT_NO_THROW(
        refiner.refine({manager.getVariableExpression("observe0") + manager.getVariableExpression("observe1") + manager.getVariableExpression("observe2") +
                            manager.getVariableExpression("observe3") + manager.getVariableExpression("observe4") <=
                        manager.getVariableExpression("runCount")}));

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(68ull, game.getNumberOfTransitions());
    EXPECT_EQ(8ull, game.getNumberOfStates());
    EXPECT_EQ(4ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}

TYPED_TEST(PrismMenuGame, CrowdsFullAbstractionTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.pm");
    program = program.substituteConstantsFormulas();

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("phase") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("phase") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("phase") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("phase") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("phase") == manager.integer(4));

    initialPredicates.push_back(manager.getVariableExpression("good"));

    initialPredicates.push_back(manager.getVariableExpression("runCount") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("runCount") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("runCount") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("runCount") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("runCount") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("runCount") == manager.integer(5));

    initialPredicates.push_back(manager.getVariableExpression("observe0") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("observe0") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("observe0") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("observe0") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("observe0") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("observe0") == manager.integer(5));

    initialPredicates.push_back(manager.getVariableExpression("observe1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("observe1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("observe1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("observe1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("observe1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("observe1") == manager.integer(5));

    initialPredicates.push_back(manager.getVariableExpression("observe2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("observe2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("observe2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("observe2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("observe2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("observe2") == manager.integer(5));

    initialPredicates.push_back(manager.getVariableExpression("observe3") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("observe3") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("observe3") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("observe3") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("observe3") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("observe3") == manager.integer(5));

    initialPredicates.push_back(manager.getVariableExpression("observe4") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("observe4") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("observe4") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("observe4") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("observe4") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("observe4") == manager.integer(5));

    initialPredicates.push_back(manager.getVariableExpression("lastSeen") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("lastSeen") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("lastSeen") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("lastSeen") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("lastSeen") == manager.integer(4));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(15113ull, game.getNumberOfTransitions());
    EXPECT_EQ(8607ull, game.getNumberOfStates());
    EXPECT_EQ(0ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}

TYPED_TEST(PrismMenuGame, TwoDiceAbstractionTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm");
    program = program.substituteConstantsFormulas();
    program = program.flattenModules(std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>());

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("s1") < manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(0));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(90ull, game.getNumberOfTransitions());
    EXPECT_EQ(8ull, game.getNumberOfStates());
    EXPECT_EQ(4ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}
TYPED_TEST(PrismMenuGame, TwoDiceAbstractionAndRefinementTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm");
    program = program.substituteConstantsFormulas();
    program = program.flattenModules(std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>());

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("s1") < manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(0));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    ASSERT_NO_THROW(refiner.refine({manager.getVariableExpression("d1") + manager.getVariableExpression("d2") == manager.integer(7)}));

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(276ull, game.getNumberOfTransitions());
    EXPECT_EQ(16ull, game.getNumberOfStates());
    EXPECT_EQ(8ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}
TYPED_TEST(PrismMenuGame, TwoDiceFullAbstractionTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm");
    program = program.substituteConstantsFormulas();
    program = program.flattenModules(std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>());

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(7));

    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("d1") == manager.integer(6));

    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(7));

    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("d2") == manager.integer(6));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(436ull, game.getNumberOfTransitions());
    EXPECT_EQ(169ull, game.getNumberOfStates());
    EXPECT_EQ(0ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}

TYPED_TEST(PrismMenuGame, WlanAbstractionTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0-2-4.nm");
    program = program.substituteConstantsFormulas();
    program = program.flattenModules(std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>());

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("s1") < manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("bc1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("c1") == manager.getVariableExpression("c2"));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(903ull, game.getNumberOfTransitions());
    EXPECT_EQ(8ull, game.getNumberOfStates());
    EXPECT_EQ(4ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}

TYPED_TEST(PrismMenuGame, WlanAbstractionAndRefinementTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0-2-4.nm");
    program = program.substituteConstantsFormulas();
    program = program.flattenModules(std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>());

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("s1") < manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("bc1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("c1") == manager.getVariableExpression("c2"));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    ASSERT_NO_THROW(refiner.refine({manager.getVariableExpression("backoff1") < manager.integer(7)}));

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(1800ull, game.getNumberOfTransitions());
    EXPECT_EQ(16ull, game.getNumberOfStates());
    EXPECT_EQ(8ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}

TYPED_TEST(PrismMenuGame, WlanFullAbstractionTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto& settings = storm::settings::mutableAbstractionSettings();
    settings.setAddAllGuards(false);
    settings.setAddAllInitialExpressions(false);

    storm::prism::Program program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0-2-4.nm");
    program = program.substituteConstantsFormulas();
    program = program.flattenModules(std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>());

    std::vector<storm::expressions::Expression> initialPredicates;
    storm::expressions::ExpressionManager& manager = program.getManager();

    initialPredicates.push_back(manager.getVariableExpression("col") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("col") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("col") == manager.integer(2));

    initialPredicates.push_back(manager.getVariableExpression("c1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("c1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("c1") == manager.integer(2));

    initialPredicates.push_back(manager.getVariableExpression("c2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("c2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("c2") == manager.integer(2));

    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("x1") == manager.integer(7));

    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(7));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(8));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(9));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(10));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(11));
    initialPredicates.push_back(manager.getVariableExpression("s1") == manager.integer(12));

    initialPredicates.push_back(manager.getVariableExpression("slot1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("slot1") == manager.integer(1));

    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(7));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(8));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(9));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(10));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(11));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(12));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(13));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(14));
    initialPredicates.push_back(manager.getVariableExpression("backoff1") == manager.integer(15));

    initialPredicates.push_back(manager.getVariableExpression("bc1") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("bc1") == manager.integer(1));

    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("x2") == manager.integer(7));

    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(7));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(8));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(9));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(10));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(11));
    initialPredicates.push_back(manager.getVariableExpression("s2") == manager.integer(12));

    initialPredicates.push_back(manager.getVariableExpression("slot2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("slot2") == manager.integer(1));

    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(1));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(2));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(3));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(4));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(5));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(6));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(7));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(8));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(9));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(10));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(11));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(12));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(13));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(14));
    initialPredicates.push_back(manager.getVariableExpression("backoff2") == manager.integer(15));

    initialPredicates.push_back(manager.getVariableExpression("bc2") == manager.integer(0));
    initialPredicates.push_back(manager.getVariableExpression("bc2") == manager.integer(1));

    std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();

    storm::gbar::abstraction::prism::PrismMenuGameAbstractor<DdType, double> abstractor(storm::Environment(), program, smtSolverFactory);
    storm::gbar::abstraction::MenuGameRefiner<DdType, double> refiner(abstractor, smtSolverFactory->create(manager));
    refiner.refine(initialPredicates);

    storm::gbar::abstraction::MenuGame<DdType, double> game = abstractor.abstract();

    EXPECT_EQ(9503ull, game.getNumberOfTransitions());
    EXPECT_EQ(5523ull, game.getNumberOfStates());
    EXPECT_EQ(0ull, game.getBottomStates().getNonZeroCount());

    storm::settings::mutableAbstractionSettings().restoreDefaults();
}
