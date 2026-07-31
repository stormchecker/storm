#include "storm-dft/parser/DFTGalileoParser.h"

#include <boost/algorithm/string.hpp>
#include <optional>
#include <regex>

#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/io/file.h"
#include "storm/utility/macros.h"

namespace storm::dft {
namespace parser {

template<typename ValueType>
storm::dft::storage::DFT<ValueType> DFTGalileoParser<ValueType>::parseDFT(const std::string& filename) {
    storm::dft::builder::DFTBuilder<ValueType> builder;
    storm::parser::ValueParser<ValueType> valueParser;
    // Regular expression to detect comments
    // taken from: https://stackoverflow.com/questions/9449887/removing-c-c-style-comments-using-boostregex
    const std::regex commentRegex("(/\\*([^*]|(\\*+[^*/]))*\\*+/)|(//.*)");

    std::ifstream file;
    storm::io::openFile(filename, file);

    std::string line;
    size_t lineNo = 0;
    std::string toplevelId = "";
    bool comment = false;  // Indicates whether the current line is part of a multiline comment
    try {
        while (storm::io::getline(file, line)) {
            ++lineNo;
            // First consider comments
            if (comment) {
                // Line is part of multiline comment -> search for end of this comment
                size_t commentEnd = line.find("*/");
                if (commentEnd == std::string::npos) {
                    continue;
                } else {
                    // Remove comment
                    line = line.substr(commentEnd + 2);
                    comment = false;
                }
            }
            // Remove in-line comments
            line = std::regex_replace(line, commentRegex, "");
            // Check if multiline comment starts
            size_t commentStart = line.find("/*");
            if (commentStart != std::string::npos) {
                // Only keep part before comment
                line = line.substr(0, commentStart);
                comment = true;
            }

            boost::trim(line);
            if (line.empty()) {
                // Empty line
                continue;
            }

            // Remove semicolon
            STORM_LOG_THROW(line.back() == ';', storm::exceptions::WrongFormatException, "Semicolon expected at the end of line " << lineNo << ".");
            line.pop_back();

            // Split line into tokens w.r.t. white space
            boost::trim(line);
            std::vector<std::string> tokens;
            boost::split(tokens, line, boost::is_any_of(" \t"), boost::token_compress_on);

            // Start actual parsing
            if (tokens[0] == "toplevel") {
                // Top level indicator
                STORM_LOG_THROW(toplevelId.empty(), storm::exceptions::WrongFormatException, "Toplevel element already defined.");
                STORM_LOG_THROW(tokens.size() == 2, storm::exceptions::WrongFormatException, "Expected unique element id after 'toplevel'.");
                toplevelId = parseName(tokens[1]);
            } else if (tokens[0] == "param") {
                // Parameters
                STORM_LOG_THROW(tokens.size() == 2, storm::exceptions::WrongFormatException, "Expected unique parameter name after 'param'.");
                STORM_LOG_THROW((std::is_same<ValueType, storm::RationalFunction>::value), storm::exceptions::NotSupportedException,
                                "Parameters are only allowed when using rational functions.");
                valueParser.addParameter(parseName(tokens[1]));
            } else {
                // DFT element
                std::string name = parseName(tokens[0]);

                std::vector<std::string> childNames;
                for (size_t i = 2; i < tokens.size(); ++i) {
                    childNames.push_back(parseName(tokens[i]));
                }

                // Add element according to type
                std::string type = tokens[1];
                if (type == "and") {
                    builder.addAndGate(name, childNames);
                } else if (type == "or") {
                    builder.addOrGate(name, childNames);
                } else if (boost::starts_with(type, "vot")) {
                    size_t threshold = storm::parser::parseNumber<size_t>(type.substr(3));
                    builder.addVotingGate(name, threshold, childNames);
                } else if (type.find("of") != std::string::npos) {
                    size_t pos = type.find("of");
                    size_t threshold = storm::parser::parseNumber<size_t>(type.substr(0, pos));
                    size_t count = storm::parser::parseNumber<size_t>(type.substr(pos + 2));
                    STORM_LOG_THROW(count == childNames.size(), storm::exceptions::WrongFormatException,
                                    "Voting gate number " << count << " does not correspond to number of children " << childNames.size() << ".");
                    builder.addVotingGate(name, threshold, childNames);
                } else if (type == "pand") {
                    builder.addPandGate(name, childNames);
                } else if (type == "pand-incl" || type == "pand<=") {
                    builder.addPandGate(name, childNames, true);
                } else if (type == "pand-excl" || type == "pand<") {
                    builder.addPandGate(name, childNames, false);
                } else if (type == "por") {
                    builder.addPorGate(name, childNames);
                } else if (type == "por-incl" || type == "por<=") {
                    builder.addPorGate(name, childNames, true);
                } else if (type == "por-excl" || type == "por<") {
                    builder.addPorGate(name, childNames, false);
                } else if (type == "wsp" || type == "csp" || type == "hsp" || type == "spare") {
                    builder.addSpareGate(name, childNames);
                } else if (type == "seq") {
                    builder.addSequenceEnforcer(name, childNames);
                } else if (type == "mutex") {
                    builder.addMutex(name, childNames);
                } else if (type == "fdep") {
                    builder.addPdep(name, childNames, storm::utility::one<ValueType>());
                } else if (boost::starts_with(type, "pdep=")) {
                    ValueType probability = valueParser.parseValue(type.substr(5));
                    builder.addPdep(name, childNames, probability);
                } else if (type.find("=") != std::string::npos) {
                    // Use dedicated method for parsing BEs
                    // Remove name from line and parse remainder
                    std::regex regexName("\"?" + tokens[0] + "\"?");
                    std::string remaining_line = std::regex_replace(line, regexName, "");
                    parseBasicElement(name, remaining_line, builder, valueParser);
                } else if (type.find("insp") != std::string::npos) {
                    // Inspection as defined by DFTCalc
                    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Inspections are not supported.");
                } else {
                    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Type name '" << type << "' not recognized.");
                }
            }
        }
    } catch (storm::exceptions::BaseException const& exception) {
        STORM_LOG_THROW(false, storm::exceptions::FileIoException, "A parsing exception occurred in line " << lineNo << ": " << exception.what());
    }
    builder.setTopLevel(toplevelId);
    storm::io::closeFile(file);

    // Build DFT
    return builder.build();
}

template<typename ValueType>
std::string DFTGalileoParser<ValueType>::parseName(std::string const& name) {
    size_t firstQuots = name.find("\"");
    if (firstQuots != std::string::npos) {
        // Remove quotation marks
        size_t secondQuots = name.find("\"", firstQuots + 1);
        STORM_LOG_THROW(secondQuots != std::string::npos, storm::exceptions::WrongFormatException, "No ending quotation mark found in " << name);
        return name.substr(firstQuots + 1, secondQuots - 1);
    } else {
        return name;
    }
}

template<typename ValueType>
std::string DFTGalileoParser<ValueType>::parseValue(std::string name, std::string& line) {
    // Build regex for: name=(value)
    std::regex nameRegex(name + "\\s*=\\s*([^\\s]*)");
    std::smatch match;
    if (std::regex_search(line, match, nameRegex)) {
        // Remove matched part
        std::string value = match.str(1);
        line = std::regex_replace(line, nameRegex, "");
        return value;
    } else {
        // No match found
        return "";
    }
}

template<typename ValueType>
void DFTGalileoParser<ValueType>::parseBasicElement(std::string const& name, std::string& input, storm::dft::builder::DFTBuilder<ValueType>& builder,
                                                    storm::parser::ValueParser<ValueType>& valueParser) {
    // Avoid writing too much
    using namespace storm::dft::storage::elements;

    // Parameters for distributions
    std::optional<BEType> distribution;
    std::optional<ValueType> prob;
    std::optional<ValueType> lambda;
    std::optional<size_t> phases;
    std::optional<ValueType> shape;
    std::optional<ValueType> rate;
    std::optional<ValueType> mean;
    std::optional<ValueType> stddev;
    std::optional<ValueType> dorm;

    // Parse distribution parameters
    // Constant distribution
    std::string value = parseValue("prob", input);
    if (!value.empty()) {
        prob = valueParser.parseValue(value);
        distribution = BEType::PROBABILITY;
    }

    // Exponential distribution
    value = parseValue("lambda", input);
    if (!value.empty()) {
        STORM_LOG_THROW(
            !distribution.has_value(), storm::exceptions::WrongFormatException,
            "Two distributions " << toString(distribution.value()) << " and " << toString(BEType::EXPONENTIAL) << " are defined for BE '" << name << "'.");
        lambda = valueParser.parseValue(value);
        distribution = BEType::EXPONENTIAL;
    }

    // Erlang distribution
    // Parameter 'lambda' was already handled before for exponential distribution
    value = parseValue("phases", input);
    if (!value.empty()) {
        STORM_LOG_THROW(
            !distribution.has_value() || distribution.value() == BEType::EXPONENTIAL, storm::exceptions::WrongFormatException,
            "Two distributions " << toString(distribution.value()) << " and " << toString(BEType::ERLANG) << " are defined for BE '" << name << "'.");
        phases = storm::parser::parseNumber<size_t>(value);
        distribution = BEType::ERLANG;
    }

    // Weibull distribution
    value = parseValue("shape", input);
    if (!value.empty()) {
        STORM_LOG_THROW(
            !distribution.has_value(), storm::exceptions::WrongFormatException,
            "Two distributions " << toString(distribution.value()) << " and " << toString(BEType::WEIBULL) << " are defined for BE '" << name << "'.");
        shape = valueParser.parseValue(value);
        distribution = BEType::WEIBULL;
    }
    value = parseValue("rate", input);
    if (!value.empty()) {
        STORM_LOG_THROW(
            !distribution.has_value() || distribution.value() == BEType::WEIBULL, storm::exceptions::WrongFormatException,
            "Two distributions " << toString(distribution.value()) << " and " << toString(BEType::WEIBULL) << " are defined for BE '" << name << "'.");
        rate = valueParser.parseValue(value);
    }

    // Log-normal distribution
    value = parseValue("mean", input);
    if (!value.empty()) {
        STORM_LOG_THROW(
            !distribution.has_value(), storm::exceptions::WrongFormatException,
            "Two distributions " << toString(distribution.value()) << " and " << toString(BEType::LOGNORMAL) << " are defined for BE '" << name << "'.");
        mean = valueParser.parseValue(value);
        distribution = BEType::LOGNORMAL;
    }
    value = parseValue("stddev", input);
    if (!value.empty()) {
        STORM_LOG_THROW(
            !distribution.has_value() || distribution.value() == BEType::LOGNORMAL, storm::exceptions::WrongFormatException,
            "Two distributions " << toString(distribution.value()) << " and " << toString(BEType::LOGNORMAL) << " are defined for BE '" << name << "'.");
        stddev = valueParser.parseValue(value);
    }

    // Dormancy factor
    value = parseValue("dorm", input);
    if (!value.empty()) {
        dorm = valueParser.parseValue(value);
    }

    // Additional arguments (wich are not supported)
    value = parseValue("cov", input);
    if (!value.empty()) {
        STORM_LOG_WARN("Coverage is not supported and will be ignored for basic element '" << name << "'.");
    }
    value = parseValue("res", input);
    if (!value.empty()) {
        STORM_LOG_WARN("Restoration is not supported and will be ignored for basic element '" << name << "'.");
    }
    value = parseValue("repl", input);
    if (!value.empty()) {
        size_t replication = storm::parser::parseNumber<size_t>(value);
        STORM_LOG_THROW(replication == 1, storm::exceptions::NotSupportedException, "Replication > 1 is not supported for basic element '" << name << "'.");
    }
    value = parseValue("interval", input);
    if (!value.empty()) {
        STORM_LOG_WARN("Interval is not supported and will be ignored for basic element '" << name << "'.");
    }
    value = parseValue("repair", input);
    STORM_LOG_THROW(value.empty(), storm::exceptions::NotSupportedException,
                    "Repairs are not supported and will be ignored for basic element '" << name << "'.");

    boost::trim(input);
    STORM_LOG_THROW(input == "", storm::exceptions::WrongFormatException, "Unknown arguments for basic element '" << name << "': " << input);

    // Create BE with given distribution
    STORM_LOG_THROW(distribution.has_value(), storm::exceptions::WrongFormatException, "No failure distribution is defined for BE '" << name << "'.");
    switch (distribution.value()) {
        case BEType::PROBABILITY:
            STORM_LOG_THROW(prob.has_value(), storm::exceptions::WrongFormatException,
                            "Distribution " << toString(BEType::PROBABILITY) << " requires parameter 'prob' for BE '" << name << "'.");
            if (!dorm.has_value()) {
                STORM_LOG_WARN("No dormancy factor was provided for basic element '" << name << "'. Assuming dormancy factor of 1.");
                dorm = storm::utility::one<ValueType>();
            }
            builder.addBasicElementProbability(name, prob.value(), dorm.value());
            break;
        case BEType::EXPONENTIAL:
            STORM_LOG_THROW(lambda.has_value(), storm::exceptions::WrongFormatException,
                            "Distribution " << toString(BEType::EXPONENTIAL) << " requires parameter 'lambda' for BE '" << name << "'.");
            if (!dorm.has_value()) {
                STORM_LOG_WARN("No dormancy factor was provided for basic element '" << name << "'. Assuming dormancy factor of 1.");
                dorm = storm::utility::one<ValueType>();
            }
            builder.addBasicElementExponential(name, lambda.value(), dorm.value());
            break;
        case BEType::ERLANG:
            STORM_LOG_THROW(lambda.has_value(), storm::exceptions::WrongFormatException,
                            "Distribution " << toString(BEType::ERLANG) << " requires parameter 'lambda' for BE '" << name << "'.");
            STORM_LOG_THROW(phases.has_value(), storm::exceptions::WrongFormatException,
                            "Distribution " << toString(BEType::ERLANG) << " requires parameter 'phases' for BE '" << name << "'.");
            if (!dorm.has_value()) {
                STORM_LOG_WARN("No dormancy factor was provided for basic element '" << name << "'. Assuming dormancy factor of 1.");
                dorm = storm::utility::one<ValueType>();
            }
            builder.addBasicElementErlang(name, lambda.value(), phases.value(), dorm.value());
            break;
        case BEType::WEIBULL:
            STORM_LOG_THROW(shape.has_value(), storm::exceptions::WrongFormatException,
                            "Distribution " << toString(BEType::WEIBULL) << " requires parameter 'shape' for BE '" << name << "'.");
            STORM_LOG_THROW(rate.has_value(), storm::exceptions::WrongFormatException,
                            "Distribution " << toString(BEType::WEIBULL) << " requires parameter 'rate' for BE '" << name << "'.");
            builder.addBasicElementWeibull(name, shape.value(), rate.value());
            break;
        case BEType::LOGNORMAL:
            STORM_LOG_THROW(mean.has_value(), storm::exceptions::WrongFormatException,
                            "Distribution " << toString(BEType::LOGNORMAL) << " requires parameter 'mean' for BE '" << name << "'.");
            STORM_LOG_THROW(stddev.has_value(), storm::exceptions::WrongFormatException,
                            "Distribution " << toString(BEType::WEIBULL) << " requires parameter 'stddev' for BE '" << name << "'.");
            builder.addBasicElementLogNormal(name, mean.value(), stddev.value());
            break;
        default:
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "No distribution defined for basic element '" << name << "'.");
            break;
    }
}

// Explicitly instantiate the class.
template class DFTGalileoParser<double>;
template class DFTGalileoParser<RationalFunction>;

}  // namespace parser
}  // namespace storm::dft
