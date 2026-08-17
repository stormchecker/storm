#include "storm-config.h"
#include "test/storm_gtest.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-negative"
#pragma clang diagnostic ignored "-Wundefined-reinterpret-cast"
#pragma clang diagnostic ignored "-Wunused-template"
#include <carl/util/stringparser.h>
#pragma clang diagnostic pop

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/environment/Environment.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/settings/SettingsManager.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/dd/Add.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/storage/dd/DdMetaVariable.h"

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
class SylvanDd : public ::testing::Test {
   public:
    void SetUp() override {
        TestType::checkLibraryAvailable();
    }

    storm::Environment env;

    static const storm::dd::DdType DdType = TestType::DdType;
};

typedef ::testing::Types<Sylvan> TestingTypes;
TYPED_TEST_SUITE(SylvanDd, TestingTypes, );

TYPED_TEST(SylvanDd, AddSharpenTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto manager(std::make_shared<storm::dd::DdManager<DdType>>(this->env));
    manager->execute([&]() {
        std::pair<storm::expressions::Variable, storm::expressions::Variable> x = manager->addMetaVariable("x", 1, 9);

        storm::dd::Add<DdType, double> dd = manager->template getAddOne<double>();
        ASSERT_NO_THROW(dd.setValue(x.first, 4, 1.89999999));
        ASSERT_EQ(2ul, dd.getLeafCount());

        storm::dd::Add<DdType, storm::RationalNumber> sharpened = dd.sharpenKwekMehlhorn(1);

        std::map<storm::expressions::Variable, int_fast64_t> metaVariableToValueMap;
        metaVariableToValueMap.emplace(x.first, 4);

        sharpened = dd.sharpenKwekMehlhorn(1);
        ASSERT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("9/5")), sharpened.getValue(metaVariableToValueMap));

        sharpened = dd.sharpenKwekMehlhorn(2);
        ASSERT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("19/10")), sharpened.getValue(metaVariableToValueMap));
    });
}

TYPED_TEST(SylvanDd, AddRationalSharpenTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto manager(std::make_shared<storm::dd::DdManager<DdType>>(this->env));
    manager->execute([&]() {
        std::pair<storm::expressions::Variable, storm::expressions::Variable> x = manager->addMetaVariable("x", 1, 9);

        storm::dd::Add<DdType, storm::RationalNumber> dd = manager->template getAddOne<storm::RationalNumber>();
        ASSERT_NO_THROW(dd.setValue(x.first, 4, storm::utility::convertNumber<storm::RationalNumber>(1.89999999)));
        ASSERT_EQ(2ul, dd.getLeafCount());

        storm::dd::Add<DdType, storm::RationalNumber> sharpened = dd.sharpenKwekMehlhorn(1);

        std::map<storm::expressions::Variable, int_fast64_t> metaVariableToValueMap;
        metaVariableToValueMap.emplace(x.first, 4);

        sharpened = dd.sharpenKwekMehlhorn(1);
        ASSERT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("9/5")), sharpened.getValue(metaVariableToValueMap));

        sharpened = dd.sharpenKwekMehlhorn(2);
        ASSERT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("19/10")), sharpened.getValue(metaVariableToValueMap));
    });
}

TYPED_TEST(SylvanDd, AddToRationalTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto manager(std::make_shared<storm::dd::DdManager<DdType>>(this->env));
    manager->execute([&]() {
        std::pair<storm::expressions::Variable, storm::expressions::Variable> x = manager->addMetaVariable("x", 1, 9);

        storm::dd::Add<DdType, double> dd = manager->template getAddOne<double>();
        ASSERT_NO_THROW(dd.setValue(x.first, 4, 0.4));
        ASSERT_EQ(2ul, dd.getLeafCount());

        storm::dd::Add<DdType, storm::RationalNumber> rationalDd = dd.template toValueType<storm::RationalNumber>();

        std::map<storm::expressions::Variable, int_fast64_t> metaVariableToValueMap;
        metaVariableToValueMap.emplace(x.first, 4);

        ASSERT_EQ(storm::utility::convertNumber<storm::RationalNumber>(std::string("3602879701896397/9007199254740992")),
                  rationalDd.getValue(metaVariableToValueMap));
    });
}

TYPED_TEST(SylvanDd, RationalFunctionConstants) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto manager(std::make_shared<storm::dd::DdManager<DdType>>(this->env));
    manager->execute([&]() {
        storm::dd::Add<DdType, storm::RationalFunction> zero;
        ASSERT_NO_THROW(zero = manager->template getAddZero<storm::RationalFunction>());

        EXPECT_EQ(0ul, zero.getNonZeroCount());
        EXPECT_EQ(1ul, zero.getLeafCount());
        EXPECT_EQ(1ul, zero.getNodeCount());

        storm::dd::Add<DdType, storm::RationalFunction> one;
        ASSERT_NO_THROW(one = manager->template getAddOne<storm::RationalFunction>());

        EXPECT_EQ(0ul, one.getNonZeroCount());
        EXPECT_EQ(1ul, one.getLeafCount());
        EXPECT_EQ(1ul, one.getNodeCount());

        storm::dd::Add<DdType, storm::RationalFunction> two;
        storm::RationalFunction constantTwo(2);

        ASSERT_NO_THROW(two = manager->template getConstant<storm::RationalFunction>(constantTwo));

        EXPECT_EQ(0ul, two.getNonZeroCount());
        EXPECT_EQ(1ul, two.getLeafCount());
        EXPECT_EQ(1ul, two.getNodeCount());

        storm::dd::Add<DdType, storm::RationalFunction> function;
        std::shared_ptr<storm::RawPolynomialCache> cache = std::make_shared<storm::RawPolynomialCache>();
        carl::StringParser parser;
        parser.setVariables({"x", "y", "z"});

        storm::RationalFunction partA =
            storm::RationalFunction(storm::Polynomial(parser.template parseMultivariatePolynomial<storm::RationalFunctionCoefficient>("2*x+x*y"), cache));
        storm::RationalFunction partB =
            storm::RationalFunction(storm::Polynomial(parser.template parseMultivariatePolynomial<storm::RationalFunctionCoefficient>("z"), cache),
                                    storm::Polynomial(parser.template parseMultivariatePolynomial<storm::RationalFunctionCoefficient>("2*y"), cache));

        storm::RationalFunction rationalFunction = storm::RationalFunction(partA + partB);

        ASSERT_NO_THROW(function = manager->template getConstant<storm::RationalFunction>(rationalFunction));

        EXPECT_EQ(0ul, function.getNonZeroCount());
        EXPECT_EQ(1ul, function.getLeafCount());
        EXPECT_EQ(1ul, function.getNodeCount());
    });
}

TYPED_TEST(SylvanDd, RationalFunctionToDouble) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto manager(std::make_shared<storm::dd::DdManager<DdType>>(this->env));

    manager->execute([&]() {
        std::pair<storm::expressions::Variable, storm::expressions::Variable> xExpr;
        std::pair<storm::expressions::Variable, storm::expressions::Variable> yExpr;
        std::pair<storm::expressions::Variable, storm::expressions::Variable> zExpr;
        ASSERT_NO_THROW(xExpr = manager->addMetaVariable("x", 0, 1));
        ASSERT_NO_THROW(yExpr = manager->addMetaVariable("y", 0, 1));
        ASSERT_NO_THROW(zExpr = manager->addMetaVariable("z", 0, 1));

        storm::dd::Bdd<DdType> bddX0 = manager->getEncoding(xExpr.first, 0);
        storm::dd::Bdd<DdType> bddX1 = manager->getEncoding(xExpr.first, 1);
        storm::dd::Bdd<DdType> bddY0 = manager->getEncoding(yExpr.first, 0);
        storm::dd::Bdd<DdType> bddY1 = manager->getEncoding(yExpr.first, 1);
        storm::dd::Bdd<DdType> bddZ0 = manager->getEncoding(zExpr.first, 0);
        storm::dd::Bdd<DdType> bddZ1 = manager->getEncoding(zExpr.first, 1);

        storm::dd::Add<DdType, storm::RationalFunction> complexAdd =
            ((bddX0 && (bddY0 && bddZ0)).template toAdd<storm::RationalFunction>() *
             manager->template getConstant<storm::RationalFunction>(storm::RationalFunction(storm::RationalFunctionCoefficient(-1)))) +
            ((bddX0 && (bddY0 && bddZ1)).template toAdd<storm::RationalFunction>() *
             manager->template getConstant<storm::RationalFunction>(storm::RationalFunction(storm::RationalFunctionCoefficient(0)))) +
            ((bddX0 && (bddY1 && bddZ0)).template toAdd<storm::RationalFunction>() *
             manager->template getConstant<storm::RationalFunction>(
                 storm::RationalFunction(storm::RationalFunctionCoefficient(1) / storm::RationalFunctionCoefficient(2)))) +
            ((bddX0 && (bddY1 && bddZ1)).template toAdd<storm::RationalFunction>() *
             manager->template getConstant<storm::RationalFunction>(
                 storm::RationalFunction(storm::RationalFunctionCoefficient(1) / storm::RationalFunctionCoefficient(3)))) +
            ((bddX1 && (bddY0 && bddZ0)).template toAdd<storm::RationalFunction>() *
             manager->template getConstant<storm::RationalFunction>(storm::RationalFunction(storm::RationalFunctionCoefficient(100000)))) +
            ((bddX1 && (bddY0 && bddZ1)).template toAdd<storm::RationalFunction>() *
             manager->template getConstant<storm::RationalFunction>(storm::RationalFunction(storm::RationalFunctionCoefficient(3)))) +
            ((bddX1 && (bddY1 && bddZ0)).template toAdd<storm::RationalFunction>() *
             manager->template getConstant<storm::RationalFunction>(storm::RationalFunction(storm::RationalFunctionCoefficient(4)))) +
            ((bddX1 && (bddY1 && bddZ1)).template toAdd<storm::RationalFunction>() *
             manager->template getConstant<storm::RationalFunction>(storm::RationalFunction(storm::RationalFunctionCoefficient(0))));
        EXPECT_EQ(6ul, complexAdd.getNonZeroCount());
        EXPECT_EQ(7ul, complexAdd.getLeafCount());
        EXPECT_EQ(14ul, complexAdd.getNodeCount());

        storm::dd::Add<DdType, double> doubleAdd = complexAdd.template toValueType<double>();

        EXPECT_EQ(6ul, doubleAdd.getNonZeroCount());
        EXPECT_EQ(7ul, doubleAdd.getLeafCount());
        EXPECT_EQ(14ul, doubleAdd.getNodeCount());

        storm::dd::Add<DdType, double> comparisonAdd =
            ((bddX0 && (bddY0 && bddZ0)).template toAdd<double>() * manager->template getConstant<double>(-1.0)) +
            ((bddX0 && (bddY0 && bddZ1)).template toAdd<double>() * manager->template getConstant<double>(0.0)) +
            ((bddX0 && (bddY1 && bddZ0)).template toAdd<double>() * manager->template getConstant<double>(0.5)) +
            ((bddX0 && (bddY1 && bddZ1)).template toAdd<double>() * manager->template getConstant<double>(0.33333333333333333333)) +
            ((bddX1 && (bddY0 && bddZ0)).template toAdd<double>() * manager->template getConstant<double>(100000.0)) +
            ((bddX1 && (bddY0 && bddZ1)).template toAdd<double>() * manager->template getConstant<double>(3.0)) +
            ((bddX1 && (bddY1 && bddZ0)).template toAdd<double>() * manager->template getConstant<double>(4.0)) +
            ((bddX1 && (bddY1 && bddZ1)).template toAdd<double>() * manager->template getConstant<double>(0.0));

        EXPECT_TRUE(comparisonAdd == doubleAdd);
    });
}

TYPED_TEST(SylvanDd, RationalFunctionEncodingTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto manager(std::make_shared<storm::dd::DdManager<DdType>>(this->env));
    manager->execute([&]() {
        std::pair<storm::expressions::Variable, storm::expressions::Variable> x = manager->addMetaVariable("x", 1, 9);

        storm::dd::Bdd<DdType> encoding;
        STORM_SILENT_ASSERT_THROW(encoding = manager->getEncoding(x.first, 0), storm::exceptions::InvalidArgumentException);
        STORM_SILENT_ASSERT_THROW(encoding = manager->getEncoding(x.first, 10), storm::exceptions::InvalidArgumentException);
        ASSERT_NO_THROW(encoding = manager->getEncoding(x.first, 4));
        EXPECT_EQ(1ul, encoding.getNonZeroCount());

        // As a BDD, this DD has one only leaf, because there does not exist a 0-leaf, and (consequently) one node less
        // than the MTBDD.
        EXPECT_EQ(5ul, encoding.getNodeCount());
        EXPECT_EQ(1ul, encoding.getLeafCount());

        storm::dd::Add<DdType, storm::RationalFunction> add;
        ASSERT_NO_THROW(add = encoding.template toAdd<storm::RationalFunction>());

        // As an MTBDD, the 0-leaf is there, so the count is actually 2 and the node count is 6.
        EXPECT_EQ(6ul, add.getNodeCount());
        EXPECT_EQ(2ul, add.getLeafCount());
    });
}

TYPED_TEST(SylvanDd, RationalFunctionIdentityTest) {
    const storm::dd::DdType DdType = TestFixture::DdType;
    auto manager(std::make_shared<storm::dd::DdManager<DdType>>(this->env));
    manager->execute([&]() {
        std::pair<storm::expressions::Variable, storm::expressions::Variable> x = manager->addMetaVariable("x", 1, 9);

        storm::dd::Add<DdType, storm::RationalFunction> identity;
        ASSERT_NO_THROW(identity = manager->template getIdentity<storm::RationalFunction>(x.first));

        EXPECT_EQ(9ul, identity.getNonZeroCount());
        EXPECT_EQ(10ul, identity.getLeafCount());
        EXPECT_EQ(21ul, identity.getNodeCount());
    });
}
