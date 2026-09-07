#pragma once

#include "storm-config.h"
#include "storm/settings/modules/ModuleSettings.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the game solver settings.
 */
class GameSolverSettings : public ModuleSettings {
   public:
    // An enumeration of all available convergence criteria.
    enum class ConvergenceCriterion { Absolute, Relative };

    GameSolverSettings();

    /*!
     * Retrieves whether a game solving technique has been set.
     *
     * @return True iff an equation solving technique has been set.
     */
    bool isGameSolvingMethodSet() const;

    /*!
     * Retrieves whether the game solving technique has been set from the default value.
     *
     * @return True iff the game solving technique has been set from the default value.
     */
    bool isGameSolvingMethodSetFromDefaultValue() const;

    /*!
     * Retrieves the selected game solving technique.
     *
     * @return The selected game solving technique.
     */
    storm::solver::GameMethod getGameSolvingMethod() const;

    /*!
     * Retrieves whether the maximal iteration count has been set.
     *
     * @return True iff the maximal iteration count has been set.
     */
    bool isMaximalIterationCountSet() const;

    /*!
     * Retrieves the maximal number of iterations to perform until giving up on converging.
     *
     * @return The maximal iteration count.
     */
    uint_fast64_t getMaximalIterationCount() const;

    /*!
     * Retrieves whether the precision has been set.
     *
     * @return True iff the precision has been set.
     */
    bool isPrecisionSet() const;

    /*!
     * Retrieves the precision that is used for detecting convergence.
     *
     * @return The precision to use for detecting convergence.
     */
    double getPrecision() const;

    /*!
     * Retrieves whether the convergence criterion has been set.
     *
     * @return True iff the convergence criterion has been set.
     */
    bool isConvergenceCriterionSet() const;

    /*!
     * Retrieves the selected convergence criterion.
     *
     * @return The selected convergence criterion.
     */
    ConvergenceCriterion getConvergenceCriterion() const;

    // The name of the module.
    static const std::string moduleName;

   private:
    static const std::string solvingMethodOptionName;
    static const std::string maximalIterationsOptionName;
    static const std::string maximalIterationsOptionShortName;
    static const std::string precisionOptionName;
    static const std::string absoluteOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
