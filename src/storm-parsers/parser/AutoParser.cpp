#include "storm-parsers/parser/AutoParser.h"

#include "storm-parsers/parser/DeterministicModelParser.h"
#include "storm-parsers/parser/MappedFile.h"
#include "storm-parsers/parser/MarkovAutomatonParser.h"
#include "storm-parsers/parser/NondeterministicModelParser.h"
#include "storm-parsers/util/cstring.h"
#include "storm/adapters/IntervalAdapter.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/utility/macros.h"

namespace storm {
namespace parser {

using namespace storm::utility::cstring;

template<typename ValueType, typename RewardValueType>
std::shared_ptr<storm::models::sparse::Model<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>>
AutoParser<ValueType, RewardValueType>::parseModel(std::string const& transitionsFilename, std::string const& labelingFilename,
                                                   std::string const& stateRewardFilename, std::string const& transitionRewardFilename,
                                                   std::string const& choiceLabelingFilename, ExplicitModelParserOptions const& options) {
    // Find and parse the model type hint.
    storm::models::ModelType type = AutoParser::analyzeHint(transitionsFilename);

    // Do the actual parsing.
    std::shared_ptr<storm::models::sparse::Model<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>> model;
    switch (type) {
        case storm::models::ModelType::Dtmc: {
            model = std::shared_ptr<storm::models::sparse::Model<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>>(
                new storm::models::sparse::Dtmc<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>(
                    std::move(DeterministicModelParser<ValueType, RewardValueType>::parseDtmc(transitionsFilename, labelingFilename, stateRewardFilename,
                                                                                              transitionRewardFilename, choiceLabelingFilename, options))));
            break;
        }
        case storm::models::ModelType::Ctmc: {
            model = std::shared_ptr<storm::models::sparse::Model<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>>(
                new storm::models::sparse::Ctmc<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>(
                    std::move(DeterministicModelParser<ValueType, RewardValueType>::parseCtmc(transitionsFilename, labelingFilename, stateRewardFilename,
                                                                                              transitionRewardFilename, choiceLabelingFilename, options))));
            break;
        }
        case storm::models::ModelType::Mdp: {
            model = std::shared_ptr<storm::models::sparse::Model<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>>(
                new storm::models::sparse::Mdp<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>(
                    std::move(NondeterministicModelParser<ValueType, RewardValueType>::parseMdp(transitionsFilename, labelingFilename, stateRewardFilename,
                                                                                                transitionRewardFilename, choiceLabelingFilename, options))));
            break;
        }
        case storm::models::ModelType::MarkovAutomaton: {
            model = std::shared_ptr<storm::models::sparse::Model<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>>(
                new storm::models::sparse::MarkovAutomaton<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>>(
                    std::move(storm::parser::MarkovAutomatonParser<ValueType, RewardValueType>::parseMarkovAutomaton(
                        transitionsFilename, labelingFilename, stateRewardFilename, transitionRewardFilename, choiceLabelingFilename, options))));
            break;
        }
        default:
            STORM_LOG_WARN("Unknown/Unhandled Model Type which cannot be parsed.");  // Unknown
    }

    return model;
}

template<typename ValueType, typename RewardValueType>
storm::models::ModelType AutoParser<ValueType, RewardValueType>::analyzeHint(std::string const& filename) {
    storm::models::ModelType hintType = storm::models::ModelType::Dtmc;

    // Open the file.
    MappedFile file(filename.c_str());

    STORM_LOG_THROW(file.getDataSize() >= hintLength, storm::exceptions::WrongFormatException, "File too short to be readable.");
    char const* fileData = file.getData();

    char filehintBuffer[hintLength + 1];
    memcpy(filehintBuffer, fileData, hintLength);
    filehintBuffer[hintLength] = 0;

    // Find and read in the hint.
    char hint[hintLength + 1] = {};
    static_assert(hintLength == 10, "sscanf format width must match the hint length");
    constexpr const char* hintFormat = "%10s";
    sscanf(filehintBuffer, hintFormat, hint);

    for (char* c = hint; *c != '\0'; c++) {
        *c = toupper(*c);
    }

    // Check if the hint value is known and store the appropriate enum value.
    if (strcmp(hint, "DTMC") == 0) {
        hintType = storm::models::ModelType::Dtmc;
    } else if (strcmp(hint, "CTMC") == 0) {
        hintType = storm::models::ModelType::Ctmc;
    } else if (strcmp(hint, "MDP") == 0) {
        hintType = storm::models::ModelType::Mdp;
    } else if (strcmp(hint, "MA") == 0) {
        hintType = storm::models::ModelType::MarkovAutomaton;
    } else {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Unable to find model hint in explicit input.");
    }

    return hintType;
}

// Explicitly instantiate the parser.
template class AutoParser<double, double>;

template class AutoParser<double, storm::Interval>;

}  // namespace parser
}  // namespace storm
