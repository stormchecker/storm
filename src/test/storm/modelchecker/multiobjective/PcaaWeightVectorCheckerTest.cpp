#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/api/storm-parsers.h"
#include "storm/api/storm.h"
#include "storm/environment/solver/SolverEnvironment.h"
#include "storm/modelchecker/multiobjective/pcaa/PcaaWeightVectorChecker.h"
#include "storm/modelchecker/multiobjective/preprocessing/SparseMultiObjectivePreprocessor.h"
#include "storm/utility/vector.h"

namespace {

class DoubleUnsoundEnvironment {
   public:
    using ValueType = double;
    static ValueType precision() {
        return 1e-6;
    }
    static storm::Environment getEnv() {
        storm::Environment env;
        env.solver().setForceSoundness(false);
        env.solver().setForceExact(false);
        return env;
    }
};

class DoubleSoundEnvironment {
   public:
    using ValueType = double;
    static ValueType precision() {
        return 1e-6;
    }
    static storm::Environment getEnv() {
        storm::Environment env;
        env.solver().setForceSoundness(true);
        env.solver().setForceExact(false);
        return env;
    }
};

class RationalExactEnvironment {
   public:
    using ValueType = storm::RationalNumber;
    static ValueType precision() {
        return storm::utility::zero<ValueType>();
    }
    static storm::Environment getEnv() {
        storm::Environment env;
        env.solver().setForceSoundness(false);
        env.solver().setForceExact(true);
        return env;
    }
};

template<typename TestType>
class PcaaWeightVectorCheckerTest : public ::testing::Test {
   public:
    using ValueType = typename TestType::ValueType;

    storm::Environment env() const {
        return TestType::getEnv();
    }

    ValueType precision() const {
        return TestType::precision();
    }

    ValueType parseNumber(std::string const& input) const {
        return storm::utility::convertNumber<ValueType>(input);
    }

    std::vector<ValueType> parseVector(std::string const& input) const {
        auto const pos = input.rfind(',');
        if (pos != std::string::npos) {
            auto result = parseVector(input.substr(0, pos));
            result.push_back(parseNumber(input.substr(pos + 1)));
            return result;
        } else {
            return {parseNumber(input)};
        }
    }

    auto getMdpAndFormulasFromPrism(std::string const& prismFileString, std::string const& constantsString, std::string const& formulasString) const {
#ifndef STORM_HAVE_Z3
        GTEST_SKIP() << "Z3 not available.";
#endif
        storm::prism::Program program = storm::api::parseProgram(prismFileString);
        program = storm::utility::prism::preprocess(program, constantsString);
        std::vector<std::shared_ptr<storm::logic::Formula const>> formulas =
            storm::api::extractFormulasFromProperties(storm::api::parsePropertiesForPrismProgram(formulasString, program));
        std::shared_ptr<storm::models::sparse::Mdp<ValueType>> mdp =
            storm::api::buildSparseModel<ValueType>(program, formulas)->template as<storm::models::sparse::Mdp<ValueType>>();
        return std::pair(std::move(mdp), std::move(formulas));
    }

    void testWeightVectorCheck(auto const& wvChecker, std::vector<ValueType> const& weightVector, std::vector<ValueType> const& expectedPoint,
                               ValueType const& expectedSum) {
        wvChecker->setWeightedPrecision(this->precision());
        ValueType const wvLength = storm::utility::sqrt(storm::utility::vector::dotProduct(weightVector, weightVector));
        ValueType const wvAbsSum = std::accumulate(weightVector.begin(), weightVector.end(), storm::utility::zero<ValueType>(),
                                                   [](ValueType acc, ValueType w) { return acc + storm::utility::abs(w); });

        EXPECT_NO_THROW(wvChecker->check(this->env(), weightVector));
        EXPECT_EQ(weightVector.size(), expectedPoint.size());
        // Check point
        auto const point = wvChecker->getAchievablePoint();
        for (uint64_t i = 0; i < expectedPoint.size(); ++i) {
            if (storm::utility::isZero(weightVector[i])) {
                continue;
            }
            // Dimensions with relatively low weight don't require as much precision.
            ValueType const prec = this->precision() / (storm::utility::abs(weightVector[i]) / wvLength);
            EXPECT_NEAR(expectedPoint[i], point[i], prec)
                << "Found point " << storm::utility::vector::toString(point) << " for weight vector " << storm::utility::vector::toString(weightVector)
                << "does not match expected point " << storm::utility::vector::toString(expectedPoint) << ".\n"
                << "Missmatch in dimension " << i << " with weight " << weightVector[i] << " and precision " << prec << ".";
        }
        // Check weighted sum
        auto const prec = this->precision() * wvLength;
        EXPECT_NEAR(expectedSum, wvChecker->getOptimalWeightedSum(), prec);
        // Check if scheduler compuatation finishes without throwing.
        EXPECT_NO_THROW(wvChecker->computeScheduler());
    }
};

typedef ::testing::Types<DoubleUnsoundEnvironment, DoubleSoundEnvironment, RationalExactEnvironment> TestingTypes;
TYPED_TEST_SUITE(PcaaWeightVectorCheckerTest, TestingTypes, );

TYPED_TEST(PcaaWeightVectorCheckerTest, oneDimWalk) {
    auto [mdp, formulas] = this->getMdpAndFormulasFromPrism(STORM_TEST_RESOURCES_DIR "/mdp/one_dim_walk.nm", "N=2",
                                                            "multi(R{\"l\"}min=? [F x=N | x=0 ], R{\"r\"}min=? [F x=N | x=0 ]);");
    using MdpType = typename std::remove_reference_t<decltype(*mdp)>;
    auto const preprocessresult = storm::modelchecker::multiobjective::preprocessing::SparseMultiObjectivePreprocessor<MdpType>::preprocess(
        this->env(), *mdp, formulas[0]->asMultiObjectiveFormula(), true);
    auto const wvChecker = storm::modelchecker::multiobjective::createWeightVectorChecker(preprocessresult);
    this->testWeightVectorCheck(wvChecker, this->parseVector("2/3,1/3"), this->parseVector("0,2"), this->parseNumber("-2/3"));
    this->testWeightVectorCheck(wvChecker, this->parseVector("1/4,3/4"), this->parseVector("2,0"), this->parseNumber("-1/2"));
    this->testWeightVectorCheck(wvChecker, this->parseVector("1000,1"), this->parseVector("0,2"), this->parseNumber("-2"));
}

TYPED_TEST(PcaaWeightVectorCheckerTest, two_dice) {
    auto [mdp, formulas] = this->getMdpAndFormulasFromPrism(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm", "", "multi(Tmax=? [F s1=7 ], Tmax=? [F s2=7 ]);");
    using MdpType = typename std::remove_reference_t<decltype(*mdp)>;
    auto const preprocessresult = storm::modelchecker::multiobjective::preprocessing::SparseMultiObjectivePreprocessor<MdpType>::preprocess(
        this->env(), *mdp, formulas[0]->asMultiObjectiveFormula(), true);
    auto const wvChecker = storm::modelchecker::multiobjective::createWeightVectorChecker(preprocessresult);
    this->testWeightVectorCheck(wvChecker, this->parseVector("9/10,1/10"), this->parseVector("22/3,17/3"), this->parseNumber("43/6"));
    this->testWeightVectorCheck(wvChecker, this->parseVector("300,3"), this->parseVector("22/3,17/3"), this->parseNumber("2217"));
    this->testWeightVectorCheck(wvChecker, this->parseVector("-3/4,-1/4"), this->parseVector("11/3,22/3"), this->parseNumber("-55/12"));
    this->testWeightVectorCheck(wvChecker, this->parseVector("3/4,-1/4"), this->parseVector("22/3,11/3"), this->parseNumber("55/12"));
    this->testWeightVectorCheck(wvChecker, this->parseVector("-3/4,1/4"), this->parseVector("11/3,22/3"), this->parseNumber("-11/12"));
}

}  // namespace