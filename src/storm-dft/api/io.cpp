#include "io.h"

#include <memory>

#include "storm-dft/modelchecker/DFTASFChecker.h"
#include "storm-dft/parser/DFTGalileoParser.h"
#include "storm-dft/parser/DFTJsonParser.h"
#include "storm-dft/storage/DftJsonExporter.h"

namespace storm::dft {
namespace api {

template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> loadDFTGalileoFile(std::string const& file) {
    return std::make_shared<storm::dft::storage::DFT<ValueType>>(parser::DFTGalileoParser<ValueType>::parseDFT(file));
}

template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> loadDFTJsonString(std::string const& jsonString) {
    return std::make_shared<storm::dft::storage::DFT<ValueType>>(parser::DFTJsonParser<ValueType>::parseJsonFromString(jsonString));
}

template<typename ValueType>
std::shared_ptr<storm::dft::storage::DFT<ValueType>> loadDFTJsonFile(std::string const& file) {
    return std::make_shared<storm::dft::storage::DFT<ValueType>>(storm::dft::parser::DFTJsonParser<ValueType>::parseJsonFromFile(file));
}

template<typename ValueType>
void exportDFTToJsonFile(storm::dft::storage::DFT<ValueType> const& dft, std::string const& file) {
    storm::dft::storage::DftJsonExporter<ValueType>::toFile(dft, file);
}

template<typename ValueType>
std::string exportDFTToJsonString(storm::dft::storage::DFT<ValueType> const& dft) {
    std::stringstream stream;
    storm::dft::storage::DftJsonExporter<ValueType>::toStream(dft, stream);
    return stream.str();
}

template<>
void exportDFTToSMT(storm::dft::storage::DFT<double> const& dft, std::string const& file) {
    storm::dft::modelchecker::DFTASFChecker asfChecker(dft);
    asfChecker.convert();
    asfChecker.toFile(file);
}

template<>
void exportDFTToSMT(storm::dft::storage::DFT<storm::RationalFunction> const& dft, std::string const& file) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Export to SMT does not support this data type.");
}

// Explicitly instantiate methods
template std::shared_ptr<storm::dft::storage::DFT<double>> loadDFTGalileoFile(std::string const&);
template std::shared_ptr<storm::dft::storage::DFT<double>> loadDFTJsonString(std::string const&);
template std::shared_ptr<storm::dft::storage::DFT<double>> loadDFTJsonFile(std::string const&);
template void exportDFTToJsonFile(storm::dft::storage::DFT<double> const&, std::string const&);
template std::string exportDFTToJsonString(storm::dft::storage::DFT<double> const&);

template std::shared_ptr<storm::dft::storage::DFT<storm::RationalFunction>> loadDFTGalileoFile(std::string const&);
template std::shared_ptr<storm::dft::storage::DFT<storm::RationalFunction>> loadDFTJsonString(std::string const&);
template std::shared_ptr<storm::dft::storage::DFT<storm::RationalFunction>> loadDFTJsonFile(std::string const&);
template void exportDFTToJsonFile(storm::dft::storage::DFT<storm::RationalFunction> const&, std::string const&);
template std::string exportDFTToJsonString(storm::dft::storage::DFT<storm::RationalFunction> const&);

}  // namespace api
}  // namespace storm::dft
