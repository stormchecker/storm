#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/jani/Model.h"
#include "storm/utility/solver.h"

namespace {

// The tags below select the SMT solver that flattening is tested with. Note that flattenModules() obtains its
// solver through the environment-less overload of SmtSolverFactory::create(), which always uses the
// compile-time default. Hence, a tag has to provide the concrete factory instead of an SMT solver type.
class DefaultEnvironment {
   public:
    static std::shared_ptr<storm::utility::solver::SmtSolverFactory> factory() {
        return std::make_shared<storm::utility::solver::DefaultSmtSolverFactory>();
    }

    static bool skip() {
#ifdef STORM_HAVE_SMT_SOLVER
        return false;
#else
        return true;
#endif
    }

    static std::string name() {
        return "Default";
    }
};

#ifdef STORM_HAVE_MATHSAT
class MathsatEnvironment {
   public:
    static std::shared_ptr<storm::utility::solver::SmtSolverFactory> factory() {
        return std::make_shared<storm::utility::solver::MathsatSmtSolverFactory>();
    }

    static bool skip() {
        return false;
    }

    static std::string name() {
        return "Mathsat";
    }
};
#endif

#ifdef STORM_HAVE_Z3
class Z3Environment {
   public:
    static std::shared_ptr<storm::utility::solver::SmtSolverFactory> factory() {
        return std::make_shared<storm::utility::solver::Z3SmtSolverFactory>();
    }

    static bool skip() {
        return false;
    }

    static std::string name() {
        return "Z3";
    }
};
#endif

#ifdef STORM_HAVE_CVC5
class Cvc5Environment {
   public:
    static std::shared_ptr<storm::utility::solver::SmtSolverFactory> factory() {
        return std::make_shared<storm::utility::solver::Cvc5SmtSolverFactory>();
    }

    static bool skip() {
        return false;
    }

    static std::string name() {
        return "Cvc5";
    }
};
#endif

}  // namespace

template<typename TestType>
class PrismProgramFlattenTest : public ::testing::Test {
   public:
    void SetUp() override {
        if (skipped()) {
            GTEST_SKIP() << "No SMT solver available.";
        }
    }

    void flattenModules(std::string const& file, std::size_t expectedNumberOfCommands) {
        storm::prism::Program program;
        ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/" + file));

        std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = TestType::factory();

        ASSERT_NO_THROW(program = program.substituteFormulas().flattenModules(smtSolverFactory));
        EXPECT_EQ(1ull, program.getNumberOfModules());
        EXPECT_EQ(expectedNumberOfCommands, program.getModule(0).getNumberOfCommands());
    }

    bool skipped() const {
        return TestType::skip();
    }
};

// Puts the SMT solver into the test name, so that a failure identifies the backend that produced it.
class SmtSolverNameGenerator {
   public:
    template<typename ParamType>
    static std::string GetName(int) {
        return ParamType::name();
    }
};

typedef ::testing::Types<DefaultEnvironment
#ifdef STORM_HAVE_MATHSAT
                         ,
                         MathsatEnvironment
#endif
#ifdef STORM_HAVE_Z3
                         ,
                         Z3Environment
#endif
#ifdef STORM_HAVE_CVC5
                         ,
                         Cvc5Environment
#endif
                         >
    FlattenModulesTypes;

TYPED_TEST_SUITE(PrismProgramFlattenTest, FlattenModulesTypes, SmtSolverNameGenerator);

TYPED_TEST(PrismProgramFlattenTest, FlattenModules_Leader) {
    this->flattenModules("leader3.nm", 74);
}

TYPED_TEST(PrismProgramFlattenTest, FlattenModules_Wlan) {
    this->flattenModules("wlan0_collide.nm", 179);
}

TYPED_TEST(PrismProgramFlattenTest, FlattenModules_Csma) {
    this->flattenModules("csma2_2.nm", 70);
}

STORM_EXPENSIVE_TYPED_TEST(PrismProgramFlattenTest, FlattenModules_Firewire) {
    this->flattenModules("firewire.nm", 5024);
}

TYPED_TEST(PrismProgramFlattenTest, FlattenModules_Coin) {
    this->flattenModules("coin2.nm", 13);
}

TYPED_TEST(PrismProgramFlattenTest, FlattenModules_Dice) {
    this->flattenModules("two_dice.nm", 16);
}

TEST(PrismProgramTest, ConvertToJani) {
    storm::prism::Program prismProgram;
    storm::jani::Model janiModel;

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0_collide.nm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-5-5.pm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/leader-3-5.pm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/nand-5-2.pm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());

    ASSERT_NO_THROW(prismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/unbounded.nm"));
    ASSERT_NO_THROW(janiModel = prismProgram.toJani());
}

TEST(PrismProgramTest, ReplaceInitialStates) {
    storm::prism::Program origPrismProgram;
    storm::prism::Program transformedPrismProgram;
    ASSERT_NO_THROW(origPrismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/brp-16-2.pm"));
    ASSERT_NO_THROW(transformedPrismProgram = origPrismProgram.replaceVariableInitializationByInitExpression());
    EXPECT_TRUE(origPrismProgram.getInitialStatesExpression().isSyntacticallyEqual(transformedPrismProgram.getInitialStatesExpression()));
    EXPECT_TRUE(transformedPrismProgram.hasInitialConstruct());
}

TEST(PrismProgramTest, ReplaceConstantByVariable) {
    storm::prism::Program origPrismProgram;
    storm::prism::Program transformedPrismProgram;
    ASSERT_NO_THROW(origPrismProgram = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds-4-3.pm"));
    ASSERT_NO_THROW(transformedPrismProgram = origPrismProgram.replaceConstantByVariable(
                        origPrismProgram.getConstant("CrowdSize"), origPrismProgram.getManager().integer(0), origPrismProgram.getManager().integer(20), true));
    EXPECT_NO_THROW(transformedPrismProgram.getGlobalIntegerVariable("CrowdSize"));
    EXPECT_FALSE(transformedPrismProgram.hasConstant("CrowdSize"));
}