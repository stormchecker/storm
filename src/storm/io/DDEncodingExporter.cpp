#include "storm/io/DDEncodingExporter.h"

#include "storm/io/file.h"
#include "storm/models/symbolic/StandardRewardModel.h"

namespace storm {
namespace io {

template<storm::dd::DdType Type, typename ValueType>
void explicitExportSymbolicModel(std::string const& filename, std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> symbolicModel) {
    std::ofstream filestream;
    storm::io::openFile(filename, filestream);
    filestream << "// storm exported dd\n";
    filestream << "%transitions\n";
    storm::io::closeFile(filestream);
    symbolicModel->getTransitionMatrix().exportToText(filename);
    storm::io::openFile(filename, filestream, true, true);
    filestream << "%initial\n";
    storm::io::closeFile(filestream);
    symbolicModel->getInitialStates().template toAdd<ValueType>().exportToText(filename);
    for (auto const& label : symbolicModel->getLabels()) {
        storm::io::openFile(filename, filestream, true, true);
        filestream << "\n%label " << label << '\n';
        storm::io::closeFile(filestream);
        symbolicModel->getStates(label).template toAdd<ValueType>().exportToText(filename);
    }
}

template void explicitExportSymbolicModel<storm::dd::DdType::CUDD, double>(
    std::string const&, std::shared_ptr<storm::models::symbolic::Model<storm::dd::DdType::CUDD, double>> sparseModel);
template void explicitExportSymbolicModel<storm::dd::DdType::Sylvan, double>(
    std::string const&, std::shared_ptr<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, double>> sparseModel);

template void explicitExportSymbolicModel<storm::dd::DdType::Sylvan, storm::RationalNumber>(
    std::string const&, std::shared_ptr<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalNumber>> sparseModel);
template void explicitExportSymbolicModel<storm::dd::DdType::Sylvan, storm::RationalFunction>(
    std::string const&, std::shared_ptr<storm::models::symbolic::Model<storm::dd::DdType::Sylvan, storm::RationalFunction>> sparseModel);
}  // namespace io
}  // namespace storm
