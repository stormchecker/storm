#pragma once

#include "storm/modelchecker/exploration/ExplorationTypes.h"
#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the exploration settings.
 */
class ExplorationSettings : public ModuleSettings {
   public:
    using PrecomputationType = storm::modelchecker::exploration_detail::PrecomputationType;
    using NextStateHeuristic = storm::modelchecker::exploration_detail::NextStateHeuristic;

    /*!
     * Creates a new set of exploration settings.
     */
    ExplorationSettings();

    /*!
     * Retrieves whether local precomputation is to be used.
     *
     * @return True iff local precomputation is to be used.
     */
    bool isLocalPrecomputationSet() const;

    /*!
     * Retrieves whether global precomputation is to be used.
     *
     * @return True iff global precomputation is to be used.
     */
    bool isGlobalPrecomputationSet() const;

    /*!
     * Retrieves the selected precomputation type.
     *
     * @return The selected precomputation type.
     */
    PrecomputationType getPrecomputationType() const;

    /*!
     * Retrieves the number of exploration steps to perform until a precomputation is triggered.
     *
     * @return The number of exploration steps to perform until a precomputation is triggered.
     */
    uint_fast64_t getNumberOfExplorationStepsUntilPrecomputation() const;

    /*
     * Retrieves whether the option to perform a precomputation after a given number of sampled paths was set.
     *
     * @return True iff a precomputation after a given number of sampled paths is to be performed.
     */
    bool isNumberOfSampledPathsUntilPrecomputationSet() const;

    /*!
     * Retrieves the number of paths to sample until a precomputation is triggered.
     *
     * @return The the number of paths to sample until a precomputation is triggered.
     */
    uint_fast64_t getNumberOfSampledPathsUntilPrecomputation() const;

    /*!
     * Retrieves the selected next-state heuristic.
     *
     * @return The selected next-state heuristic.
     */
    NextStateHeuristic getNextStateHeuristic() const;

    /*!
     * Retrieves the precision to use for numerical operations.
     *
     * @return The precision to use for numerical operations.
     */
    double getPrecision() const;

    virtual bool check() const override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string precomputationTypeOptionName;
    static const std::string numberOfExplorationStepsUntilPrecomputationOptionName;
    static const std::string numberOfSampledPathsUntilPrecomputationOptionName;
    static const std::string nextStateHeuristicOptionName;
    static const std::string precisionOptionName;
    static const std::string precisionOptionShortName;
};
}  // namespace modules
}  // namespace settings
}  // namespace storm
