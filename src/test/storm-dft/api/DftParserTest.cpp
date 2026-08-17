#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-dft/api/storm-dft.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/WrongFormatException.h"

namespace {

TEST(DftParserTest, LoadFromGalileoFile) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/and.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(file);
    EXPECT_EQ(3ul, dft->nrElements());
    EXPECT_EQ(2ul, dft->nrBasicElements());
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
}

TEST(DftParserTest, LoadFromJsonFile) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/and.json";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTJsonFile<double>(file);
    EXPECT_EQ(3ul, dft->nrElements());
    EXPECT_EQ(2ul, dft->nrBasicElements());
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
}

TEST(DftParserTest, LoadAllBeDistributionsFromGalileoFile) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/all_be_distributions.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(file);
    EXPECT_EQ(8ul, dft->nrElements());
    EXPECT_EQ(7ul, dft->nrBasicElements());
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft, false).first);
    EXPECT_EQ(storm::dft::storage::elements::BEType::CONSTANT, dft->getBasicElement(dft->getIndex("A"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::CONSTANT, dft->getBasicElement(dft->getIndex("B"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::PROBABILITY, dft->getBasicElement(dft->getIndex("C"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::EXPONENTIAL, dft->getBasicElement(dft->getIndex("D"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::ERLANG, dft->getBasicElement(dft->getIndex("E"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::LOGNORMAL, dft->getBasicElement(dft->getIndex("F"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::WEIBULL, dft->getBasicElement(dft->getIndex("G"))->beType());
}

TEST(DftParserTest, LoadAllGatesFromGalileoFile) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/all_gates.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(file);
    EXPECT_EQ(42ul, dft->nrElements());
    EXPECT_EQ(19ul, dft->nrBasicElements());
    EXPECT_EQ(5ul, dft->nrStaticElements());
    EXPECT_EQ(18ul, dft->nrDynamicElements());
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
}

TEST(DftParserTest, LoadAllBEDistributionsFromJsonFile) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/all_be_distributions.json";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTJsonFile<double>(file);
    EXPECT_EQ(8ul, dft->nrElements());
    EXPECT_EQ(7ul, dft->nrBasicElements());
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft, false).first);
    EXPECT_EQ(storm::dft::storage::elements::BEType::CONSTANT, dft->getBasicElement(dft->getIndex("A"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::CONSTANT, dft->getBasicElement(dft->getIndex("B"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::PROBABILITY, dft->getBasicElement(dft->getIndex("C"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::EXPONENTIAL, dft->getBasicElement(dft->getIndex("D"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::ERLANG, dft->getBasicElement(dft->getIndex("E"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::LOGNORMAL, dft->getBasicElement(dft->getIndex("F"))->beType());
    EXPECT_EQ(storm::dft::storage::elements::BEType::WEIBULL, dft->getBasicElement(dft->getIndex("G"))->beType());
}

TEST(DftParserTest, LoadAllGatesFromJsonFile) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/all_gates.json";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTJsonFile<double>(file);
    EXPECT_EQ(42ul, dft->nrElements());
    EXPECT_EQ(19ul, dft->nrBasicElements());
    EXPECT_EQ(5ul, dft->nrStaticElements());
    EXPECT_EQ(18ul, dft->nrDynamicElements());
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
}

TEST(DftParserTest, CatchCycles) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/cyclic.dft";
    STORM_SILENT_EXPECT_THROW(storm::dft::api::loadDFTGalileoFile<double>(file), storm::exceptions::WrongFormatException);
}

TEST(DftParserTest, LoadSeqChildren) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/seqChild.dft";
    std::shared_ptr<storm::dft::storage::DFT<double>> dft = storm::dft::api::loadDFTGalileoFile<double>(file);
    EXPECT_EQ(4ul, dft->nrElements());
    EXPECT_EQ(2ul, dft->nrBasicElements());
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
}

TEST(DftParserTest, LoadParametricFromGalileoFile) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/and_param.dft";
    std::shared_ptr<storm::dft::storage::DFT<storm::RationalFunction>> dft = storm::dft::api::loadDFTGalileoFile<storm::RationalFunction>(file);
    EXPECT_EQ(3ul, dft->nrElements());
    EXPECT_EQ(2ul, dft->nrBasicElements());
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
    auto parameters = storm::dft::storage::getParameters(*dft);
    EXPECT_TRUE(parameters.size() == 1);
    auto it = std::find_if(parameters.begin(), parameters.end(), [](storm::RationalFunctionVariable const& x) { return x.name() == "x"; });
    EXPECT_TRUE(it != parameters.end());
}

TEST(DftParserTest, LoadParametricFromJsonFile) {
    std::string file = STORM_TEST_RESOURCES_DIR "/dft/and_param.json";
    std::shared_ptr<storm::dft::storage::DFT<storm::RationalFunction>> dft = storm::dft::api::loadDFTJsonFile<storm::RationalFunction>(file);
    EXPECT_EQ(3ul, dft->nrElements());
    EXPECT_EQ(2ul, dft->nrBasicElements());
    EXPECT_TRUE(storm::dft::api::isWellFormed(*dft).first);
    auto parameters = storm::dft::storage::getParameters(*dft);
    EXPECT_TRUE(parameters.size() == 1);
    auto it = std::find_if(parameters.begin(), parameters.end(), [](storm::RationalFunctionVariable const& x) { return x.name() == "x"; });
    EXPECT_TRUE(it != parameters.end());
}
}  // namespace
