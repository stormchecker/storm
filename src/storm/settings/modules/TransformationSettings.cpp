#include "TransformationSettings.h"

#include "storm/exceptions/IllegalArgumentValueException.h"
#include "storm/exceptions/InvalidSettingsException.h"
#include "storm/settings/ArgumentBuilder.h"
#include "storm/settings/Option.h"
#include "storm/settings/OptionBuilder.h"
#include "storm/utility/permutation.h"

namespace storm {
namespace settings {
namespace modules {

const std::string TransformationSettings::moduleName = "transformation";

const std::string TransformationSettings::chainEliminationOptionName = "eliminate-chains";
const std::string TransformationSettings::labelBehaviorOptionName = "ec-label-behavior";
const std::string TransformationSettings::toNondetOptionName = "to-nondet";
const std::string TransformationSettings::toDiscreteTimeOptionName = "to-discrete";
const std::string TransformationSettings::mergeEquivalentStatesOptionName = "merge-equivalent-states";
const std::string TransformationSettings::permuteModelOptionName = "permute";

TransformationSettings::TransformationSettings() : ModuleSettings(moduleName) {
    this->addOption(storm::settings::OptionBuilder(moduleName, chainEliminationOptionName, false,
                                                   "If set, chains of non-Markovian states are eliminated if the resulting model is a Markov Automaton.")
                        .setIsAdvanced()
                        .build());
    std::vector<std::string> labelBehavior;
    labelBehavior.push_back("keep");
    labelBehavior.push_back("merge");
    labelBehavior.push_back("delete");
    this->addOption(
        storm::settings::OptionBuilder(moduleName, labelBehaviorOptionName, false,
                                       "Sets the behavior of labels for all non-Markovian states. Some options may cause wrong results.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument(
                             "behavior",
                             "The behavior how the transformer handles labels of non-Markovian states. 'keep' does not eliminate states with different labels, "
                             "'merge' builds the union of labels of all eliminated states, 'delete' only keeps the labels of the last state.")
                             .setDefaultValueString("keep")
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(labelBehavior))
                             .build())
            .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, toNondetOptionName, false,
                                                   "If set, DTMCs/CTMCs are converted to MDPs/MAs (without actual nondeterminism) before model checking.")
                        .setIsAdvanced()
                        .build());
    this->addOption(storm::settings::OptionBuilder(moduleName, toDiscreteTimeOptionName, false,
                                                   "If set, CTMCs/MAs are converted to DTMCs/MDPs (which might or might not preserve the provided properties).")
                        .setIsAdvanced()
                        .build());
    this->addOption(
        storm::settings::OptionBuilder(moduleName, mergeEquivalentStatesOptionName, false,
                                       "If set, merges states that are equivalent w.r.t. the considered property into a single state before model checking.")
            .setIsAdvanced()
            .build());

    this->addOption(
        storm::settings::OptionBuilder(moduleName, permuteModelOptionName, false, "Permutes the build model w.r.t. the given order.")
            .setIsAdvanced()
            .addArgument(storm::settings::ArgumentBuilder::createStringArgument("order", "The order.")
                             .addValidatorString(ArgumentValidatorFactory::createMultipleChoiceValidator(storm::utility::permutation::orderKinds()))
                             .setDefaultValueString(storm::utility::permutation::orderKindtoString(storm::utility::permutation::OrderKind::Bfs))
                             .build())
            .addArgument(storm::settings::ArgumentBuilder::createUnsignedIntegerArgument("seed", "An optional seed, makes random order deterministic.")
                             .setDefaultValueUnsignedInteger(0)
                             .makeOptional()
                             .build())
            .build());
}

bool TransformationSettings::isChainEliminationSet() const {
    return this->getOption(chainEliminationOptionName).getHasOptionBeenSet();
}

storm::transformer::EliminationLabelBehavior TransformationSettings::getLabelBehavior() const {
    std::string labelBehaviorAsString = this->getOption(labelBehaviorOptionName).getArgumentByName("behavior").getValueAsString();
    if (labelBehaviorAsString == "keep") {
        return storm::transformer::EliminationLabelBehavior::KeepLabels;
    } else if (labelBehaviorAsString == "merge") {
        return storm::transformer::EliminationLabelBehavior::MergeLabels;
    } else if (labelBehaviorAsString == "delete") {
        return storm::transformer::EliminationLabelBehavior::DeleteLabels;
    }
    STORM_LOG_THROW(false, storm::exceptions::IllegalArgumentValueException,
                    "Illegal value '" << labelBehaviorAsString << "' set as label behavior for the elimination.");
}

bool TransformationSettings::isToNondeterministicModelSet() const {
    return this->getOption(toNondetOptionName).getHasOptionBeenSet();
}

bool TransformationSettings::isToDiscreteTimeModelSet() const {
    return this->getOption(toDiscreteTimeOptionName).getHasOptionBeenSet();
}

bool TransformationSettings::isMergeEquivalentStatesSet() const {
    return this->getOption(mergeEquivalentStatesOptionName).getHasOptionBeenSet();
}

std::optional<storm::utility::permutation::OrderKind> TransformationSettings::getModelPermutation() const {
    if (this->getOption(permuteModelOptionName).getHasOptionBeenSet()) {
        return storm::utility::permutation::orderKindFromString(this->getOption(permuteModelOptionName).getArgumentByName("order").getValueAsString());
    }
    return std::nullopt;
}

std::optional<uint64_t> TransformationSettings::getModelPermutationSeed() const {
    if (this->getOption(permuteModelOptionName).getHasOptionBeenSet()) {
        if (auto const& arg = this->getOption(permuteModelOptionName).getArgumentByName("seed"); arg.getHasBeenSet()) {
            return arg.getValueAsUnsignedInteger();
        }
    }
    return std::nullopt;
}

bool TransformationSettings::check() const {
    // Ensure that labeling preservation is only set if chain elimination is set
    STORM_LOG_THROW(isChainEliminationSet() || !this->getOption(labelBehaviorOptionName).getHasOptionBeenSet(), storm::exceptions::InvalidSettingsException,
                    "Label preservation can only be chosen if chain elimination is applied.");

    // If there is a seed, the permutation order shall be random
    STORM_LOG_WARN_COND(!getModelPermutationSeed().has_value() || getModelPermutation() == storm::utility::permutation::OrderKind::Random,
                        "Random seed is given for permutation order, but the order is not random. Seed will be ignored.");

    return true;
}

void TransformationSettings::finalize() {
    // Intentionally left empty
}

}  // namespace modules
}  // namespace settings
}  // namespace storm
