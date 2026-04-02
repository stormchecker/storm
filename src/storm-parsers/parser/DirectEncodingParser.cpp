#include "storm-parsers/parser/DirectEncodingParser.h"

#include <boost/algorithm/string.hpp>
#include <iostream>
#include <istream>
#include <regex>
#include <sstream>
#include <string>

#include "storm-parsers/parser/ValueParser.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/IntervalForward.h"
#include "storm/adapters/RationalFunctionAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/AbortException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/io/ArchiveReader.h"
#include "storm/io/file.h"
#include "storm/utility/SignalHandler.h"
#include "storm/utility/builder.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::parser {

namespace detail {

DirectEncodingValueType valueTypeFromString(std::string const& valueTypeStr) {
    if (valueTypeStr == "double") {
        return DirectEncodingValueType::Double;
    } else if (valueTypeStr == "double-interval") {
        return DirectEncodingValueType::DoubleInterval;
    } else if (valueTypeStr == "rational") {
        return DirectEncodingValueType::Rational;
    } else if (valueTypeStr == "rational-interval") {
        return DirectEncodingValueType::RationalInterval;
    } else if (valueTypeStr == "parametric") {
        return DirectEncodingValueType::Parametric;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Unknown value type '" << valueTypeStr << "'.");
    }
}

std::string toString(DirectEncodingValueType const& vt) {
    using enum DirectEncodingValueType;
    switch (vt) {
        case Default:
            return "default";
        case Double:
            return "double";
        case DoubleInterval:
            return "double-interval";
        case Rational:
            return "rational";
        case RationalInterval:
            return "rational-interval";
        case Parametric:
            return "parametric";
    }
    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected value type.");
}

template<typename ValueType>
std::string valueTypeToString() {
    if constexpr (std::is_same_v<ValueType, double>) {
        return "double";
    } else if constexpr (std::is_same_v<ValueType, storm::RationalNumber>) {
        return "rational";
    } else if constexpr (std::is_same_v<ValueType, storm::Interval>) {
        return "double-interval";
    } else if constexpr (std::is_same_v<ValueType, storm::RationalFunction>) {
        return "parametric";
    } else {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected value type.");
    }
}

template<typename OutputValueType>
bool isCompatibleValueType(DirectEncodingValueType fileValueType) {
    // Return true iff we can output a model with the given OutputValueType, given that the file is encoded using the given fileValueType
    using enum DirectEncodingValueType;
    if (fileValueType == Default) {
        return true;  // If no value type is given, just try to parse and let the value parser throw
    }
    if constexpr (std::is_same_v<OutputValueType, double> || std::is_same_v<OutputValueType, storm::RationalNumber>) {
        return fileValueType == Double || fileValueType == Rational;
    } else if constexpr (std::is_same_v<OutputValueType, storm::Interval>) {
        return fileValueType == Double || fileValueType == Rational || fileValueType == DoubleInterval || fileValueType == RationalInterval;
    } else if constexpr (std::is_same_v<OutputValueType, storm::RationalInterval>) {
        return fileValueType == Double || fileValueType == Rational || fileValueType == DoubleInterval || fileValueType == RationalInterval;
    } else if constexpr (std::is_same_v<OutputValueType, storm::RationalFunction>) {
        return fileValueType == Double || fileValueType == Rational || fileValueType == Parametric;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected output value type." << toString(fileValueType));
    }
}

struct DrnHeader {
    storm::models::ModelType modelType;
    DirectEncodingValueType valueType{DirectEncodingValueType::Default};
    uint64_t nrStates{0}, nrChoices{0};
    std::vector<std::string> parameters;
    std::unordered_map<std::string, std::string> placeholders;
    std::vector<std::string> rewardModelNames;
};

DrnHeader parseHeader(std::istream& file) {
    DrnHeader header;
    // Parse header
    std::string line;
    bool sawModelType{false}, sawParameters{false};
    while (storm::io::getline(file, line)) {
        if (line.empty() || line.starts_with("//")) {
            continue;
        }

        if (line.starts_with("@type: ")) {
            // Parse model type
            STORM_LOG_THROW(!sawModelType, storm::exceptions::WrongFormatException, "Type declared twice");
            header.modelType = storm::models::getModelType(line.substr(7));
            STORM_LOG_TRACE("Model type: " << header.modelType);
            STORM_LOG_THROW(header.modelType != storm::models::ModelType::S2pg, storm::exceptions::NotSupportedException,
                            "Stochastic Two Player Games in DRN format are not supported.");
            sawModelType = true;
        } else if (line.starts_with("@value_type: ")) {
            // Parse value type
            STORM_LOG_THROW(header.valueType == DirectEncodingValueType::Default, storm::exceptions::WrongFormatException, "Value type declared twice");
            header.valueType = valueTypeFromString(line.substr(13));
        } else if (line == "@parameters") {
            // Parse parameters
            STORM_LOG_THROW(!sawParameters, storm::exceptions::WrongFormatException, "Parameters declared twice");
            storm::io::getline(file, line);
            if (line != "") {
                boost::split(header.parameters, line, boost::is_any_of(" "));
            }
            sawParameters = true;
        } else if (line == "@placeholders") {
            // Parse placeholders
            while (storm::io::getline(file, line)) {
                size_t posColon = line.find(':');
                STORM_LOG_THROW(posColon != std::string::npos, storm::exceptions::WrongFormatException, "':' not found.");
                std::string placeName = line.substr(0, posColon - 1);
                STORM_LOG_THROW(placeName.front() == '$', storm::exceptions::WrongFormatException, "Placeholder must start with dollar symbol $.");
                std::string valueStr = line.substr(posColon + 2);
                auto ret = header.placeholders.emplace(placeName.substr(1), valueStr);
                STORM_LOG_THROW(ret.second, storm::exceptions::WrongFormatException, "Placeholder '$" << placeName << "' was already defined before.");
                if (file.peek() == '@') {
                    // Next character is @ -> placeholder definitions ended
                    break;
                }
            }
        } else if (line == "@reward_models") {
            // Parse reward models
            STORM_LOG_THROW(header.rewardModelNames.empty(), storm::exceptions::WrongFormatException, "Reward model names declared twice");
            storm::io::getline(file, line);
            boost::split(header.rewardModelNames, line, boost::is_any_of("\t "));
        } else if (line == "@nr_states") {
            // Parse no. of states
            STORM_LOG_THROW(header.nrStates == 0, storm::exceptions::WrongFormatException, "Number states declared twice");
            storm::io::getline(file, line);
            header.nrStates = parseNumber<size_t>(line);
        } else if (line == "@nr_choices") {
            STORM_LOG_THROW(header.nrChoices == 0, storm::exceptions::WrongFormatException, "Number of actions declared twice");
            storm::io::getline(file, line);
            header.nrChoices = parseNumber<size_t>(line);
        } else if (line == "@model") {
            // Parse rest of the model
            STORM_LOG_THROW(sawModelType, storm::exceptions::WrongFormatException, "Model type has to be declared before model.");
            STORM_LOG_THROW(header.nrStates != 0, storm::exceptions::WrongFormatException, "No. of states has to be declared before model.");
            STORM_LOG_WARN_COND(header.nrChoices != 0, "No. of actions has to be declared. We may continue now, but future versions might not support this.");
            // Done parsing header
            // We might have no @value_type section but a non-empty list of parameters which means that the value type must be parametric.
            if (header.valueType == DirectEncodingValueType::Default && !header.parameters.empty()) {
                header.valueType = DirectEncodingValueType::Parametric;
            }
            return header;
        } else {
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Could not parse line '" << line << "'.");
        }
    }
    // If we reach this point, we reached end of file before @model was found.
    STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Reached end of file before @model was found.");
}

template<typename ValueType>
ValueType parseValue(std::string const& valueStr, std::unordered_map<std::string, ValueType> const& placeholders, ValueParser<ValueType> const& valueParser) {
    if (valueStr.starts_with("$")) {
        auto it = placeholders.find(valueStr.substr(1));
        STORM_LOG_THROW(it != placeholders.end(), storm::exceptions::WrongFormatException, "Placeholder " << valueStr << " unknown.");
        return it->second;
    } else {
        // Use default value parser
        return valueParser.parseValue(valueStr);
    }
}

template<typename ValueType, typename RewardModelType = storm::models::sparse::StandardRewardModel<ValueType>>
std::shared_ptr<storm::models::sparse::Model<ValueType, RewardModelType>> parseModel(std::istream& file, DrnHeader const& header,
                                                                                     DirectEncodingParserOptions const& options) {
    // Initialize value parsing
    STORM_LOG_THROW(
        isCompatibleValueType<ValueType>(header.valueType), storm::exceptions::WrongFormatException,
        "Value type '" << toString(header.valueType) << "' in DRN file is not compatible with output value type '" << valueTypeToString<ValueType>() << "'.");
    STORM_LOG_TRACE("Parsing model with file value type '" << toString(header.valueType) << "' into model with output value type '"
                                                           << valueTypeToString<ValueType>() << "'.");
    ValueParser<ValueType> valueParser;
    for (std::string const& parameter : header.parameters) {
        STORM_LOG_TRACE("New parameter: " << parameter);
        valueParser.addParameter(parameter);
    }
    std::unordered_map<std::string, ValueType> placeholders;
    for (auto const& [placeName, valueStr] : header.placeholders) {
        ValueType v = parseValue(valueStr, placeholders, valueParser);
        STORM_LOG_TRACE("Placeholder " << placeName << " for value " << v);
        placeholders.emplace(placeName, std::move(v));
    }
    size_t const nrStates = header.nrStates;
    size_t const nrChoices = header.nrChoices;
    storm::storage::sparse::ModelComponents<ValueType, RewardModelType> modelComponents;
    bool const nonDeterministic = (header.modelType == storm::models::ModelType::Mdp || header.modelType == storm::models::ModelType::MarkovAutomaton ||
                                   header.modelType == storm::models::ModelType::Pomdp);
    bool const continuousTime = (header.modelType == storm::models::ModelType::Ctmc || header.modelType == storm::models::ModelType::MarkovAutomaton);
    storm::storage::SparseMatrixBuilder<ValueType> builder = storm::storage::SparseMatrixBuilder<ValueType>(0, 0, 0, false, nonDeterministic, 0);
    modelComponents.stateLabeling = storm::models::sparse::StateLabeling(nrStates);
    modelComponents.observabilityClasses = std::vector<uint32_t>();
    modelComponents.observabilityClasses->resize(nrStates);
    if (options.buildChoiceLabeling) {
        STORM_LOG_THROW(nrChoices != 0, storm::exceptions::WrongFormatException,
                        "No. of actions (@nr_choices) has to be declared when building a model with choice labeling.");
        modelComponents.choiceLabeling = storm::models::sparse::ChoiceLabeling(nrChoices);
    }
    std::vector<std::vector<ValueType>> stateRewards;
    std::vector<std::vector<ValueType>> actionRewards;
    if (continuousTime) {
        modelComponents.exitRates = std::vector<ValueType>(nrStates);
        if (header.modelType == storm::models::ModelType::MarkovAutomaton) {
            modelComponents.markovianStates = storm::storage::BitVector(nrStates);
        }
    }
    // We parse rates for continuous time models.
    if (header.modelType == storm::models::ModelType::Ctmc) {
        modelComponents.rateTransitions = true;
    }

    // Iterate over all lines
    std::string line;
    size_t row = 0;
    size_t state = 0;
    uint64_t lineNumber = 0;
    bool firstState = true;
    bool firstActionForState = true;
    while (storm::io::getline(file, line)) {
        lineNumber++;
        if (line.starts_with("//")) {
            continue;
        }
        if (line.empty()) {
            continue;
        }
        STORM_LOG_TRACE("Parsing line no " << lineNumber << " : " << line);
        boost::trim_left(line);
        if (line.starts_with("state ")) {
            // New state
            if (firstState) {
                firstState = false;
            } else {
                ++state;
                ++row;
            }
            firstActionForState = true;
            STORM_LOG_TRACE("New state " << state);
            STORM_LOG_THROW(state <= nrStates, storm::exceptions::WrongFormatException, "More states detected than declared (in @nr_states).");

            // Parse state id
            line = line.substr(6);  // Remove "state "
            std::string curString = line;
            size_t posEnd = line.find(" ");
            if (posEnd != std::string::npos) {
                curString = line.substr(0, posEnd);
                line = line.substr(posEnd + 1);
            } else {
                line = "";
            }
            size_t parsedId = parseNumber<size_t>(curString);
            STORM_LOG_THROW(state == parsedId, storm::exceptions::WrongFormatException,
                            "In line " << lineNumber << " state ids are not ordered and without gaps. Expected " << state << " but got " << parsedId << ".");
            if (nonDeterministic) {
                STORM_LOG_TRACE("new Row Group starts at " << row << ".");
                builder.newRowGroup(row);
                STORM_LOG_THROW(nrChoices == 0 || builder.getCurrentRowGroupCount() <= nrChoices, storm::exceptions::WrongFormatException,
                                "More actions detected than declared (in @nr_choices).");
            }

            if (continuousTime) {
                // Parse exit rate for CTMC or MA
                STORM_LOG_THROW(line.starts_with("!"), storm::exceptions::WrongFormatException, "Exit rate missing in " << lineNumber);
                line = line.substr(1);  // Remove "!"
                curString = line;
                posEnd = line.find(" ");
                if (posEnd != std::string::npos) {
                    curString = line.substr(0, posEnd);
                    line = line.substr(posEnd + 1);
                } else {
                    line = "";
                }
                ValueType exitRate = parseValue(curString, placeholders, valueParser);
                if (header.modelType == storm::models::ModelType::MarkovAutomaton && !storm::utility::isZero<ValueType>(exitRate)) {
                    modelComponents.markovianStates.get().set(state);
                }
                STORM_LOG_TRACE("Exit rate " << exitRate);
                modelComponents.exitRates.get()[state] = exitRate;
            }

            if (header.modelType == storm::models::ModelType::Pomdp) {
                if (line.starts_with("{")) {
                    size_t posEndObservation = line.find("}");
                    std::string observation = line.substr(1, posEndObservation - 1);
                    STORM_LOG_TRACE("State observation " << observation);
                    modelComponents.observabilityClasses.value()[state] = std::stoi(observation);
                    line = line.substr(posEndObservation + 1);
                    if (!line.empty()) {
                        STORM_LOG_THROW(line.starts_with(" "), storm::exceptions::WrongFormatException,
                                        "Expected whitespace after observation in line " << lineNumber);

                        line = line.substr(1);
                    }
                } else {
                    STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Expected an observation for state " << state << " in line " << lineNumber);
                }
            }

            if (line.starts_with("[")) {
                // Parse rewards
                size_t posEndReward = line.find(']');
                STORM_LOG_THROW(posEndReward != std::string::npos, storm::exceptions::WrongFormatException, "] missing in line " << lineNumber << " .");
                std::string rewardsStr = line.substr(1, posEndReward - 1);
                STORM_LOG_TRACE("State rewards: " << rewardsStr);
                std::vector<std::string> rewards;
                boost::split(rewards, rewardsStr, boost::is_any_of(","));
                if (stateRewards.size() < rewards.size()) {
                    stateRewards.resize(rewards.size());
                }
                auto stateRewardsIt = stateRewards.begin();
                for (auto const& rew : rewards) {
                    auto rewardValue = parseValue(rew, placeholders, valueParser);
                    if (!storm::utility::isZero(rewardValue)) {
                        if (stateRewardsIt->empty()) {
                            stateRewardsIt->resize(nrStates, storm::utility::zero<ValueType>());
                        }
                        (*stateRewardsIt)[state] = std::move(rewardValue);
                    }
                    ++stateRewardsIt;
                }
                line = line.substr(posEndReward + 1);
            }

            // Parse labels
            if (!line.empty()) {
                std::vector<std::string> labels;
                // Labels are separated by whitespace and can optionally be enclosed in quotation marks
                // Regex for labels with two cases:
                // * Enclosed in quotation marks: \"([^\"]+?)\"(?=(\s|$|\"))
                //   - First part matches string enclosed in quotation marks with no quotation mark inbetween (\"([^\"]+?)\")
                //   - second part is lookahead which ensures that after the matched part either whitespace, end of line or a new quotation mark follows
                //   (?=(\s|$|\"))
                // * Separated by whitespace: [^\s\"]+?(?=(\s|$))
                //   - First part matches string without whitespace and quotation marks [^\s\"]+?
                //   - Second part is again lookahead matching whitespace or end of line (?=(\s|$))
                std::regex labelRegex(R"(\"([^\"]+?)\"(?=(\s|$|\"))|([^\s\"]+?(?=(\s|$))))");

                // Iterate over matches
                auto match_begin = std::sregex_iterator(line.begin(), line.end(), labelRegex);
                auto match_end = std::sregex_iterator();
                for (std::sregex_iterator i = match_begin; i != match_end; ++i) {
                    std::smatch match = *i;
                    // Find matched group and add as label
                    if (match.length(1) > 0) {
                        labels.push_back(match.str(1));
                    } else {
                        labels.push_back(match.str(3));
                    }
                }

                for (std::string const& label : labels) {
                    if (!modelComponents.stateLabeling.containsLabel(label)) {
                        modelComponents.stateLabeling.addLabel(label);
                    }
                    modelComponents.stateLabeling.addLabelToState(label, state);
                    STORM_LOG_TRACE("New label: '" << label << "'");
                }
            }
        } else if (line.starts_with("action ")) {
            // New action
            if (firstActionForState) {
                firstActionForState = false;
            } else {
                ++row;
            }
            STORM_LOG_TRACE("New action: " << row);
            line = line.substr(7);
            std::string curString = line;
            size_t posEnd = line.find(" ");
            if (posEnd != std::string::npos) {
                curString = line.substr(0, posEnd);
                line = line.substr(posEnd + 1);
            } else {
                line = "";
            }

            // curString contains action name.
            if (options.buildChoiceLabeling) {
                if (curString != "__NOLABEL__") {
                    if (!modelComponents.choiceLabeling.value().containsLabel(curString)) {
                        modelComponents.choiceLabeling.value().addLabel(curString);
                    }
                    modelComponents.choiceLabeling.value().addLabelToChoice(curString, row);
                }
            }
            // Check for rewards
            if (line.starts_with("[")) {
                // Rewards found
                size_t posEndReward = line.find(']');
                STORM_LOG_THROW(posEndReward != std::string::npos, storm::exceptions::WrongFormatException, "] missing.");
                std::string rewardsStr = line.substr(1, posEndReward - 1);
                STORM_LOG_TRACE("Action rewards: " << rewardsStr);
                std::vector<std::string> rewards;
                boost::split(rewards, rewardsStr, boost::is_any_of(","));
                if (actionRewards.size() < rewards.size()) {
                    actionRewards.resize(rewards.size());
                }
                auto actionRewardsIt = actionRewards.begin();
                for (auto const& rew : rewards) {
                    auto rewardValue = parseValue(rew, placeholders, valueParser);
                    if (!storm::utility::isZero(rewardValue)) {
                        if (actionRewardsIt->size() <= row) {
                            actionRewardsIt->resize(std::max(row + 1, nrStates), storm::utility::zero<ValueType>());
                        }
                        (*actionRewardsIt)[row] = std::move(rewardValue);
                    }
                    ++actionRewardsIt;
                }
                line = line.substr(posEndReward + 1);
            }

        } else {
            // New transition
            size_t posColon = line.find(':');
            STORM_LOG_THROW(posColon != std::string::npos, storm::exceptions::WrongFormatException,
                            "':' not found in '" << line << "' on line " << lineNumber << ".");
            size_t target = parseNumber<size_t>(line.substr(0, posColon - 1));
            std::string valueStr = line.substr(posColon + 2);
            ValueType value = parseValue(valueStr, placeholders, valueParser);
            STORM_LOG_TRACE("Transition " << row << " -> " << target << ": " << value);
            STORM_LOG_THROW(target < nrStates, storm::exceptions::WrongFormatException,
                            "In line " << lineNumber << " target state " << target << " is greater than state size " << nrStates);
            builder.addNextValue(row, target, value);
        }

        if (storm::utility::resources::isTerminate()) {
            std::cout << "Parsed " << state << "/" << nrStates << " states before abort.\n";
            STORM_LOG_THROW(false, storm::exceptions::AbortException, "Aborted in state space exploration.");
            break;
        }

    }  // end state iteration
    STORM_LOG_TRACE("Finished parsing");

    if (nonDeterministic) {
        STORM_LOG_THROW(nrChoices == 0 || builder.getLastRow() + 1 == nrChoices, storm::exceptions::WrongFormatException,
                        "Number of actions detected (at least " << builder.getLastRow() + 1 << ") does not match number of actions declared (" << nrChoices
                                                                << ", in @nr_choices).");
    }

    // Build transition matrix
    modelComponents.transitionMatrix = builder.build(row + 1, nrStates, nonDeterministic ? nrStates : 0);
    STORM_LOG_TRACE("Built matrix");

    // Build reward models
    uint64_t numRewardModels = std::max(stateRewards.size(), actionRewards.size());
    for (uint64_t i = 0; i < numRewardModels; ++i) {
        std::string rewardModelName;
        if (header.rewardModelNames.size() <= i) {
            rewardModelName = "rew" + std::to_string(i);
        } else {
            rewardModelName = header.rewardModelNames[i];
        }
        std::optional<std::vector<ValueType>> stateRewardVector, actionRewardVector;
        if (i < stateRewards.size() && !stateRewards[i].empty()) {
            stateRewardVector = std::move(stateRewards[i]);
        }
        if (i < actionRewards.size() && !actionRewards[i].empty()) {
            actionRewards[i].resize(row + 1, storm::utility::zero<ValueType>());
            actionRewardVector = std::move(actionRewards[i]);
        }
        modelComponents.rewardModels.emplace(
            rewardModelName, storm::models::sparse::StandardRewardModel<ValueType>(std::move(stateRewardVector), std::move(actionRewardVector)));
    }
    STORM_LOG_TRACE("Built reward models");
    return storm::utility::builder::buildModelFromComponents(header.modelType, std::move(modelComponents));
}

std::shared_ptr<storm::models::ModelBase> parseModel(DirectEncodingValueType valueType, std::istream& file, DrnHeader const& header,
                                                     DirectEncodingParserOptions const& options) {
    // Derive output model value type
    using enum DirectEncodingValueType;
    if (valueType == Default) {
        valueType = header.valueType;
    }
    STORM_LOG_THROW(valueType != Default, storm::exceptions::WrongFormatException, "Value type cannot be derived from file and is not provided as argument.");
    // Potentially promote to interval
    if (header.valueType == DoubleInterval || header.valueType == RationalInterval) {
        if (valueType == Double) {
            valueType = DoubleInterval;
        } else if (valueType == Rational) {
            valueType = RationalInterval;
        }
    }
    // Derive value type for parsing
    switch (valueType) {
        case Double:
            return parseModel<double>(file, header, options);
        case Rational:
            return parseModel<storm::RationalNumber>(file, header, options);
        case DoubleInterval:
            return parseModel<storm::Interval>(file, header, options);
        case Parametric:
            return parseModel<storm::RationalFunction>(file, header, options);
        default:
            STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Unsupported value type " << toString(valueType) << ".");
    }
}

auto openFileAsInputStream(std::filesystem::path const& file, auto&& f) {
    auto archiveReader = storm::io::openArchive(file);
    // Load file either as archive or as regular file
    if (archiveReader.isReadableArchive()) {
        std::optional<std::istringstream> filestream;
        for (auto entry : archiveReader) {
            if (!entry.isDir()) {
                STORM_LOG_THROW(!filestream.has_value(), storm::exceptions::WrongFormatException, "Multiple files in archive " << file << ".");
                STORM_LOG_INFO("Reading file " << entry.name() << " from archive " << file);
                // Loads the entire content into memory.
                filestream.emplace(entry.toString());
            }
        }
        STORM_LOG_THROW(filestream.has_value(), storm::exceptions::WrongFormatException, "Empty archive " << file << ".");
        return f(filestream.value());
    } else {
        STORM_LOG_INFO("Reading from file " << file);
        std::ifstream filestream;
        storm::io::openFile(file, filestream);
        auto res = f(filestream);
        storm::io::closeFile(filestream);
        return res;
    }
}

}  // namespace detail

template<typename ValueType, typename RewardModelType>
std::shared_ptr<storm::models::sparse::Model<ValueType, RewardModelType>> parseDirectEncodingModel(std::filesystem::path const& file,
                                                                                                   DirectEncodingParserOptions const& options) {
    static_assert(std::is_same_v<ValueType, typename RewardModelType::ValueType>, "ValueType and RewardModelType::ValueType are assumed to be the same.");
    return detail::openFileAsInputStream(file, [&options](std::istream& filestream) {
        auto header = detail::parseHeader(filestream);
        return detail::parseModel<ValueType, RewardModelType>(filestream, header, options);
    });
}

std::shared_ptr<storm::models::ModelBase> parseDirectEncodingModel(std::filesystem::path const& file, DirectEncodingValueType valueType,
                                                                   DirectEncodingParserOptions const& options) {
    return detail::openFileAsInputStream(file, [&valueType, &options](std::istream& filestream) {
        auto header = detail::parseHeader(filestream);
        return detail::parseModel(valueType, filestream, header, options);
    });
}

// Template instantiations.
template std::shared_ptr<storm::models::sparse::Model<double>> parseDirectEncodingModel<double>(std::filesystem::path const& file,
                                                                                                DirectEncodingParserOptions const& options);
template std::shared_ptr<storm::models::sparse::Model<storm::RationalNumber>> parseDirectEncodingModel<storm::RationalNumber>(
    std::filesystem::path const& file, DirectEncodingParserOptions const& options);
template std::shared_ptr<storm::models::sparse::Model<storm::Interval>> parseDirectEncodingModel<storm::Interval>(std::filesystem::path const& file,
                                                                                                                  DirectEncodingParserOptions const& options);
template std::shared_ptr<storm::models::sparse::Model<storm::RationalInterval>> parseDirectEncodingModel<storm::RationalInterval>(
    std::filesystem::path const& file, DirectEncodingParserOptions const& options);
template std::shared_ptr<storm::models::sparse::Model<storm::RationalFunction>> parseDirectEncodingModel<storm::RationalFunction>(
    std::filesystem::path const& file, DirectEncodingParserOptions const& options);

}  // namespace storm::parser
