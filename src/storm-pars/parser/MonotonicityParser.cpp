#include "storm-pars/parser/MonotonicityParser.h"

#include <boost/algorithm/string.hpp>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/io/file.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

template<typename VariableType>
std::pair<std::set<VariableType>, std::set<VariableType>> MonotonicityParser<VariableType>::parseMonotoneVariablesFromFile(
    std::string const& fileName, std::set<VariableType> const& consideredVariables) {
    // Open file and initialize result.
    std::ifstream inputFileStream;
    storm::io::openFile(fileName, inputFileStream);

    std::set<VariableType> monotoneIncrVars;
    std::set<VariableType> monotoneDecrVars;

    // Now try to parse the contents of the file.
    try {
        std::string fileContent((std::istreambuf_iterator<char>(inputFileStream)), (std::istreambuf_iterator<char>()));
        std::vector<std::string> fileSplitted;

        boost::split(fileSplitted, fileContent, boost::is_any_of(";"));
        STORM_LOG_THROW(fileSplitted.size() == 2, storm::exceptions::WrongFormatException, "Expecting content to contain \";\" between monotone variables.");
        std::vector<std::string> monotoneIncrVarsString;
        boost::split(monotoneIncrVarsString, fileSplitted[0], boost::is_any_of(" "));
        std::vector<std::string> monotoneDecrVarsString;
        boost::split(monotoneDecrVarsString, fileSplitted[0], boost::is_any_of(" "));
        // TODO: throw errors if file not formatted correctly
        for (auto varString : monotoneIncrVarsString) {
            VariableType var;
            for (auto const& v : consideredVariables) {
                std::stringstream stream;
                stream << v;
                if (varString == stream.str()) {
                    var = v;
                    break;
                }
            }
            monotoneIncrVars.insert(var);
        }
        for (auto varString : monotoneDecrVarsString) {
            VariableType var;
            for (auto const& v : consideredVariables) {
                std::stringstream stream;
                stream << v;
                if (varString == stream.str()) {
                    var = v;
                    break;
                }
            }
            monotoneDecrVars.insert(var);
        }

    } catch (std::exception& e) {
        // In case of an exception properly close the file before passing exception.
        storm::io::closeFile(inputFileStream);
        throw e;
    }

    // Close the stream in case everything went smoothly and return result.
    storm::io::closeFile(inputFileStream);
    return {std::move(monotoneIncrVars), std::move(monotoneDecrVars)};
}

template class MonotonicityParser<storm::RationalFunctionVariable>;
}  // namespace parser
}  // namespace storm