#pragma once

#include "storm-config.h"
#include "storm/builder/ExplorationOrder.h"
#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {
class BuildSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of core settings.
     */
    BuildSettings();

    /*!
     * Retrieves whether the model exploration order was set.
     *
     * @return True if the model exploration option was set.
     */
    bool isExplorationOrderSet() const;

    /*!
     * Retrieves whether to perform additional checks during model exploration (e.g. out-of-bounds, etc.).
     *
     * @return True if additional checks are to be performed.
     */
    bool isExplorationChecksSet() const;

    /*!
     * Retrieves the exploration order if it was set.
     *
     * @return The chosen exploration order.
     */
    storm::builder::ExplorationOrder getExplorationOrder() const;

    /*!
     * Retrieves whether the PRISM compatibility mode was enabled.
     *
     * @return True iff the PRISM compatibility mode was enabled.
     */
    bool isPrismCompatibilityEnabled() const;

    /*!
     * Retrieves whether the dont-fix-deadlocks option was set.
     *
     * @return True if the dont-fix-deadlocks option was set.
     */
    bool isDontFixDeadlocksSet() const;

    /*!
     * Overrides the option to not fix deadlocks by setting it to the specified value. As soon as the
     * returned memento goes out of scope, the original value is restored.
     *
     * @param stateToSet The value that is to be set for the fix-deadlocks option.
     * @return The memento that will eventually restore the original value.
     */
    std::unique_ptr<storm::settings::SettingMemento> overrideDontFixDeadlocksSet(bool stateToSet);

    /**
     * Retrieves whether no model should be build at all, in case one just want to translate models or parse a file.
     */
    bool isNoBuildModelSet() const;

    /*!
     * Retrieves whether the full model should be build, that is, the model including all labels and rewards.
     *
     * @return true iff the full model should be build.
     */
    bool isBuildFullModelSet() const;

    /*!
     * Retrieves whether the maximum progress assumption is to be applied when building the model
     */
    bool isApplyNoMaximumProgressAssumptionSet() const;

    /*!
     * Retrieves whether Markov automata should be checked for the presence of Zeno cycles after building.
     */
    bool isCheckZenoSet() const;

    /*!
     * Retrieves whether the choice labels should be build
     */
    bool isBuildChoiceLabelsSet() const;

    /*!
     * Retrieves whether the choice origins should be build
     */
    bool isBuildChoiceOriginsSet() const;

    /*!
     * Retrieves whether the choice labels should be build
     * @return
     */
    bool isBuildStateValuationsSet() const;

    /*!
     * Retrieves whether the observation valuations should be build. Only relevant for POMDPs
     * @return
     */
    bool isBuildObservationValuationsSet() const;

    /*!
     * Retrieves whether out of bounds state should be added
     * @return
     */
    bool isBuildOutOfBoundsStateSet() const;

    /*!
     * Retrieves whether to build the overlapping label
     */
    bool isAddOverlappingGuardsLabelSet() const;

    /*!
     * Retrieves whether all labels should be build
     */
    bool isBuildAllLabelsSet() const;

    /*!
     * Retrieves the number of bits that should be used to represent unbounded integer variables
     * @return
     */
    uint64_t getBitsForUnboundedVariables() const;

    /*!
     * Retrieves whether simplification of symbolic inputs through static analysis shall be disabled
     */
    bool isNoSimplifySet() const;

    /*!
     * Retrieves whether location elimination is enabled
     */
    bool isLocationEliminationSet() const;

    /*!
     * Retrieves the location parameter of the location elimination heuristic.
     */
    uint64_t getLocationEliminationLocationHeuristic() const;

    /*!
     * Retrieves the edge parameter of the location elimination heuristic.
     */
    uint64_t getLocationEliminationEdgesHeuristic() const;

    /*!
     * Retrieves whether an exploration state limit has been set in which case state space exploration might be stopped once the specified number is exceeded.
     */
    bool isExplorationStateLimitSet() const;

    /*!
     * Retrieves the state limit (if set). State space exploration might be stopped once the specified number is exceeded.
     */
    uint64_t getExplorationStateLimit() const;

    // The name of the module.
    static const std::string moduleName;
};
}  // namespace modules
}  // namespace settings
}  // namespace storm