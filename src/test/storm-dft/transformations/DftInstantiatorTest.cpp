#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/storm-dft.h"
#include "storm-dft/transformations/DftInstantiator.h"
#include "storm/adapters/RationalFunctionAdapter.h"

namespace {

TEST(DftInstantiatorTest, InstantiateSimple) {
    carl::VariablePool::getInstance().clear();

    std::string file = STORM_TEST_RESOURCES_DIR "/dft/and_param.dft";
    std::shared_ptr<storm::dft::storage::DFT<storm::RationalFunction>> dft = storm::dft::api::loadDFTGalileoFile<storm::RationalFunction>(file);
    EXPECT_EQ(3ul, dft->nrElements());
    EXPECT_EQ(2ul, dft->nrBasicElements());

    storm::dft::transformations::DftInstantiator<storm::RationalFunction, double> instantiator(*dft);

    std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> valuation;
    storm::RationalFunctionVariable const& x = carl::VariablePool::getInstance().findVariableWithName("x");
    ASSERT_NE(x, carl::Variable::NO_VARIABLE);

    valuation.insert(std::make_pair(x, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.5)));
    std::shared_ptr<storm::dft::storage::DFT<double>> instDft = instantiator.instantiate(valuation);
    std::shared_ptr<storm::dft::storage::elements::DFTBE<double> const> elem = instDft->getBasicElement(dft->getIndex("C"));
    auto beExp = std::static_pointer_cast<storm::dft::storage::elements::BEExponential<double> const>(elem);
    EXPECT_EQ(beExp->activeFailureRate(), 0.5);

    valuation.clear();
    valuation.insert(std::make_pair(x, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(1.5)));
    instDft = instantiator.instantiate(valuation);
    elem = instDft->getBasicElement(dft->getIndex("C"));
    beExp = std::static_pointer_cast<storm::dft::storage::elements::BEExponential<double> const>(elem);
    EXPECT_EQ(beExp->activeFailureRate(), 1.5);
}

TEST(DftInstantiatorTest, InstantiateSymmetry) {
    carl::VariablePool::getInstance().clear();

    std::string file = STORM_TEST_RESOURCES_DIR "/dft/symmetry_param.dft";
    std::shared_ptr<storm::dft::storage::DFT<storm::RationalFunction>> dft = storm::dft::api::loadDFTGalileoFile<storm::RationalFunction>(file);
    EXPECT_EQ(7ul, dft->nrElements());
    EXPECT_EQ(4ul, dft->nrBasicElements());

    storm::dft::transformations::DftInstantiator<storm::RationalFunction, double> instantiator(*dft);

    std::map<storm::RationalFunctionVariable, storm::RationalFunctionCoefficient> valuation;
    storm::RationalFunctionVariable const& x = carl::VariablePool::getInstance().findVariableWithName("x");
    ASSERT_NE(x, carl::Variable::NO_VARIABLE);
    storm::RationalFunctionVariable const& y = carl::VariablePool::getInstance().findVariableWithName("y");
    ASSERT_NE(y, carl::Variable::NO_VARIABLE);

    valuation.insert(std::make_pair(x, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(5)));
    valuation.insert(std::make_pair(y, storm::utility::convertNumber<storm::RationalFunctionCoefficient>(0.01)));
    std::shared_ptr<storm::dft::storage::DFT<double>> instDft = instantiator.instantiate(valuation);
    std::shared_ptr<storm::dft::storage::elements::DFTBE<double> const> elem = instDft->getBasicElement(dft->getIndex("C"));
    auto beExp = std::static_pointer_cast<storm::dft::storage::elements::BEExponential<double> const>(elem);
    EXPECT_EQ(beExp->activeFailureRate(), 5);
    EXPECT_EQ(beExp->passiveFailureRate(), 0.05);
    elem = instDft->getBasicElement(dft->getIndex("D"));
    beExp = std::static_pointer_cast<storm::dft::storage::elements::BEExponential<double> const>(elem);
    EXPECT_EQ(beExp->activeFailureRate(), 0.01);
}
}  // namespace
