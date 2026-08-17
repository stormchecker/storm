#pragma once

#include <filesystem>

#include "storm/adapters/JsonForward.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/io/DDEncodingExporter.h"
#include "storm/io/DirectEncodingExporter.h"
#include "storm/io/file.h"
#include "storm/modelchecker/results/CheckResult.h"
#include "storm/modelchecker/results/ExplicitQualitativeCheckResult.h"
#include "storm/modelchecker/results/ExplicitQuantitativeCheckResult.h"
#include "storm/storage/Scheduler.h"
#include "storm/storage/umb/Umb.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/UnexpectedException.h"

namespace storm {

namespace jani {
class Model;
}

namespace api {

void exportJaniModelAsDot(storm::jani::Model const& model, std::string const& filename);

template<typename ValueType>
void exportSparseModelAsDrn(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, std::string const& filename,
                            std::vector<std::string> const& parameterNames = {}, bool allowPlaceholders = true) {
    storm::io::DirectEncodingExporterOptions options;
    options.allowPlaceholders = allowPlaceholders;
    storm::io::explicitExportSparseModel(filename, model, parameterNames, options);
}

template<typename ValueType>
void exportSparseModelAsDrn(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, std::string const& filename,
                            storm::io::DirectEncodingExporterOptions options, std::vector<std::string> const& parameterNames = {}) {
    storm::io::explicitExportSparseModel(filename, model, parameterNames, options);
}

template<storm::dd::DdType Type, typename ValueType>
void exportSymbolicModelAsDrdd(std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> const& model, std::string const& filename) {
    storm::io::explicitExportSymbolicModel(filename, model);
}

template<typename ValueType>
void exportSparseModelAsDot(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, std::string const& filename, size_t maxWidth = 30) {
    std::ofstream stream;
    storm::io::openFile(filename, stream);
    model->writeDotToStream(stream, maxWidth);
    storm::io::closeFile(stream);
}

template<typename ValueType>
void exportSparseModelAsJson(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, std::string const& filename) {
    std::ofstream stream;
    storm::io::openFile(filename, stream);
    model->writeJsonToStream(stream);
    storm::io::closeFile(stream);
}

template<typename ValueType>
void exportSparseModelAsUmb(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, std::string const& filename,
                            storm::umb::ExportOptions const& options = {}) {
    if constexpr (std::is_same_v<ValueType, storm::RationalFunction>) {
        STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Export of rational functions to UMB is not supported.");
    } else {
        storm::umb::exportModelToUmb(*model, filename, options);
    }
}

template<storm::dd::DdType Type, typename ValueType>
void exportSymbolicModelAsDot(std::shared_ptr<storm::models::symbolic::Model<Type, ValueType>> const& model, std::string const& filename) {
    model->writeDotToFile(filename);
}

template<typename ValueType>
void exportScheduler(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, storm::storage::Scheduler<ValueType> const& scheduler,
                     std::string const& filename) {
    std::ofstream stream;
    storm::io::openFile(filename, stream);
    std::string jsonFileExtension = ".json";
    if (filename.size() > 4 && std::equal(jsonFileExtension.rbegin(), jsonFileExtension.rend(), filename.rbegin())) {
        scheduler.printJsonToStream(stream, model, false, true);
    } else {
        scheduler.printToStream(stream, model, false, true);
    }
    storm::io::closeFile(stream);
}

template<typename ValueType, typename PointType>
void exportParetoScheduler(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model, std::vector<PointType> const& points,
                           std::vector<storm::storage::Scheduler<ValueType>> const& schedulers, std::string const& baseFilenameStr) {
    // We export each scheduler in a separate file, named baseFilename_pointi.ext, where ext is the extension of the baseFilename.
    // Additionally, we create a CSV file that associates each scheduler file with the corresponding point.
    // Note that we cannot directly put the point coordinates into the filename, as some characters (e.g., ',' or '/' (occurring in rational numbers)) are not
    // handled well in filenames.
    std::filesystem::path baseFilename(baseFilenameStr);
    std::ofstream infoStream;
    auto infoFilePath = baseFilename;
    infoFilePath.replace_filename(infoFilePath.stem().string() + "_info.csv");
    storm::io::openFile(infoFilePath, infoStream);
    infoStream << "file;point\n";
    STORM_LOG_THROW(points.size() == schedulers.size(), storm::exceptions::UnexpectedException, "Number of points and schedulers must match.");
    for (uint64_t i = 0; i < points.size(); ++i) {
        std::string schedulerFileName = baseFilename.stem().string() + "_point" + std::to_string(i) + baseFilename.extension().string();
        infoStream << schedulerFileName << ";[";
        bool first = true;
        for (auto const& pointEntry : points[i]) {
            if (!first) {
                infoStream << ",";
            }
            first = false;
            infoStream << pointEntry;
        }
        infoStream << "]\n";
        std::filesystem::path schedulerPath = baseFilename;
        schedulerPath.replace_filename(schedulerFileName);
        storm::api::exportScheduler(model, schedulers[i], schedulerPath);
    }
    storm::io::closeFile(infoStream);
}

template<typename ValueType>
inline void exportCheckResultToJson(std::shared_ptr<storm::models::sparse::Model<ValueType>> const& model,
                                    std::unique_ptr<storm::modelchecker::CheckResult> const& checkResult, std::string const& filename) {
    std::ofstream stream;
    storm::io::openFile(filename, stream);
    if (checkResult->isExplicitQualitativeCheckResult()) {
        auto j = checkResult->template asExplicitQualitativeCheckResult<ValueType>().template toJson<storm::RationalNumber>(model->getOptionalStateValuations(),
                                                                                                                            model->getStateLabeling());
        stream << storm::dumpJson(j);
    } else {
        STORM_LOG_THROW(checkResult->isExplicitQuantitativeCheckResult(), storm::exceptions::NotSupportedException,
                        "Export of check results is only supported for explicit check results (e.g. in the sparse engine).");
        auto j = checkResult->template asExplicitQuantitativeCheckResult<ValueType>().toJson(model->getOptionalStateValuations(), model->getStateLabeling());
        stream << storm::dumpJson(j);
    }
    storm::io::closeFile(stream);
}

template<>
inline void exportCheckResultToJson<storm::RationalFunction>(std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> const&,
                                                             std::unique_ptr<storm::modelchecker::CheckResult> const&, std::string const&) {
    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Export of check results is not supported for rational functions.");
}

}  // namespace api
}  // namespace storm
