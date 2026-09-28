#pragma once

#include <optional>

#include "storm-config.h"
#include "storm/api/transformation.h"
#include "storm/settings/modules/ModuleSettings.h"

namespace storm {

namespace utility::permutation {
enum class OrderKind;
}

namespace settings::modules {

/*!
 * This class represents the model transformer settings
 */
class TransformationSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of transformer settings.
     */
    TransformationSettings();

    /*!
     * Retrieves whether the option to eliminate chains of non-Markovian states was set.
     *
     * @return True if the option to eliminate chains of non-Markovian states was set.
     */
    bool isChainEliminationSet() const;

    /*!
     * Retrieves how the transformer should deal with labels when non-Markovian states are eliminated
     *
     * @return the label behavior
     */
    storm::transformer::EliminationLabelBehavior getLabelBehavior() const;

    /*!
     * Retrieves whether a DTMC/CTMC should be converted to an MDP/MA
     */
    bool isToNondeterministicModelSet() const;

    /*!
     * Retrieves whether a CTMC/MA should be converted to a DTMC/MDP
     */
    bool isToDiscreteTimeModelSet() const;

    /*!
     * Retrieves whether the target/sink states relevant for the considered properties should be merged into single states before model checking.
     */
    bool isMergeEquivalentStatesSet() const;

    /*!
     * If the returned value is not empty, the model should be permuted according to the given order.
     */
    std::optional<storm::utility::permutation::OrderKind> getModelPermutation() const;

    /*!
     * Retrieves a random seed to be used in case the model shall be permuted randomly.
     */
    std::optional<uint64_t> getModelPermutationSeed() const;

    bool check() const override;

    void finalize() override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string chainEliminationOptionName;
    static const std::string labelBehaviorOptionName;
    static const std::string toNondetOptionName;
    static const std::string toDiscreteTimeOptionName;
    static const std::string mergeEquivalentStatesOptionName;
    static const std::string permuteModelOptionName;
};

}  // namespace settings::modules
}  // namespace storm
