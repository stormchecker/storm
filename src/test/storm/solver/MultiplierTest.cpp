#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm/environment/solver/MultiplierEnvironment.h"
#include "storm/solver/multiplier/Multiplier.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/vector.h"

namespace {

class NativeEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().multiplier().setType(storm::solver::MultiplierType::Native);
        return env;
    }
};

class ViOperatorEnvironment {
   public:
    typedef double ValueType;
    static const bool isExact = false;
    static storm::Environment createEnvironment() {
        storm::Environment env;
        env.solver().multiplier().setType(storm::solver::MultiplierType::ViOperator);
        return env;
    }
};

template<typename TestType>
class MultiplierTest : public ::testing::Test {
   public:
    typedef typename TestType::ValueType ValueType;
    MultiplierTest() : _environment(TestType::createEnvironment()) {}
    storm::Environment const& env() const {
        return _environment;
    }
    ValueType precision() const {
        return TestType::isExact ? parseNumber("0") : parseNumber("1e-15");
    }
    ValueType parseNumber(std::string const& input) const {
        return storm::utility::convertNumber<ValueType>(input);
    }

   private:
    storm::Environment _environment;
};

typedef ::testing::Types<NativeEnvironment, ViOperatorEnvironment> TestingTypes;

TYPED_TEST_SUITE(MultiplierTest, TestingTypes, );

TYPED_TEST(MultiplierTest, repeatedMultiplyTest) {
    typedef typename TestFixture::ValueType ValueType;
    ASSERT_NO_THROW(storm::storage::SparseMatrixBuilder<ValueType> builder);
    storm::storage::SparseMatrixBuilder<ValueType> builder;
    ASSERT_NO_THROW(builder.addNextValue(0, 1, this->parseNumber("0.5")));
    ASSERT_NO_THROW(builder.addNextValue(0, 4, this->parseNumber("0.5")));
    ASSERT_NO_THROW(builder.addNextValue(1, 2, this->parseNumber("0.5")));
    ASSERT_NO_THROW(builder.addNextValue(1, 4, this->parseNumber("0.5")));
    ASSERT_NO_THROW(builder.addNextValue(2, 3, this->parseNumber("0.5")));
    ASSERT_NO_THROW(builder.addNextValue(2, 4, this->parseNumber("0.5")));
    ASSERT_NO_THROW(builder.addNextValue(3, 4, this->parseNumber("1")));
    ASSERT_NO_THROW(builder.addNextValue(4, 4, this->parseNumber("1")));

    storm::storage::SparseMatrix<ValueType> A;
    ASSERT_NO_THROW(A = builder.build());

    std::vector<ValueType> x(5);
    x[4] = this->parseNumber("1");

    auto factory = storm::solver::MultiplierFactory<ValueType>();
    auto multiplier = factory.create(this->env(), A);
    ASSERT_NO_THROW(multiplier->repeatedMultiply(this->env(), x, nullptr, 4));
    EXPECT_NEAR(x[0], this->parseNumber("1"), this->precision());
}

TYPED_TEST(MultiplierTest, repeatedMultiplyAndReduceTest) {
    typedef typename TestFixture::ValueType ValueType;

    storm::storage::SparseMatrixBuilder<ValueType> builder(0, 0, 0, false, true);
    ASSERT_NO_THROW(builder.newRowGroup(0));
    ASSERT_NO_THROW(builder.addNextValue(0, 0, this->parseNumber("0.9")));
    ASSERT_NO_THROW(builder.addNextValue(0, 1, this->parseNumber("0.099")));
    ASSERT_NO_THROW(builder.addNextValue(0, 2, this->parseNumber("0.001")));
    ASSERT_NO_THROW(builder.addNextValue(1, 1, this->parseNumber("0.5")));
    ASSERT_NO_THROW(builder.addNextValue(1, 2, this->parseNumber("0.5")));
    ASSERT_NO_THROW(builder.newRowGroup(2));
    ASSERT_NO_THROW(builder.addNextValue(2, 1, this->parseNumber("1")));
    ASSERT_NO_THROW(builder.newRowGroup(3));
    ASSERT_NO_THROW(builder.addNextValue(3, 2, this->parseNumber("1")));

    storm::storage::SparseMatrix<ValueType> A;
    ASSERT_NO_THROW(A = builder.build());

    std::vector<ValueType> initialX = {this->parseNumber("0"), this->parseNumber("1"), this->parseNumber("0")};
    std::vector<ValueType> x;

    auto factory = storm::solver::MultiplierFactory<ValueType>();
    auto multiplier = factory.create(this->env(), A);

    x = initialX;
    ASSERT_NO_THROW(multiplier->repeatedMultiplyAndReduce(this->env(), storm::OptimizationDirection::Minimize, x, nullptr, 1));
    EXPECT_NEAR(x[0], this->parseNumber("0.099"), this->precision());

    x = initialX;
    ASSERT_NO_THROW(multiplier->repeatedMultiplyAndReduce(this->env(), storm::OptimizationDirection::Minimize, x, nullptr, 2));
    EXPECT_NEAR(x[0], this->parseNumber("0.1881"), this->precision());

    x = initialX;
    ASSERT_NO_THROW(multiplier->repeatedMultiplyAndReduce(this->env(), storm::OptimizationDirection::Minimize, x, nullptr, 20));
    EXPECT_NEAR(x[0], this->parseNumber("0.5"), this->precision());

    x = initialX;
    ASSERT_NO_THROW(multiplier->repeatedMultiplyAndReduce(this->env(), storm::OptimizationDirection::Maximize, x, nullptr, 1));
    EXPECT_NEAR(x[0], this->parseNumber("0.5"), this->precision());

    x = initialX;
    ASSERT_NO_THROW(multiplier->repeatedMultiplyAndReduce(this->env(), storm::OptimizationDirection::Maximize, x, nullptr, 20));
    EXPECT_NEAR(x[0], this->parseNumber("0.923808265834023387639"), this->precision());
}

}  // namespace