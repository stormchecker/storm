#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-pars/api/storm-pars.h"
#include "storm-parsers/api/properties.h"
#include "storm-parsers/parser/PrismParser.h"
#include "storm/api/builder.h"
#include "storm/api/properties.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/utility/constants.h"

namespace {

using Engine = storm::modelchecker::RegionCheckEngine;
using Method = storm::pars::FeasibilityMethod;
using Status = storm::modelchecker::RegionResult;
using Region = storm::storage::ParameterRegion<storm::RationalFunction>;

class SynthesisApiTest : public ::testing::Test {
   protected:
    void SetUp() override {
        storm::clearRFVariablePool();
#ifndef STORM_HAVE_SMT_SOLVER
        GTEST_SKIP() << "No SMT solver available.";
#endif
    }

    void build(std::string const& probability = "p", std::string const& rewards = "", bool mdp = false) {
        program = storm::parser::PrismParser::parseFromString(std::string(mdp ? "mdp\n" : "dtmc\n") +
                                                                  "const double p; const double r;\nmodule simple\n s : [0..2] init 0;\n"
                                                                  " [] s=0 -> " +
                                                                  probability + " : (s'=1) + (1-(" + probability + ")) : (s'=2);\n" +
                                                                  (mdp ? " [] s=0 -> (s'=1);\n [] s=0 -> (s'=2);\n" : "") +
                                                                  " [] s=1 -> (s'=1);\n [] s=2 -> (s'=2);\nendmodule\nlabel \"target\" = s=1;\n" + rewards,
                                                              "SynthesisApiTest");
        program = program.preprocess("");
        storm::builder::BuilderOptions options({formula("P=? [F \"target\"]")}, program);
        options.setBuildAllRewardModels(true);
        model = storm::api::buildSparseModel<storm::RationalFunction>(program, options);
    }

    std::shared_ptr<storm::logic::Formula const> formula(std::string const& text) const {
        return storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(text, program)).at(0);
    }

    Region region(std::string const& text = "0.2<=p<=0.8") const {
        return storm::api::parseRegion<storm::RationalFunction>(text, *model);
    }

    bool enableScp() {
#if defined STORM_HAVE_GLPK
        environment.solver().setLpSolverType(storm::solver::LpSolverType::Glpk);
#elif defined STORM_HAVE_HIGHS
        environment.solver().setLpSolverType(storm::solver::LpSolverType::Highs);
#elif defined STORM_HAVE_SOPLEX
        environment.solver().setLpSolverType(storm::solver::LpSolverType::Soplex);
#else
        return false;
#endif
        return true;
    }

    storm::Environment environment;
    storm::prism::Program program;
    std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> model;
};

TEST_F(SynthesisApiTest, VerificationEnginesAndOverloads) {
    build();
    Region::CoefficientType const bound = storm::utility::one<Region::CoefficientType>() / 5;
    for (auto engine : {Engine::ParameterLifting, Engine::ExactParameterLifting, Engine::ValidatingParameterLifting, Engine::RobustParameterLifting}) {
        SCOPED_TRACE(static_cast<int>(engine));
        auto property = formula("P<=0.9 [F \"target\"]");
        EXPECT_TRUE(storm::api::verify(environment, model, property, region(), engine));
        EXPECT_TRUE(storm::api::verify(environment, model, property, bound, engine));
        EXPECT_FALSE(storm::api::verify(environment, model, formula("P>=0.6 [F \"target\"]"), region(), engine));
        for (auto const& text : {"P<0.5 [F \"target\"]", "P>0.5 [F \"target\"]"}) {
            EXPECT_FALSE(storm::api::verify(environment, model, formula(text), region("0.4<=p<=0.6"), engine));
        }
    }
}

TEST_F(SynthesisApiTest, PlaEnginesAndInfeasibility) {
    build();
    for (auto engine : {Engine::ParameterLifting, Engine::ExactParameterLifting, Engine::ValidatingParameterLifting, Engine::RobustParameterLifting}) {
        SCOPED_TRACE(static_cast<int>(engine));
        auto result = storm::api::checkFeasibility(environment, model, formula("P>=0.6 [F \"target\"]"), region(), Method::PLA, engine);
        ASSERT_EQ(Status::ExistsSat, result.status);
        ASSERT_TRUE(result.witness);
        EXPECT_TRUE(region().contains(result.witness->valuation));
        EXPECT_GE(result.witness->value, 0.6);
        result = storm::api::checkFeasibility(environment, model, formula("P>=0.9 [F \"target\"]"), region(), Method::PLA, engine);
        EXPECT_EQ(Status::AllViolated, result.status);
        EXPECT_FALSE(result.witness);
    }
}

TEST_F(SynthesisApiTest, FeasibilityMethodsAndOverloads) {
    build("p", "rewards \"cost\"\n s=0 : r;\nendrewards\n");
    std::vector<Method> methods{Method::PLA, Method::GD};
    if (enableScp()) {
        methods.push_back(Method::SCP);
    }
    Region::CoefficientType const bound = storm::utility::one<Region::CoefficientType>() / 5;
    auto box = region("0.2<=p<=0.8,0.2<=r<=0.8");
    auto property = formula("P>=0.6 [F \"target\"]");
    for (auto method : methods) {
        SCOPED_TRACE(static_cast<int>(method));
        for (auto const& result : {storm::api::checkFeasibility(environment, model, property, box, method),
                                   storm::api::checkFeasibility(environment, model, property, bound, method)}) {
            ASSERT_EQ(Status::ExistsSat, result.status);
            ASSERT_TRUE(result.witness);
            EXPECT_EQ(2u, result.witness->valuation.size());
            EXPECT_GE(result.witness->value, 0.6);
            EXPECT_LE(result.witness->value, 0.8);
        }
    }
    if (enableScp()) {
        auto result = storm::api::checkFeasibility(environment, model, formula("P>=0.9 [F \"target\"]"), box, Method::SCP);
        EXPECT_EQ(Status::Unknown, result.status);
        EXPECT_FALSE(result.witness);
    }
}

TEST_F(SynthesisApiTest, SingletonRegions) {
    build();
    Region::CoefficientType const half = storm::utility::one<Region::CoefficientType>() / 2;
    EXPECT_TRUE(storm::api::verify(environment, model, formula("P>=0.5 [F \"target\"]"), half));
    for (auto method : {Method::PLA, Method::GD}) {
        auto result = storm::api::checkFeasibility(environment, model, formula("P>=0.5 [F \"target\"]"), half, method);
        ASSERT_EQ(Status::ExistsSat, result.status);
        ASSERT_TRUE(result.witness);
        EXPECT_EQ(half, result.witness->valuation.begin()->second);
        result = storm::api::checkFeasibility(environment, model, formula("P>0.5 [F \"target\"]"), half, method);
        EXPECT_EQ(Status::AllViolated, result.status);
        EXPECT_FALSE(result.witness);
    }
}

TEST_F(SynthesisApiTest, InvalidInput) {
    build();
    auto property = formula("P>=0.5 [F \"target\"]");
    EXPECT_THROW(storm::api::verify(environment, nullptr, property, region()), storm::exceptions::InvalidArgumentException);
    EXPECT_THROW(storm::api::checkFeasibility(environment, model, nullptr, region()), storm::exceptions::InvalidArgumentException);
    EXPECT_THROW(storm::api::verify(environment, model, formula("P=? [F \"target\"]"), region()), storm::exceptions::NotSupportedException);
    EXPECT_THROW(storm::api::checkFeasibility(environment, model, formula("P=? [F \"target\"]"), region()), storm::exceptions::NotSupportedException);
    EXPECT_THROW(storm::api::verify(environment, model, property, Region()), storm::exceptions::InvalidArgumentException);
    EXPECT_THROW(storm::api::checkFeasibility(environment, model, property, Region()), storm::exceptions::InvalidArgumentException);
    for (Region::CoefficientType const& invalid : {Region::CoefficientType(-1), Region::CoefficientType(1)}) {
        EXPECT_THROW(storm::api::verify(environment, model, property, invalid), storm::exceptions::InvalidArgumentException);
        EXPECT_THROW(storm::api::checkFeasibility(environment, model, property, invalid), storm::exceptions::InvalidArgumentException);
    }
}

TEST_F(SynthesisApiTest, RegionRequirements) {
    build();
    auto zero = storm::utility::zero<Region::CoefficientType>();
    auto property = formula("P<=1 [F \"target\"]");
    EXPECT_THROW(storm::api::verify(environment, model, property, zero), storm::exceptions::NotSupportedException);
    EXPECT_THROW(storm::api::checkFeasibility(environment, model, property, zero, Method::GD), storm::exceptions::NotSupportedException);
    EXPECT_TRUE(storm::api::verify(environment, model, property, zero, Engine::RobustParameterLifting));
    build("p+4/5");
    EXPECT_THROW(storm::api::verify(environment, model, property, region()), storm::exceptions::NotSupportedException);
    EXPECT_THROW(storm::api::checkFeasibility(environment, model, property, region()), storm::exceptions::NotSupportedException);
    build("p*p");
    EXPECT_TRUE(storm::api::verify(environment, model, property, region(), Engine::RobustParameterLifting));
}

TEST_F(SynthesisApiTest, RobustFormulaRequirements) {
    build("p", "rewards \"cost\"\n s=0 : r;\nendrewards\n");
    auto box = region("0.2<=p<=0.8,0.2<=r<=0.8");
    for (auto const& text : {"P<=0.9 [F<=2 \"target\"]", "R{\"cost\"}<=2 [C<=2]", "R{\"cost\"}<=2 [F \"target\"]"}) {
        auto property = formula(text);
        EXPECT_THROW(storm::api::verify(environment, model, property, box, Engine::RobustParameterLifting), storm::exceptions::NotSupportedException);
        EXPECT_THROW(storm::api::checkFeasibility(environment, model, property, box, Method::PLA, Engine::RobustParameterLifting),
                     storm::exceptions::NotSupportedException);
    }
}

TEST_F(SynthesisApiTest, MdpSchedulerQuantification) {
    build("p", "", true);
    auto box = region("0.4<=p<=0.8");
    // Pmin is zero and Pmax is one, so SCP's qualitative shortcut must not bypass the quantifier checks.
    EXPECT_TRUE(storm::api::verify(environment, model, formula("Pmax>=0.3 [F \"target\"]"), box));
    for (auto const& text : {"P>=0.6 [F \"target\"]", "P<=0.3 [F \"target\"]"}) {
        EXPECT_FALSE(storm::api::verify(environment, model, formula(text), box));
        EXPECT_EQ(Status::AllViolated, storm::api::checkFeasibility(environment, model, formula(text), box).status);
    }
    EXPECT_EQ(Status::ExistsSat, storm::api::checkFeasibility(environment, model, formula("Pmax>=0.6 [F \"target\"]"), box).status);
    EXPECT_EQ(Status::ExistsSat, storm::api::checkFeasibility(environment, model, formula("Pmin<=0.3 [F \"target\"]"), box).status);
    for (auto const& text : {"Pmax>=0.6 [F \"target\"]", "Pmin<=0.3 [F \"target\"]"}) {
        EXPECT_THROW(storm::api::checkFeasibility(environment, model, formula(text), box, Method::SCP), storm::exceptions::NotSupportedException);
    }
    EXPECT_EQ(Status::ExistsSat, storm::api::checkFeasibility(environment, model, formula("P>=0 [F \"target\"]"), box, Method::SCP).status);
    EXPECT_THROW(storm::api::verify(environment, model, formula("P<=1 [F \"target\"]"), box, Engine::RobustParameterLifting),
                 storm::exceptions::NotSupportedException);
    EXPECT_THROW(storm::api::checkFeasibility(environment, model, formula("P>=0.6 [F \"target\"]"), box, Method::GD), storm::exceptions::NotSupportedException);
}

TEST_F(SynthesisApiTest, RewardFormula) {
    build("p", "rewards \"cost\"\n s=0 : r;\nendrewards\n");
    auto box = region("0.5<=p<=0.5,2<=r<=4");
    EXPECT_TRUE(storm::api::verify(environment, model, formula("R{\"cost\"}<=4 [F !\"init\"]"), box));
    auto result = storm::api::checkFeasibility(environment, model, formula("R{\"cost\"}>=3 [F !\"init\"]"), box);
    ASSERT_EQ(Status::ExistsSat, result.status);
    ASSERT_TRUE(result.witness);
    EXPECT_TRUE(box.contains(result.witness->valuation));
    EXPECT_GE(result.witness->value, 3.0);
}

}  // namespace
