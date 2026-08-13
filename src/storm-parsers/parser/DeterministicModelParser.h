#pragma once

#include "storm-parsers/parser/ExplicitModelParserOptions.h"
#include "storm/models/sparse/Ctmc.h"
#include "storm/models/sparse/Dtmc.h"
#include "storm/models/sparse/StandardRewardModel.h"
#include "storm/storage/sparse/ModelComponents.h"

namespace storm {
namespace parser {

/*!
 * Loads a deterministic model (Dtmc or Ctmc) from files.
 *
 * Given the file paths of the files holding the transitions, the atomic propositions and optionally the state- and transition rewards
 * it loads the files, parses them and returns the desired model.
 */
template<typename ValueType = double, typename RewardValueType = double>
class DeterministicModelParser {
   public:
    /*!
     * Parse a Dtmc.
     *
     * This method is an adapter to the actual parsing function.
     * I.e. it uses @parseDeterministicModel internally to parse the given input files, takes its result and compiles it into a Dtmc.
     *
     * @note The number of states of the model is determined by the transitions file.
     *       The labeling file may therefore not contain labels of states that are not contained in the transitions file.
     *
     * @param transitionsFilename The path and name of the file containing the transitions of the model.
     * @param labelingFilename The path and name of the file containing the labels for the states of the model.
     * @param stateRewardFilename The path and name of the file containing the state reward of the model. This file is optional.
     * @param transitionRewardFilename The path and name of the file containing the transition rewards of the model. This file is optional.
     * @param choiceLabelingFilename The path and name of the file containing the choice labeling of the model. This file is optional.
     * @return The parsed Dtmc.
     */
    static storm::models::sparse::Dtmc<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>> parseDtmc(
        std::string const& transitionsFilename, std::string const& labelingFilename, std::string const& stateRewardFilename = "",
        std::string const& transitionRewardFilename = "", std::string const& choiceLabelingFilename = "",
        ExplicitModelParserOptions const& options = ExplicitModelParserOptions());

    /*!
     * Parse a Ctmc.
     *
     * This method is an adapter to the actual parsing function.
     * I.e. it uses @parseDeterministicModel internally to parse the given input files, takes its result and compiles it into a Ctmc.
     *
     * @note The number of states of the model is determined by the transitions file.
     *       The labeling file may therefore not contain labels of states that are not contained in the transitions file.
     *
     * @param transitionsFilename The path and name of the file containing the transitions of the model.
     * @param labelingFilename The path and name of the file containing the labels for the states of the model.
     * @param stateRewardFilename The path and name of the file containing the state reward of the model. This file is optional.
     * @param transitionRewardFilename The path and name of the file containing the transition rewards of the model. This file is optional.
     * @param choiceLabelingFilename The path and name of the file containing the choice labeling of the model. This file is optional.

     * @return The parsed Ctmc.
     */
    static storm::models::sparse::Ctmc<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>> parseCtmc(
        std::string const& transitionsFilename, std::string const& labelingFilename, std::string const& stateRewardFilename = "",
        std::string const& transitionRewardFilename = "", std::string const& choiceLabelingFilename = "",
        ExplicitModelParserOptions const& options = ExplicitModelParserOptions());

   private:
    /*!
     * Parses a deterministic model from the given files.
     * Calls sub-parsers on the given files and fills the container with the results.
     *
     * @note The number of states of the model is determined by the transitions file.
     *       The labeling file may therefore not contain labels of states that are not contained in the transitions file.
     *
     * @param transitionsFilename The path and name of the file containing the transitions of the model.
     * @param labelingFilename The path and name of the file containing the labels for the states of the model.
     * @param stateRewardFilename The path and name of the file containing the state reward of the model. This file is optional.
     * @param transitionRewardFilename The path and name of the file containing the transition rewards of the model. This file is optional.
     * @param choiceLabelingFilename The path and name of the file containing the choice labeling of the model. This file is optional.
     * @return The parsed model encapsulated in a Result structure.
     */
    static storm::storage::sparse::ModelComponents<ValueType, storm::models::sparse::StandardRewardModel<RewardValueType>> parseDeterministicModel(
        std::string const& transitionsFilename, std::string const& labelingFilename, std::string const& stateRewardFilename = "",
        std::string const& transitionRewardFilename = "", std::string const& choiceLabelingFilename = "",
        ExplicitModelParserOptions const& options = ExplicitModelParserOptions());
};

} /* namespace parser */
} /* namespace storm */
