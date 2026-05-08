#include "storm/storage/umb/export/UmbExport.h"

#include <boost/pfr.hpp>

#include "storm/storage/umb/model/UmbModel.h"
#include "storm/storage/umb/model/ValueEncoding.h"

#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/io/ArchiveWriter.h"
#include "storm/io/file.h"
#include "storm/utility/bitoperations.h"
#include "storm/utility/macros.h"

namespace storm::umb {

namespace detail {

void createDirectory(storm::io::ArchiveWriter& archiveWriter, std::filesystem::path const& subdirectory) {
    archiveWriter.addDirectory(subdirectory.string());
}

/*!
 * Write a vector to archive.
 * The file path must have the extension .bin.
 */
template<typename VectorType>
    requires(!std::is_same_v<std::remove_cvref_t<VectorType>, storm::umb::GenericVector>)
void writeVector(VectorType const& vector, storm::io::ArchiveWriter& archiveWriter, std::filesystem::path const& filepath) {
    archiveWriter.addBinaryFile(filepath.string(), vector);
}

void writeVector(GenericVector const& vector, storm::io::ArchiveWriter& target, std::filesystem::path const& filepath) {
    if (!vector.hasValue()) {
        return;
    }
    if (vector.template isType<bool>()) {
        writeVector(vector.template get<bool>(), target, filepath);
    } else if (vector.template isType<uint64_t>()) {
        writeVector(vector.template get<uint64_t>(), target, filepath);
    } else if (vector.template isType<int64_t>()) {
        writeVector(vector.template get<int64_t>(), target, filepath);
    } else if (vector.template isType<double>()) {
        writeVector(vector.template get<double>(), target, filepath);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type.");
    }
}

template<typename VectorType>
void writeVector(std::optional<VectorType> const& vector, storm::io::ArchiveWriter& target, std::filesystem::path const& filepath) {
    if (vector) {
        writeVector(*vector, target, filepath);
    }
}

void writeIndexFile(storm::umb::ModelIndex const& index, storm::io::ArchiveWriter& archiveWriter, std::filesystem::path const& filepath) {
    archiveWriter.addTextFile(filepath, storm::dumpJson(storm::json<storm::RationalNumber>(index)));
}

template<typename T>
concept HasFileNames = requires { T::FileNames.size(); };

template<typename T>
concept FileNameMap = std::same_as<std::remove_cvref_t<typename T::key_type>, std::string>;

template<typename T>
concept IsOptionalWithFileNames = std::same_as<std::remove_cvref_t<T>, std::optional<typename T::value_type>> && HasFileNames<typename T::value_type>;

// Forward declare function so that it can be called recursively
template<typename UmbStructure>
    requires HasFileNames<UmbStructure>
void exportFiles(UmbStructure const& umbStructure, storm::io::ArchiveWriter& target, std::filesystem::path const& context);

void exportFiles(FileNameMap auto const& umbStructure, storm::io::ArchiveWriter& target, std::filesystem::path const& context) {
    for (auto const& [key, value] : umbStructure) {
        if (!key.empty()) {
            createDirectory(target, context / key);
        }
        exportFiles(value, target, context / key);
    }
}

template<typename UmbStructure>
    requires HasFileNames<UmbStructure>
void exportFiles(UmbStructure const& umbStructure, storm::io::ArchiveWriter& target, std::filesystem::path const& context) {
    static_assert(UmbStructure::FileNames.size() == boost::pfr::tuple_size_v<UmbStructure>, "Number of file names does not match number of fields in struct.");
    boost::pfr::for_each_field(umbStructure, [&](auto const& field, std::size_t fieldIndex) {
        // potentially create directory for sub-field
        std::filesystem::path fieldName = std::data(UmbStructure::FileNames)[fieldIndex];
        // export the field, either with a recursive call or via writeVector
        using FieldType = std::remove_cvref_t<decltype(field)>;
        if constexpr (HasFileNames<FieldType>) {
            if (!fieldName.empty()) {
                createDirectory(target, context / fieldName);
            }
            exportFiles(field, target, context / fieldName);
        } else if constexpr (IsOptionalWithFileNames<FieldType>) {
            if (field) {
                if (!fieldName.empty()) {
                    createDirectory(target, context / fieldName);
                }
                exportFiles(*field, target, context / fieldName);
            }
        } else if constexpr (FileNameMap<FieldType>) {
            if (!field.empty() && !fieldName.empty()) {
                createDirectory(target, context / fieldName);
            }
            exportFiles(field, target, context / fieldName);
        } else if constexpr (std::is_same_v<FieldType, storm::umb::ModelIndex>) {
            writeIndexFile(field, target, context / fieldName);
        } else if constexpr (std::is_same_v<FieldType, decltype(UmbModel::nonStandardFiles)>) {
            for (auto const& [path, data] : field) {
                if (path.has_parent_path()) {
                    createDirectory(target, context / fieldName / path.parent_path());
                }
                writeVector(data, target, context / fieldName / path);
            }
        } else if constexpr (std::is_same_v<FieldType, GenericVector>) {
            if (field.template isType<storm::RationalNumber>() || field.template isType<storm::RationalInterval>()) {
                // Need to call UMBModel::encodeRationals prior to export
                STORM_LOG_THROW(false, storm::exceptions::UnexpectedException,
                                "Unable to export rational vector '" << (context / fieldName) << "' to UMB. Rational values must be encoded prior to export");
            } else if (field.template isType<storm::Interval>()) {
                writeVector(ValueEncoding::intervalToBaseRangeView<double>(field.template get<storm::Interval>()), target, context / fieldName);
            } else {
                writeVector(field, target, context / fieldName);
            }
        } else {
            writeVector(field, target, context / fieldName);
        }
    });
}

}  // namespace detail

void toArchive(storm::umb::UmbModel const& umbModel, std::filesystem::path const& archivePath, ExportOptions const& options) {
    auto compression = options.compression;
    // Set gzip as default compression
    if (compression == storm::io::CompressionMode::Default) {
        compression = storm::io::CompressionMode::Gzip;
    }
    storm::io::ArchiveWriter archiveWriter(archivePath, compression);
    detail::exportFiles(umbModel, archiveWriter, {});
}

}  // namespace storm::umb