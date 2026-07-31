#include "storm/storage/Qvbs.h"

#include <algorithm>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/io/file.h"
#include "storm/settings/SettingsManager.h"
#include "storm/settings/modules/IOSettings.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"
#include "storm/utility/string.h"

namespace storm {
namespace storage {

storm::json<storm::RationalNumber> readQvbsJsonFile(std::string const& filePath) {
    STORM_LOG_THROW(storm::io::fileExistsAndIsReadable(filePath), storm::exceptions::WrongFormatException, "QVBS json file " << filePath << " was not found.");
    storm::json<storm::RationalNumber> result;
    std::ifstream file;
    storm::io::openFile(filePath, file);
    file >> result;
    storm::io::closeFile(file);
    return result;
}

std::string getString(storm::json<storm::RationalNumber> const& structure, std::string const& errorInfo = "") {
    if (structure.is_number_integer()) {
        return std::to_string(structure.get<int64_t>());
    } else if (structure.is_number_float()) {
        return storm::utility::to_string(structure.get<storm::RationalNumber>());
    } else if (structure.is_string()) {
        return structure.get<std::string>();
    } else if (structure.is_boolean()) {
        return structure.get<bool>() ? "true" : "false";
    } else {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException,
                        "Expected a string, number, or bool, got '" << structure.dump() << "' " << errorInfo << ".");
    }
    return "";
}

std::string findModelPath(std::string const& modelName) {
    std::string rootIndexFile = storm::settings::getModule<storm::settings::modules::IOSettings>().getQvbsRoot() + "/index.json";

    auto modelPaths = readQvbsJsonFile(rootIndexFile);
    storm::utility::string::SimilarStrings similarNames(modelName, 0.6, false);
    for (auto const& pathJson : modelPaths) {
        STORM_LOG_THROW(pathJson.count("path") == 1, storm::exceptions::WrongFormatException, "QVBS file " << rootIndexFile << " has unexpected format.");
        std::string path = getString(pathJson["path"], "Path entry in QVBS file " + rootIndexFile);
        std::string currModelName = path.substr(path.find("/") + 1);
        if (currModelName == modelName) {
            return storm::settings::getModule<storm::settings::modules::IOSettings>().getQvbsRoot() + "/" + path;
        } else {
            similarNames.add(currModelName);
        }
    }
    STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "QVBS model '" + modelName + "' was not found. " + similarNames.toDidYouMeanString() + ".");
    return "";
}

QvbsBenchmark::QvbsBenchmark(std::string const& modelName) {
    std::string actualModelName = modelName;
    std::transform(actualModelName.begin(), actualModelName.end(), actualModelName.begin(), ::tolower);
    modelPath = findModelPath(actualModelName);
    std::string indexPath = modelPath + "/index.json";
    modelData = readQvbsJsonFile(indexPath);

    STORM_LOG_THROW(modelData.count("files") == 1, storm::exceptions::WrongFormatException, "No files in " + indexPath + ".");
    for (auto const& fileJson : modelData["files"]) {
        std::string janiFileName = getString(fileJson["file"], "file of " + indexPath + ".");
        if (fileJson.count("open-parameter-values") == 1 && fileJson["open-parameter-values"].size() > 0) {
            for (auto const& openParJson : fileJson["open-parameter-values"]) {
                std::string constantDefString = "";
                if (openParJson.count("values") == 1) {
                    for (auto const& valueJson : openParJson["values"]) {
                        if (constantDefString != "") {
                            constantDefString += ",";
                        }
                        constantDefString += getString(valueJson["name"], "open-parameter-values in files in " + indexPath + ".") + "=";
                        constantDefString += getString(valueJson["value"], "open-parameter-values in files in " + indexPath + ".");
                    }
                }
                constantDefinitions.push_back(constantDefString);
                janiFiles.push_back(modelPath + "/" + janiFileName);
                instanceInfos.push_back(janiFileName + " \t" + constantDefString);
                if (openParJson.count("states") == 1 && openParJson["states"].size() > 0) {
                    uint64_t states = 0;
                    for (auto const& statesJson : openParJson["states"]) {
                        auto note = getString(statesJson["note"]);
                        if (note.find("Storm") != std::string::npos) {
                            states = statesJson["number"].get<uint64_t>();
                        }
                    }
                    if (states > 0) {
                        instanceInfos.back() += " \t(" + std::to_string(states) + " states)";
                    }
                }
            }
        } else {
            constantDefinitions.push_back("");
            janiFiles.push_back(modelPath + "/" + janiFileName);
            instanceInfos.push_back(janiFileName);
        }
    }
}

std::string const& QvbsBenchmark::getJaniFile(uint64_t instanceIndex) const {
    STORM_LOG_THROW(instanceIndex < janiFiles.size(), storm::exceptions::InvalidArgumentException, "Instance index " << instanceIndex << " is too high.");
    return janiFiles[instanceIndex];
}

std::string const& QvbsBenchmark::getConstantDefinition(uint64_t instanceIndex) const {
    STORM_LOG_THROW(instanceIndex < constantDefinitions.size(), storm::exceptions::InvalidArgumentException,
                    "Instance index " << instanceIndex << " is too high.");
    return constantDefinitions[instanceIndex];
}

std::string QvbsBenchmark::getInfo(uint64_t instanceIndex, boost::optional<std::vector<std::string>> propertyFilter) const {
    std::stringstream s;
    s << "--------------------------------------------------------------\n";
    s << "QVBS " << getString(modelData["type"]) << "-Benchmark: " << getString(modelData["name"]) << " (" << getString(modelData["short"]) << ") v"
      << getString(modelData["version"]) << '\n';
    if (instanceInfos.size() == 1) {
        s << "1 instance:\n";
    } else if (instanceInfos.size() > 1) {
        s << instanceInfos.size() << " instances:\n";
    }
    for (uint64_t i = 0; i < instanceInfos.size(); ++i) {
        s << "\t" << (i == instanceIndex ? "*" : " ") << i << "\t" << instanceInfos[i] << '\n';
    }
    if (modelData.count("properties") == 1) {
        if (modelData["properties"].size() == 1) {
            s << "1 property:\n";
        } else if (modelData["properties"].size() > 1) {
            s << modelData["properties"].size() << " properties:\n";
        }
        for (auto const& property : modelData["properties"]) {
            std::string propertyName = getString(property["name"]);
            s << "\t";
            if (!propertyFilter.is_initialized() || std::find(propertyFilter->begin(), propertyFilter->end(), propertyName) != propertyFilter->end()) {
                s << "*";
            } else {
                s << " ";
            }
            s << propertyName << " \t(" << getString(property["type"]) << ")\n";
        }
    }
    s << "--------------------------------------------------------------\n";
    return s.str();
}

}  // namespace storage
}  // namespace storm
