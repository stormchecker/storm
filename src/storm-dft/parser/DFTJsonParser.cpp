#include "storm-dft/parser/DFTJsonParser.h"

#include <boost/algorithm/string.hpp>

#include "storm-dft/builder/DFTBuilder.h"
#include "storm-dft/utility/RelevantEvents.h"
#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/io/file.h"
#include "storm/utility/macros.h"

namespace storm::dft {
namespace parser {

template<typename ValueType>
storm::dft::storage::DFT<ValueType> DFTJsonParser<ValueType>::parseJsonFromFile(std::string const& filename) {
    std::ifstream file;
    storm::io::openFile(filename, file);
    Json jsonInput;
    file >> jsonInput;
    storm::io::closeFile(file);
    return parseJson(jsonInput);
}

template<typename ValueType>
storm::dft::storage::DFT<ValueType> DFTJsonParser<ValueType>::parseJsonFromString(std::string const& jsonString) {
    Json jsonInput = Json::parse(jsonString);
    return parseJson(jsonInput);
}

template<typename ValueType>
storm::dft::storage::DFT<ValueType> DFTJsonParser<ValueType>::parseJson(Json const& jsonInput) {
    // Initialize DFT builder and value parser
    storm::dft::builder::DFTBuilder<ValueType> builder;
    storm::parser::ValueParser<ValueType> valueParser;
    std::string toplevelName = "";
    storm::dft::utility::RelevantEvents relevantEvents;

    std::string currentLocation;
    try {
        // Try to parse parameters
        currentLocation = "parameters";
        if (jsonInput.count("parameters") > 0) {
            Json parameters = jsonInput.at("parameters");
            STORM_LOG_THROW(parameters.empty() || (std::is_same<ValueType, storm::RationalFunction>::value), storm::exceptions::NotSupportedException,
                            "Parameters are only allowed when using rational functions.");
            for (auto const& parameter : parameters) {
                valueParser.addParameter(parseValue(parameter));
            }
        }

        currentLocation = "nodes";
        Json const& nodes = jsonInput.at("nodes");
        // Start by building mapping from ids to their unique names
        std::map<std::string, std::string> nameMapping;
        std::set<std::string> names;
        for (auto const& element : nodes) {
            Json data = element.at("data");
            std::string id = data.at("id");
            std::string name = parseName(data.at("name"));
            STORM_LOG_THROW(names.find(name) == names.end(), storm::exceptions::WrongFormatException, "Element '" << name << "' was already declared.");
            names.insert(name);
            nameMapping[id] = name;
        }

        // Parse nodes
        for (auto const& element : nodes) {
            currentLocation = parseValue(element);
            Json const& data = element.at("data");
            std::string name = parseName(data.at("name"));
            // TODO: use contains() if modernjson is updated
            if (data.count("relevant") > 0) {
                bool isRelevant = data.at("relevant");
                if (isRelevant) {
                    relevantEvents.insert(name);
                }
            }
            // Create list of children
            std::vector<std::string> childNames;
            // TODO: use contains() if modernjson is updated
            if (data.count("children") > 0) {
                for (auto const& child : data.at("children")) {
                    STORM_LOG_THROW(nameMapping.find(child) != nameMapping.end(), storm::exceptions::WrongFormatException,
                                    "Child '" << child << "' for element '" << name << "' was not defined.");
                    childNames.push_back(nameMapping.at(child));
                }
            }

            std::string type = data.at("type");
            if (type == "and") {
                builder.addAndGate(name, childNames);
            } else if (type == "or") {
                builder.addOrGate(name, childNames);
            } else if (type == "vot") {
                STORM_LOG_THROW(data.count("voting") > 0, storm::exceptions::WrongFormatException, "Voting gate '" << name << "' requires parameter 'voting'.");
                std::string votThreshold = parseValue(data.at("voting"));
                builder.addVotingGate(name, storm::parser::parseNumber<size_t>(votThreshold), childNames);
            } else if (type == "pand") {
                if (data.count("inclusive") > 0) {
                    bool inclusive = data.at("inclusive");
                    builder.addPandGate(name, childNames, inclusive);
                } else {
                    builder.addPandGate(name, childNames);
                }
            } else if (type == "por") {
                if (data.count("inclusive") > 0) {
                    bool inclusive = data.at("inclusive");
                    builder.addPorGate(name, childNames, inclusive);
                } else {
                    builder.addPorGate(name, childNames);
                }
            } else if (type == "spare") {
                builder.addSpareGate(name, childNames);
            } else if (type == "seq") {
                builder.addSequenceEnforcer(name, childNames);
            } else if (type == "mutex") {
                builder.addMutex(name, childNames);
            } else if (type == "fdep") {
                builder.addPdep(name, childNames, storm::utility::one<ValueType>());
            } else if (type == "pdep") {
                STORM_LOG_THROW(data.count("probability") > 0, storm::exceptions::WrongFormatException,
                                "PDEP '" << name << "' requires parameter 'probability'.");
                ValueType probability = valueParser.parseValue(parseValue(data.at("probability")));
                if (storm::utility::isZero<ValueType>(probability)) {
                    // Skip element. Otherwise, trying to add layout information later on will fail
                    STORM_LOG_WARN("Dependency " << name << " with probability 0 is superfluous and will not be added.");
                    continue;
                }
                builder.addPdep(name, childNames, probability);
            } else if (boost::starts_with(type, "be")) {
                parseBasicElement(name, type, data, builder, valueParser);
            } else if (type == "compound") {
                STORM_LOG_TRACE("Ignoring compound node '" << name << "'.");
            } else {
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Type name '" << type << "' not recognized.");
            }

            if (element.find("position") != element.end()) {
                // Set layout positions
                Json position = element.at("position");
                double x = position.at("x");
                double y = position.at("y");
                builder.addLayoutInfo(name, x / 7, y / 7);
            }
        }

        STORM_LOG_THROW(jsonInput.count("toplevel") > 0, storm::exceptions::WrongFormatException,
                        "Top level element must be specified via parameter 'toplevel'.");
        std::string topLevelId = parseValue(jsonInput.at("toplevel"));
        STORM_LOG_THROW(nameMapping.find(topLevelId) != nameMapping.end(), storm::exceptions::WrongFormatException,
                        "Top level element with id '" << topLevelId << "' was not defined.");
        builder.setTopLevel(nameMapping.at(topLevelId));

    } catch (storm::exceptions::BaseException const& exception) {
        STORM_LOG_THROW(false, storm::exceptions::FileIoException, "A parsing exception occurred in " << currentLocation << ": " << exception.what() << ".");
    } catch (std::exception const& exception) {
        STORM_LOG_THROW(false, storm::exceptions::FileIoException,
                        "An exception occurred during parsing in " << currentLocation << ": " << exception.what() << ".");
    }

    // Build DFT
    storm::dft::storage::DFT<ValueType> dft = builder.build();
    // Set relevant events
    dft.setRelevantEvents(relevantEvents, false);
    STORM_LOG_DEBUG("Relevant events: " << dft.getRelevantEventsString());
    return dft;
}

template<typename ValueType>
std::string DFTJsonParser<ValueType>::parseName(std::string const& name) {
    std::string newName = name;
    std::replace(newName.begin(), newName.end(), ' ', '_');
    std::replace(newName.begin(), newName.end(), '-', '_');
    return newName;
}

template<typename ValueType>
std::string DFTJsonParser<ValueType>::parseValue(Json value) {
    if (value.is_string()) {
        return value.get<std::string>();
    } else {
        std::stringstream stream;
        stream << value;
        return stream.str();
    }
}

template<typename ValueType>
void DFTJsonParser<ValueType>::parseBasicElement(std::string const& name, std::string const& type, Json input,
                                                 storm::dft::builder::DFTBuilder<ValueType>& builder, storm::parser::ValueParser<ValueType>& valueParser) {
    std::string distribution = "exponential";  // Default is exponential distribution
    if (input.count("distribution") > 0) {
        distribution = input.at("distribution");
        // Handle short-form for exponential distribution
        if (distribution == "exp") {
            distribution = "exponential";
        }
    }
    STORM_LOG_THROW(type == "be" || (type == "be_exp" && distribution == "exponential"), storm::exceptions::WrongFormatException,
                    "BE type '" << type << "' and distribution '" << distribution << " do not agree.");

    if (distribution == "const") {
        // Constant failed/failsafe
        STORM_LOG_THROW(input.count("failed") > 0, storm::exceptions::WrongFormatException, "Constant BE '" << name << "' requires parameter 'failed'.");
        bool failed = input.at("failed");
        builder.addBasicElementConst(name, failed);
    } else if (distribution == "probability") {
        // Constant probability distribution
        STORM_LOG_THROW(input.count("prob") > 0, storm::exceptions::WrongFormatException,
                        "BE '" << name << "' with probability distribution requires parameter 'prob'.");
        ValueType probability = valueParser.parseValue(parseValue(input.at("prob")));
        ValueType dormancy = storm::utility::one<ValueType>();
        if (input.count("dorm") > 0) {
            dormancy = valueParser.parseValue(parseValue(input.at("dorm")));
        } else {
            STORM_LOG_WARN("No dormancy factor was provided for basic element '" << name << "'. Assuming dormancy factor of 1.");
        }
        builder.addBasicElementProbability(name, probability, dormancy);
    } else if (distribution == "exponential") {
        // Exponential distribution
        STORM_LOG_THROW(input.count("rate") > 0, storm::exceptions::WrongFormatException,
                        "BE '" << name << "' with exponential distribution requires parameter 'rate'.");
        ValueType rate = valueParser.parseValue(parseValue(input.at("rate")));
        ValueType dormancy = storm::utility::one<ValueType>();
        if (input.count("dorm") > 0) {
            dormancy = valueParser.parseValue(parseValue(input.at("dorm")));
        } else {
            STORM_LOG_WARN("No dormancy factor was provided for basic element '" << name << "'. Assuming dormancy factor of 1.");
        }
        bool transient = false;
        if (input.count("transient") > 0) {
            transient = input.at("transient");
        }
        builder.addBasicElementExponential(name, rate, dormancy, transient);
    } else if (distribution == "erlang") {
        // Erlang distribution
        STORM_LOG_THROW(input.count("rate") > 0, storm::exceptions::WrongFormatException,
                        "BE '" << name << "' with Erlang distribution requires parameter 'rate'.");
        ValueType rate = valueParser.parseValue(parseValue(input.at("rate")));
        STORM_LOG_THROW(input.count("phases") > 0, storm::exceptions::WrongFormatException,
                        "BE '" << name << "' with Erlang distribution requires parameter 'phases'.");
        size_t phases = storm::parser::parseNumber<size_t>(parseValue(input.at("phases")));
        ValueType dormancy = storm::utility::one<ValueType>();
        if (input.count("dorm") > 0) {
            dormancy = valueParser.parseValue(parseValue(input.at("dorm")));
        } else {
            STORM_LOG_WARN("No dormancy factor was provided for basic element '" << name << "'. Assuming dormancy factor of 1.");
        }
        builder.addBasicElementErlang(name, rate, phases, dormancy);
    } else if (distribution == "weibull") {
        // Weibull distribution
        STORM_LOG_THROW(input.count("shape") > 0, storm::exceptions::WrongFormatException,
                        "BE '" << name << "' with Weibull distribution requires parameter 'shape'.");
        ValueType shape = valueParser.parseValue(parseValue(input.at("shape")));
        STORM_LOG_THROW(input.count("rate") > 0, storm::exceptions::WrongFormatException,
                        "BE '" << name << "' with Weibull distribution requires parameter 'rate'.");
        ValueType rate = valueParser.parseValue(parseValue(input.at("rate")));
        builder.addBasicElementWeibull(name, shape, rate);
    } else if (distribution == "lognormal") {
        // Log-normal distribution
        STORM_LOG_THROW(input.count("mean") > 0, storm::exceptions::WrongFormatException,
                        "BE '" << name << "' with Log-normal distribution requires parameter 'mean'.");
        ValueType mean = valueParser.parseValue(parseValue(input.at("mean")));
        STORM_LOG_THROW(input.count("stddev") > 0, storm::exceptions::WrongFormatException,
                        "BE '" << name << "' with Log-normal distribution requires parameter 'stddev'.");
        ValueType stddev = valueParser.parseValue(parseValue(input.at("stddev")));
        builder.addBasicElementLogNormal(name, mean, stddev);
    } else {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Distribution " << distribution << " not known.");
    }
}

// Explicitly instantiate the class.
template class DFTJsonParser<double>;
template class DFTJsonParser<RationalFunction>;

}  // namespace parser
}  // namespace storm::dft
