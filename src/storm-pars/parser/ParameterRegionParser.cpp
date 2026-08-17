#include "storm-pars/parser/ParameterRegionParser.h"

#include <boost/algorithm/string.hpp>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/io/file.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

template<typename ParametricType>
void ParameterRegionParser<ParametricType>::parseParameterBoundaries(Valuation& lowerBoundaries, Valuation& upperBoundaries,
                                                                     std::string const& parameterBoundariesString,
                                                                     std::set<VariableType> const& consideredVariables) {
    std::string::size_type positionOfFirstRelation = parameterBoundariesString.find("<=");
    STORM_LOG_THROW(positionOfFirstRelation != std::string::npos, storm::exceptions::InvalidArgumentException,
                    "When parsing the region" << parameterBoundariesString << " I could not find a '<=' after the first number.");
    std::string::size_type positionOfSecondRelation = parameterBoundariesString.find("<=", positionOfFirstRelation + 2);
    STORM_LOG_THROW(positionOfSecondRelation != std::string::npos, storm::exceptions::InvalidArgumentException,
                    "When parsing the region" << parameterBoundariesString << " I could not find a '<=' after the parameter.");

    std::string parameter = parameterBoundariesString.substr(positionOfFirstRelation + 2, positionOfSecondRelation - (positionOfFirstRelation + 2));

    // removes all whitespaces from the parameter string:
    parameter.erase(std::remove_if(parameter.begin(), parameter.end(), ::isspace), parameter.end());
    STORM_LOG_THROW(parameter.length() > 0, storm::exceptions::InvalidArgumentException,
                    "When parsing the region" << parameterBoundariesString << " I could not find a parameter.");

    std::unique_ptr<VariableType> var;
    for (auto const& v : consideredVariables) {
        std::stringstream stream;
        stream << v;
        if (parameter == stream.str()) {
            var = std::make_unique<VariableType>(v);
            break;
        }
    }
    if (var) {
        CoefficientType lb = storm::utility::convertNumber<CoefficientType>(parameterBoundariesString.substr(0, positionOfFirstRelation));
        CoefficientType ub = storm::utility::convertNumber<CoefficientType>(parameterBoundariesString.substr(positionOfSecondRelation + 2));
        lowerBoundaries.emplace(std::make_pair(*var, lb));
        upperBoundaries.emplace(std::make_pair(*var, ub));
    } else {
        STORM_LOG_WARN("Could not find parameter " << parameter << " in the set of considered variables. Ignoring this parameter.");
    }
}

template<typename ParametricType>
storm::storage::ParameterRegion<ParametricType> ParameterRegionParser<ParametricType>::parseRegion(std::string const& regionString,
                                                                                                   std::set<VariableType> const& consideredVariables) {
    Valuation lowerBoundaries;
    Valuation upperBoundaries;
    std::vector<std::string> parameterBoundaries;
    boost::split(parameterBoundaries, regionString, boost::is_any_of(","));
    for (auto const& parameterBoundary : parameterBoundaries) {
        if (!std::all_of(parameterBoundary.begin(), parameterBoundary.end(), ::isspace)) {  // skip this string if it only consists of space
            parseParameterBoundaries(lowerBoundaries, upperBoundaries, parameterBoundary, consideredVariables);
        }
    }

    // Check that all considered variables are bounded
    for (auto const& v : consideredVariables) {
        STORM_LOG_THROW(lowerBoundaries.count(v) > 0, storm::exceptions::WrongFormatException, "Variable " << v << " was not defined in region string.");
        STORM_LOG_ASSERT(upperBoundaries.count(v) > 0, "Variable " << v << " has a lower but not an upper bound.");
    }
    auto res = storm::storage::ParameterRegion<ParametricType>(std::move(lowerBoundaries), std::move(upperBoundaries));

    return res;
}

template<typename ParametricType>
storm::storage::ParameterRegion<ParametricType> ParameterRegionParser<ParametricType>::createRegion(std::string const& regionBound,
                                                                                                    std::set<VariableType> const& consideredVariables) {
    Valuation lowerBoundaries;
    Valuation upperBoundaries;
    std::vector<std::string> parameterBoundaries;
    CoefficientType bound = storm::utility::convertNumber<CoefficientType>(regionBound);
    for (auto const& v : consideredVariables) {
        lowerBoundaries.emplace(std::make_pair(v, 0 + bound));
        upperBoundaries.emplace(std::make_pair(v, 1 - bound));
    }
    auto res = storm::storage::ParameterRegion<ParametricType>(std::move(lowerBoundaries), std::move(upperBoundaries));

    return res;
}

template<typename ParametricType>
std::vector<storm::storage::ParameterRegion<ParametricType>> ParameterRegionParser<ParametricType>::parseMultipleRegions(
    std::string const& regionsString, std::set<VariableType> const& consideredVariables) {
    std::vector<storm::storage::ParameterRegion<ParametricType>> result;
    std::vector<std::string> regionsStrVec;
    boost::split(regionsStrVec, regionsString, boost::is_any_of(";"));
    for (auto const& regionStr : regionsStrVec) {
        if (!std::all_of(regionStr.begin(), regionStr.end(), ::isspace)) {  // skip this string if it only consists of space
            result.emplace_back(parseRegion(regionStr, consideredVariables));
        }
    }
    return result;
}

template<typename ParametricType>
std::vector<storm::storage::ParameterRegion<ParametricType>> ParameterRegionParser<ParametricType>::parseMultipleRegionsFromFile(
    std::string const& fileName, std::set<VariableType> const& consideredVariables) {
    // Open file and initialize result.
    std::ifstream inputFileStream;
    storm::io::openFile(fileName, inputFileStream);

    std::vector<storm::storage::ParameterRegion<ParametricType>> result;

    // Now try to parse the contents of the file.
    try {
        std::string fileContent((std::istreambuf_iterator<char>(inputFileStream)), (std::istreambuf_iterator<char>()));
        result = parseMultipleRegions(fileContent, consideredVariables);
    } catch (std::exception& e) {
        // In case of an exception properly close the file before passing exception.
        storm::io::closeFile(inputFileStream);
        throw e;
    }

    // Close the stream in case everything went smoothly and return result.
    storm::io::closeFile(inputFileStream);
    return result;
}

template class ParameterRegionParser<storm::RationalFunction>;
}  // namespace parser
}  // namespace storm
