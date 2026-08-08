#include "storm/storage/umb/import/UmbImport.h"

#include "storm/storage/umb/model/UmbModel.h"

#include "storm/adapters/JsonAdapter.h"
#include "storm/io/ArchiveReader.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"

namespace storm::umb {

namespace internal {
template<typename T>
concept HasFileNames = requires { T::FileNames.size(); };

template<typename T>
concept FileNameMap = std::same_as<std::remove_cvref_t<typename T::key_type>, std::string>;

template<typename T>
concept IsOptional = std::same_as<std::remove_cvref_t<T>, std::optional<typename T::value_type>>;

template<typename T>
concept IsOptionalWithFileNames = IsOptional<T> && HasFileNames<typename T::value_type>;

void parseIndexFromString(std::string const& indexFileString, storm::umb::ModelIndex& index) {
    storm::json<storm::RationalNumber>::parse(indexFileString).get_to(index);
}

/*!
 * Prepares annotations so that all fields are available according to the index.
 */
void prepareAnnotations(storm::umb::UmbModel& umbModel) {
    if (!umbModel.index.annotations.has_value()) {
        return;
    }
    for (auto const& [annotationType, annotationMap] : umbModel.index.annotations.value()) {
        for (auto const& [annotationName, annotationIndex] : annotationMap) {
            umbModel.annotations[annotationType][annotationName];  // ensure that the map keys exist
        }
    }
}

std::filesystem::path getFilePath(typename storm::io::ArchiveReader::ArchiveReadEntry const& src) {
    return src.name();
}

template<typename VecT>
    requires std::same_as<VecT, std::vector<typename VecT::value_type>>
VecT importVector(typename storm::io::ArchiveReader::ArchiveReadEntry& src) {
    return src.template toVector<typename VecT::value_type>();
}

template<typename VecT>
    requires std::same_as<VecT, storm::storage::BitVector>
VecT importVector(typename storm::io::ArchiveReader::ArchiveReadEntry& src) {
    return src.template toVector<bool>();
}

template<typename VecT>
void importVector(typename storm::io::ArchiveReader::ArchiveReadEntry& src, std::optional<VecT>& target) {
    target = importVector<VecT>(src);
}

template<typename VecT>
    requires(!IsOptional<VecT>)
void importVector(typename storm::io::ArchiveReader::ArchiveReadEntry& src, VecT& target) {
    target = importVector<VecT>(src);
}

template<typename ValueType>
void importGenericVector(typename storm::io::ArchiveReader::ArchiveReadEntry& src, GenericVector& target) {
    target.template set<ValueType>(importVector<typename GenericVector::template Vec<ValueType>>(src));
}

void importGenericVector(typename storm::io::ArchiveReader::ArchiveReadEntry& src, SizedType const& type, GenericVector& target) {
    using enum Type;
    switch (type.type) {
        case Bool:
            STORM_LOG_ASSERT(type.bitSize() == 1, "Boolean types must have size 1.");
            importGenericVector<bool>(src, target);
            break;
        case Int:
        case IntInterval:
            STORM_LOG_ASSERT(type.bitSize() % 64 == 0, "Int-based types must have size multiple of 64.");
            importGenericVector<int64_t>(src, target);
            break;
        case Uint:
        case UintInterval:
        case Rational:
        case RationalInterval:
        case String:
            STORM_LOG_ASSERT(type.bitSize() % 64 == 0, "Uint-based types must have size multiple of 64.");
            importGenericVector<uint64_t>(src, target);
            break;
        case Double:
        case DoubleInterval:
            STORM_LOG_ASSERT(type.bitSize() % 64 == 0, "Double-based types must have size 64.");
            importGenericVector<double>(src, target);
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                            "Type " << type.toString() << " for vector located in '" << getFilePath(src) << "' is not handled.");
    }
}

void importGenericVector(typename storm::io::ArchiveReader::ArchiveReadEntry& src, storm::umb::ModelIndex const& index, GenericVector& target) {
    // Find type information in the index that matches the given src.
    auto srcPath = getFilePath(src);
    std::vector<std::string> srcPathVec(srcPath.begin(), srcPath.end());
    if (srcPath == "branch-to-probability.bin") {
        STORM_LOG_THROW(index.transitionSystem.branchProbabilityType.has_value(), storm::exceptions::WrongFormatException,
                        "Found branch probabilities but no type specified.");
        importGenericVector(src, index.transitionSystem.branchProbabilityType.value(), target);
    } else if (srcPath == "state-to-exit-rate.bin") {
        STORM_LOG_THROW(index.transitionSystem.exitRateType.has_value(), storm::exceptions::WrongFormatException, "Found exit rates but no type specified.");
        importGenericVector(src, index.transitionSystem.exitRateType.value(), target);
    } else if (srcPathVec.size() == 3 && srcPathVec[0] == "observations" && srcPathVec[2] == "probabilities.bin") {
        STORM_LOG_THROW(index.transitionSystem.observationProbabilityType.has_value(), storm::exceptions::WrongFormatException,
                        "Found observation probabilities but no type specified.");
        importGenericVector(src, index.transitionSystem.observationProbabilityType.value(), target);
    } else {
        // Reaching this point means that we must have annotation values.
        // We expect a path of the form "annotations/<annotationType>/<annotationId>/<entity>/[values|probabilities].bin"
        STORM_LOG_THROW(
            srcPathVec.size() == 5 && srcPathVec[0] == "annotations" && (srcPathVec[4] == "values.bin" || srcPathVec[4] == "probabilities.bin"),
            storm::exceptions::WrongFormatException,
            "Unexpected file path '" << srcPath << "'. Expected 'annotations/<annotationType>/<annotationId>/<entity>/[values|probabilities].bin'.");
        auto annotationMap = index.annotation(srcPathVec[1]);
        STORM_LOG_THROW(annotationMap.has_value(), storm::exceptions::WrongFormatException,
                        "Annotation type '" << srcPathVec[1] << "' referenced in files but not found in index.");
        auto annotationIt = annotationMap->find(srcPathVec[2]);
        STORM_LOG_THROW(annotationIt != annotationMap->end(), storm::exceptions::WrongFormatException,
                        "Annotation id '" << srcPathVec[2] << "' for type '" << srcPathVec[1] << "' referenced in files but not found in index.");
        auto const& annotation = annotationIt->second;
        if (srcPathVec[4] == "values.bin") {
            importGenericVector(src, annotation.type, target);
        } else {
            STORM_LOG_THROW(
                annotation.probabilityType.has_value(), storm::exceptions::WrongFormatException,
                "Found probabilities for annotation '" << srcPathVec[2] << "' of type '" << srcPathVec[1] << "' but no probability type specified.");
            importGenericVector(src, annotation.probabilityType.value(), target);
        }
    }
}

// Forward declare function so that it can be called recursively
template<typename UmbStructure>
    requires HasFileNames<UmbStructure>
bool importVector(typename storm::io::ArchiveReader::ArchiveReadEntry& src, storm::umb::ModelIndex const& index, UmbStructure& umbStructure,
                  std::filesystem::path const& context);

bool importVector(typename storm::io::ArchiveReader::ArchiveReadEntry& src, storm::umb::ModelIndex const& index, FileNameMap auto& umbStructure,
                  std::filesystem::path const& context) {
    // Assumes that the file name maps are pre-filled (see prepareAnnotations).
    for (auto& [key, value] : umbStructure) {
        if (importVector(src, index, value, context / key)) {
            return true;
        }
    }
    return false;
}

template<typename UmbStructure>
    requires HasFileNames<UmbStructure>
bool importVector(typename storm::io::ArchiveReader::ArchiveReadEntry& src, storm::umb::ModelIndex const& index, UmbStructure& umbStructure,
                  std::filesystem::path const& context) {
    static_assert(UmbStructure::FileNames.size() == boost::pfr::tuple_size_v<UmbStructure>, "Number of file names does not match number of fields in struct.");

    // helper to check if src is in the given path
    auto containsSrc = [&src](auto const& other) {
        auto srcPath = getFilePath(src);
        auto srcIt = srcPath.begin();
        for (auto const& o : other) {
            if (srcIt == srcPath.end() || (*srcIt != o && !o.empty())) {
                return false;
            }
            ++srcIt;
        }
        return true;
    };

    // check if the src is in the given context
    if (!containsSrc(context)) {
        return false;
    }
    bool found = false;
    boost::pfr::for_each_field(umbStructure, [&](auto& field, std::size_t fieldIndex) {
        auto fieldPath = context / std::data(UmbStructure::FileNames)[fieldIndex];
        // handle the case that we already found the subfield or that this is not the right subfield
        if (found || !containsSrc(fieldPath)) {
            return;
        }

        // load the file into this field, either with a recursive call or directly if the field points to a file
        using FieldType = std::remove_cvref_t<decltype(field)>;
        if constexpr (HasFileNames<FieldType>) {
            found = importVector(src, index, field, fieldPath);
        } else if constexpr (IsOptionalWithFileNames<FieldType>) {
            if (!field.has_value()) {
                field.emplace();
            }
            found = importVector(src, index, field.value(), fieldPath);
        } else if constexpr (FileNameMap<FieldType>) {
            found = importVector(src, index, field, fieldPath);
        } else if constexpr (std::is_same_v<FieldType, decltype(UmbModel::nonStandardFiles)>) {
            // The following assumes that we have tried all other fields first as it matches any file.
            STORM_LOG_ASSERT(fieldIndex + 1 == UmbStructure::FileNames.size(), "The field for non-standard files must be the last field in the struct.");
            found = true;
            field.emplace(fieldPath / getFilePath(src), importVector<std::vector<char>>(src));
        } else {
            // reaching this point means that we have found the right field
            STORM_LOG_THROW(fieldPath == getFilePath(src), storm::exceptions::WrongFormatException,
                            "Unexpected file paths: " << fieldPath << " != " << getFilePath(src) << ".");
            found = true;
            if constexpr (std::is_same_v<FieldType, GenericVector>) {
                importGenericVector(src, index, field);
            } else if constexpr (!std::is_same_v<FieldType, storm::umb::ModelIndex>) {
                importVector(src, field);
            }
        }
    });
    return found;
}

storm::umb::UmbModel fromArchive(std::filesystem::path const& umbArchive, ImportOptions const& /* options */) {
    storm::umb::UmbModel umbModel;
    // First pass: find the index file
    bool indexFound = false;
    for (auto entry : storm::io::openArchive(umbArchive)) {
        if (entry.name() == "index.json") {
            parseIndexFromString(entry.toString(), umbModel.index);
            indexFound = true;
            break;
        }
    }
    STORM_LOG_THROW(indexFound, storm::exceptions::FileIoException, "File 'index.json' not found in UMB archive.");
    STORM_LOG_TRACE("Index file found in umb archive " << umbArchive << ": \n" << storm::dumpJson(storm::json<storm::RationalNumber>(umbModel.index)));
    // Second pass: load the bin files
    prepareAnnotations(umbModel);
    for (auto entry : storm::io::openArchive(umbArchive)) {
        if (entry.name() == "index.json" || entry.isDir()) {
            continue;  // skip the index file and directories
        }
        STORM_LOG_TRACE("Loading file " << getFilePath(entry) << " in UMB archive " << umbArchive << "'.");
        bool const found = importVector(entry, umbModel.index, umbModel, "");
        // At this point, we must have found a matching field, even for non-standard files.
        STORM_LOG_THROW(found, storm::exceptions::UnexpectedException,
                        "File " << getFilePath(entry) << " in UMB archive " << umbArchive << " could not be matched to any UMB field.");
    }
    return umbModel;
}

}  // namespace internal

storm::umb::UmbModel importUmb(std::filesystem::path const& umbLocation, ImportOptions const& options) {
    STORM_LOG_THROW(std::filesystem::exists(umbLocation), storm::exceptions::FileIoException, "The given path '" << umbLocation << "' does not exist.");
    return internal::fromArchive(umbLocation, options);
}

}  // namespace storm::umb