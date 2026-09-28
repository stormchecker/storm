#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/storage/jani/Model.h"
#include "storm/utility/solver.h"

namespace {

// The tags below select the SMT solver that flattening is tested with. Note that flattenComposition() obtains
// its solver through the environment-less overload of SmtSolverFactory::create(), which always uses the
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
class JaniModelFlattenTest : public ::testing::Test {
   public:
    void SetUp() override {
        if (skipped()) {
            GTEST_SKIP() << "No SMT solver available.";
        }
    }

    void flattenComposition(std::string const& file, std::size_t expectedNumberOfEdges) {
        storm::prism::Program program;
        ASSERT_NO_THROW(program = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/" + file));
        storm::jani::Model janiModel = program.toJani();

        std::shared_ptr<storm::utility::solver::SmtSolverFactory> smtSolverFactory = TestType::factory();

        janiModel.substituteFunctions();
        ASSERT_NO_THROW(janiModel = janiModel.flattenComposition(smtSolverFactory));
        EXPECT_EQ(1ull, janiModel.getNumberOfAutomata());
        EXPECT_EQ(expectedNumberOfEdges, janiModel.getAutomaton(0).getNumberOfEdges());
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
    FlattenCompositionTypes;

TYPED_TEST_SUITE(JaniModelFlattenTest, FlattenCompositionTypes, SmtSolverNameGenerator);

TYPED_TEST(JaniModelFlattenTest, FlattenComposition_Leader) {
    this->flattenComposition("leader3.nm", 74);
}

TYPED_TEST(JaniModelFlattenTest, FlattenComposition_Wlan) {
    this->flattenComposition("wlan0_collide.nm", 179);
}

TYPED_TEST(JaniModelFlattenTest, FlattenComposition_Csma) {
    this->flattenComposition("csma2_2.nm", 70);
}

STORM_EXPENSIVE_TYPED_TEST(JaniModelFlattenTest, FlattenComposition_Firewire) {
    this->flattenComposition("firewire.nm", 5024);
}

TYPED_TEST(JaniModelFlattenTest, FlattenComposition_Coin) {
    this->flattenComposition("coin2.nm", 13);
}

TYPED_TEST(JaniModelFlattenTest, FlattenComposition_Dice) {
    this->flattenComposition("two_dice.nm", 16);
}
