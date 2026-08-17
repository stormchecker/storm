#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm/api/storm.h"
#include "storm/environment/Environment.h"
#include "storm/environment/modelchecker/MultiObjectiveModelCheckerEnvironment.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/modelchecker/multiobjective/MultiObjectiveModelChecking.h"
#include "storm/modelchecker/results/ExplicitParetoCurveCheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/storage/SchedulerClass.h"

namespace {

class FlowBigMEnvironment {
   public:
    static storm::Environment getEnv() {
        storm::Environment env;
        env.modelchecker().multi().setEncodingType(storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Flow);
        env.modelchecker().multi().setUseIndicatorConstraints(false);
        env.modelchecker().multi().setUseBsccOrderEncoding(false);         // not relevant for Flow
        env.modelchecker().multi().setUseRedundantBsccConstraints(false);  // not relevant for Flow
        return env;
    }
};

class FlowIndicatorEnvironment {
   public:
    static storm::Environment getEnv() {
        storm::Environment env;
        env.modelchecker().multi().setEncodingType(storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Flow);
        env.modelchecker().multi().setUseIndicatorConstraints(true);
        env.modelchecker().multi().setUseBsccOrderEncoding(false);         // not relevant for Flow
        env.modelchecker().multi().setUseRedundantBsccConstraints(false);  // not relevant for Flow
        return env;
    }
};

class BigMEnvironment {
   public:
    static storm::Environment getEnv() {
        storm::Environment env;
        env.modelchecker().multi().setEncodingType(storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Classic);
        env.modelchecker().multi().setUseIndicatorConstraints(false);
        env.modelchecker().multi().setUseBsccOrderEncoding(false);
        env.modelchecker().multi().setUseRedundantBsccConstraints(false);
        return env;
    }
};

class IndicatorEnvironment {
   public:
    static storm::Environment getEnv() {
        storm::Environment env;
        env.modelchecker().multi().setEncodingType(storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Classic);
        env.modelchecker().multi().setUseIndicatorConstraints(true);
        env.modelchecker().multi().setUseBsccOrderEncoding(false);
        env.modelchecker().multi().setUseRedundantBsccConstraints(false);
        return env;
    }
};

class BigMOrderEnvironment {
   public:
    static storm::Environment getEnv() {
        storm::Environment env;
        env.modelchecker().multi().setEncodingType(storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Classic);
        env.modelchecker().multi().setUseIndicatorConstraints(false);
        env.modelchecker().multi().setUseBsccOrderEncoding(true);
        env.modelchecker().multi().setUseRedundantBsccConstraints(false);
        return env;
    }
};

class IndicatorOrderEnvironment {
   public:
    static storm::Environment getEnv() {
        storm::Environment env;
        env.modelchecker().multi().setEncodingType(storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Classic);
        env.modelchecker().multi().setUseIndicatorConstraints(true);
        env.modelchecker().multi().setUseBsccOrderEncoding(true);
        env.modelchecker().multi().setUseRedundantBsccConstraints(false);
        return env;
    }
};

class RedundantEnvironment {
   public:
    static storm::Environment getEnv() {
        storm::Environment env;
        env.modelchecker().multi().setEncodingType(storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Classic);
        env.modelchecker().multi().setUseIndicatorConstraints(false);
        env.modelchecker().multi().setUseBsccOrderEncoding(false);
        env.modelchecker().multi().setUseRedundantBsccConstraints(true);
        return env;
    }
};

class RedundantOrderEnvironment {
   public:
    static storm::Environment getEnv() {
        storm::Environment env;
        env.modelchecker().multi().setEncodingType(storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Classic);
        env.modelchecker().multi().setUseIndicatorConstraints(false);
        env.modelchecker().multi().setUseBsccOrderEncoding(true);
        env.modelchecker().multi().setUseRedundantBsccConstraints(true);
        return env;
    }
};

template<typename TestType>
class MultiObjectiveSchedRestModelCheckerTest : public ::testing::Test {
   public:
    typedef storm::RationalNumber ValueType;

    void SetUp() override {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
    }

    bool isFlowEncoding() const {
        return TestType::getEnv().modelchecker().multi().getEncodingType() == storm::MultiObjectiveModelCheckerEnvironment::EncodingType::Flow;
    }

    storm::Environment getPositionalDeterministicEnvironment() {
        auto env = TestType::getEnv();
        env.modelchecker().multi().setSchedulerRestriction(storm::storage::SchedulerClass().setPositional().setIsDeterministic());
        env.solver().setForceExact(true);
        return env;
    }

    storm::Environment getGoalDeterministicEnvironment() {
        auto env = TestType::getEnv();
        env.modelchecker().multi().setSchedulerRestriction(
            storm::storage::SchedulerClass().setMemoryPattern(storm::storage::SchedulerClass::MemoryPattern::GoalMemory).setIsDeterministic());
        env.solver().setForceExact(true);
        return env;
    }

    typedef std::vector<storm::RationalNumber> Point;

    ValueType parseNumber(std::string const& input) const {
        return storm::utility::convertNumber<ValueType>(input);
    }

    std::vector<Point> parsePoints(std::vector<std::string> const& input) {
        std::vector<Point> result;
        for (auto const& i : input) {
            Point currPoint;
            std::size_t pos1 = 0;
            std::size_t pos2 = i.find(",");
            while (pos2 != std::string::npos) {
                currPoint.push_back(parseNumber(i.substr(pos1, pos2 - pos1)));
                pos1 = pos2 + 1;
                pos2 = i.find(",", pos1);
            }
            currPoint.push_back(parseNumber(i.substr(pos1)));
            result.push_back(currPoint);
        }
        return result;
    }

    std::set<Point> setMinus(std::vector<Point> const& lhs, std::vector<Point> const& rhs) {
        std::set<Point> result(lhs.begin(), lhs.end());
        for (auto const& r : rhs) {
            for (auto lIt = result.begin(); lIt != result.end(); ++lIt) {
                if (*lIt == r) {
                    result.erase(lIt);
                    break;
                }
            }
        }
        return result;
    }

    std::string toString(Point point, bool asDouble) {
        std::stringstream s;
        s << "[";
        bool first = true;
        for (auto const& pi : point) {
            if (first) {
                first = false;
            } else {
                s << ", ";
            }
            if (asDouble) {
                s << storm::utility::convertNumber<double>(pi);
            } else {
                s << pi;
            }
        }
        s << "]";
        return s.str();
    }

    std::string getDiffString(std::vector<Point> const& expected, std::vector<Point> const& actual) {
        std::stringstream stream;
        stream << "Unexpected set of Points:\n";
        stream << "    Expected  |  Actual  |  Point\n";
        for (auto const& p : expected) {
            if (std::find(actual.begin(), actual.end(), p) != actual.end()) {
                stream << "       yes    |   yes    |  " << toString(p, true) << "\n";
            } else {
                stream << " -->   yes    |   no     |  " << toString(p, true) << "\n";
            }
        }
        for (auto const& p : actual) {
            if (std::find(expected.begin(), expected.end(), p) == expected.end()) {
                stream << " -->   no     |   yes    |  " << toString(p, true) << "\n";
            }
        }
        return stream.str();
    }

    bool isSame(std::vector<Point> const& expected, std::vector<Point> const& actual, std::string& diffString) {
        if (expected.size() != actual.size()) {
            diffString = getDiffString(expected, actual);
            return false;
        }
        for (auto const& p : expected) {
            if (std::find(actual.begin(), actual.end(), p) == actual.end()) {
                diffString = getDiffString(expected, actual);
                return false;
            }
        }
        diffString = "";
        return true;
    }

    template<typename SparseModelType>
    bool testParetoFormula(storm::Environment const& env, SparseModelType const& model, storm::logic::Formula const& formula,
                           std::vector<Point> const& expected, std::string& errorString) {
        auto result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *model, formula.asMultiObjectiveFormula());
        if (!result->isParetoCurveCheckResult()) {
            errorString = "Not a ParetoCurveCheckResult";
            return false;
        }
        return isSame(expected, result->template asExplicitParetoCurveCheckResult<ValueType>().getPoints(), errorString);
    }
};

typedef ::testing::Types<FlowBigMEnvironment, FlowIndicatorEnvironment, BigMEnvironment, IndicatorEnvironment, BigMOrderEnvironment, IndicatorOrderEnvironment,
                         RedundantEnvironment, RedundantOrderEnvironment>
    TestingTypes;

TYPED_TEST_SUITE(MultiObjectiveSchedRestModelCheckerTest, TestingTypes, );

TYPED_TEST(MultiObjectiveSchedRestModelCheckerTest, steps) {
    typedef typename TestFixture::ValueType ValueType;

    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_stairs.nm";
    std::string constantsString = "N=3";
    std::string formulasAsString = "multi(Pmax=? [ F y=1], Pmax=? [ F y=2 ]);";
    formulasAsString += "multi(P>=0.375 [ F y=1], Pmax>=0.5 [ F y=2 ]);";
    formulasAsString += "multi(P>=0.4 [ F y=1], Pmax>=0.4 [ F y=2 ]);";
    formulasAsString += "multi(Pmax=? [ F y=1], Pmax>=0.4 [ F y=2 ]);";
    formulasAsString += "multi(Pmax=? [ F y=1], Pmax>=0.9 [ F y=2 ]);";

    // programm, model, formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsString);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> mdp =
        storm::api::buildSparseModel<ValueType>(program, formulas)->template as<storm::models::sparse::Mdp<ValueType>>();
    std::string errorString;  // storage for error reporting

    storm::Environment env = this->getPositionalDeterministicEnvironment();
    uint64_t formulaIndex = 0;
    {
        auto expected = this->parsePoints({"0.875,0", "0,0.875", "0.125,0.75", "0.25,0.625", "0.375,0.5", "0.5,0.375", "0.625,0.25", "0.75,0.125"});
        EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;
    }
    ++formulaIndex;
    {
        auto result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[formulaIndex]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
        EXPECT_TRUE(result->template asExplicitQualitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()]);
    }
    ++formulaIndex;
    {
        auto result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[formulaIndex]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
        EXPECT_FALSE(result->template asExplicitQualitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()]);
    }
    ++formulaIndex;
    {
        auto result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[formulaIndex]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitQuantitativeCheckResult());
        auto expected = storm::utility::convertNumber<ValueType, std::string>("0.375");
        EXPECT_EQ(result->template asExplicitQuantitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()], expected);
    }
    ++formulaIndex;
    {
        auto result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[formulaIndex]->asMultiObjectiveFormula());
        ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
        EXPECT_FALSE(result->template asExplicitQualitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()]);
    }
}

TYPED_TEST(MultiObjectiveSchedRestModelCheckerTest, mecs) {
    typedef typename TestFixture::ValueType ValueType;
    typedef typename TestFixture::Point Point;
    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_mecs.nm";
    std::string constantsString = "";
    std::string formulasAsString = "multi(Pmin=? [ F x=3], Pmax=? [ F x=4 ]);";
    formulasAsString += "\nmulti(R{\"test\"}min=? [C], Pmin=? [F \"t1\"]);";
    formulasAsString += "\nmulti(R{\"test\"}min=? [C], Pmin=? [F \"t2\"]);";
    formulasAsString += "\nmulti(R{\"test\"}min=? [C], Pmin=? [F \"t2\"], Pmax=? [F \"t1\"]);";
    formulasAsString += "\nmulti(R{\"test\"}<=0 [C], P<=1 [F \"t2\"], P>=0 [F \"t1\"]);";
    formulasAsString += "\nmulti(R{\"test\"}<=1 [C], P<=1 [F \"t2\"], P>=0.1 [F \"t1\"]);";
    // programm, model, formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsString);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> mdp =
        storm::api::buildSparseModel<ValueType>(program, formulas)->template as<storm::models::sparse::Mdp<ValueType>>();
    std::string errorString;  // storage for error reporting
    std::vector<Point> expected;

    storm::Environment env = this->getPositionalDeterministicEnvironment();

    uint64_t formulaIndex = 0;
    expected = this->parsePoints({"0.5,0.5", "1,1"});
    if (this->isFlowEncoding()) {
        STORM_SILENT_EXPECT_THROW(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString),
                                  storm::exceptions::InvalidOperationException);
    } else {
        EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;
    }

    ++formulaIndex;
    expected = this->parsePoints({"0,0"});
    EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;

    ++formulaIndex;
    expected = this->parsePoints({"0,1"});
    if (this->isFlowEncoding()) {
        STORM_SILENT_EXPECT_THROW(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString),
                                  storm::exceptions::InvalidOperationException);
    } else {
        EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;
    }

    ++formulaIndex;
    expected = this->parsePoints({"0,1,0", "2,1,1"});
    if (this->isFlowEncoding()) {
        STORM_SILENT_EXPECT_THROW(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString),
                                  storm::exceptions::InvalidOperationException);
    } else {
        EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;
    }
    ++formulaIndex;
    {
        if (this->isFlowEncoding()) {
            STORM_SILENT_EXPECT_THROW(
                storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[formulaIndex]->asMultiObjectiveFormula()),
                storm::exceptions::InvalidOperationException);
        } else {
            auto result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[formulaIndex]->asMultiObjectiveFormula());
            ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
            EXPECT_TRUE(result->template asExplicitQualitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()]);
        }
    }
    ++formulaIndex;
    {
        if (this->isFlowEncoding()) {
            STORM_SILENT_EXPECT_THROW(
                storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[formulaIndex]->asMultiObjectiveFormula()),
                storm::exceptions::InvalidOperationException);
        } else {
            auto result = storm::modelchecker::multiobjective::performMultiObjectiveModelChecking(env, *mdp, formulas[formulaIndex]->asMultiObjectiveFormula());
            ASSERT_TRUE(result->isExplicitQualitativeCheckResult());
            EXPECT_FALSE(result->template asExplicitQualitativeCheckResult<ValueType>()[*mdp->getInitialStates().begin()]);
        }
    }
}

TYPED_TEST(MultiObjectiveSchedRestModelCheckerTest, compromise) {
    typedef typename TestFixture::ValueType ValueType;
    typedef typename TestFixture::Point Point;
    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_compromise.nm";
    std::string constantsString = "";
    std::string formulasAsString = "multi(Pmax=? [ F x=1], Pmax=? [ F x=2 ]);";
    formulasAsString += "\nmulti(R{\"test\"}min=? [F x=1], Pmax=? [F x=1 ]);";
    formulasAsString += "\nmulti(Pmax=? [F x=1], Pmax=? [F x=2 ], Pmax=? [F x=3 ]);";
    formulasAsString += "\nmulti(R{\"test\"}min=? [C], Pmin=? [F x=2 ], Pmin=? [F x=3]);";
    // programm, model, formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsString);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> mdp =
        storm::api::buildSparseModel<ValueType>(program, formulas)->template as<storm::models::sparse::Mdp<ValueType>>();
    std::string errorString;  // storage for error reporting
    std::vector<Point> expected;

    storm::Environment env = this->getPositionalDeterministicEnvironment();

    uint64_t formulaIndex = 0;
    expected = this->parsePoints({"1,0", "0,1", "0.3,3/7"});
    if (this->isFlowEncoding()) {
        STORM_SILENT_EXPECT_THROW(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString),
                                  storm::exceptions::InvalidOperationException);
    } else {
        EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;
    }

    ++formulaIndex;
    expected = this->parsePoints({"0,0.3", "2,1"});
    EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;

    ++formulaIndex;
    expected = this->parsePoints({"1,0,0", "0,1,0", "0.3,3/7,4/7"});
    if (this->isFlowEncoding()) {
        STORM_SILENT_EXPECT_THROW(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString),
                                  storm::exceptions::InvalidOperationException);
    } else {
        EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;
    }

    ++formulaIndex;
    expected = this->parsePoints({"0,1,0", "0,3/7,4/7"});
    EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;

    env = this->getGoalDeterministicEnvironment();

    formulaIndex = 0;
    expected = this->parsePoints({"1,1"});
    EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;

    ++formulaIndex;
    expected = this->parsePoints({"0,0.3", "2,1"});
    EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;

    ++formulaIndex;
    expected = this->parsePoints({"1,1,0", "1,3/7,4/7"});
    EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;

    ++formulaIndex;
    expected = this->parsePoints({"0,1,0", "0,3/7,4/7"});
    EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;
}

TYPED_TEST(MultiObjectiveSchedRestModelCheckerTest, infrew) {
    typedef typename TestFixture::ValueType ValueType;
    typedef typename TestFixture::Point Point;
    std::string programFile = STORM_TEST_RESOURCES_DIR "/mdp/multiobj_infrew.nm";
    std::string constantsString = "";
    std::string formulasAsString = "multi(R{\"one\"}min=? [ F x=2], R{\"two\"}min=? [ C ]);";
    // programm, model, formula
    storm::prism::Program program = storm::api::parseProgram(programFile);
    program = program.preprocess(constantsString);
    std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
        storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasAsString, program));
    std::shared_ptr<storm::models::sparse::Mdp<ValueType>> mdp =
        storm::api::buildSparseModel<ValueType>(program, formulas)->template as<storm::models::sparse::Mdp<ValueType>>();
    std::string errorString;  // storage for error reporting
    std::vector<Point> expected;

    storm::Environment env = this->getPositionalDeterministicEnvironment();

    uint64_t formulaIndex = 0;
    expected = this->parsePoints({"10,1"});
    if (this->isFlowEncoding()) {
        STORM_SILENT_EXPECT_THROW(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString),
                                  storm::exceptions::InvalidOperationException);
    } else {
        EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;
    }

    env = this->getGoalDeterministicEnvironment();

    formulaIndex = 0;
    expected = this->parsePoints({"0,1"});
    EXPECT_TRUE(this->testParetoFormula(env, mdp, *formulas[formulaIndex], expected, errorString)) << errorString;
}

}  // namespace
