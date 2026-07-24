#pragma once

#include "storm/settings/modules/ModuleSettings.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for gmm++.
 */
class GmmxxEquationSolverSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of gmm++ settings.
     */
    GmmxxEquationSolverSettings();

    /*!
     * Retrieves whether the linear equation system method has been set.
     *
     * @return True iff the linear equation system method has been set.
     */
    bool isLinearEquationSystemMethodSet() const;

    /*!
     * Retrieves the method that is to be used for solving systems of linear equations.
     *
     * @return The method to use.
     */
    storm::solver::GmmxxLinearEquationSolverMethod getLinearEquationSystemMethod() const;

    /*!
     * Retrieves whether the preconditioning method has been set.
     *
     * @return True iff the preconditioning method has been set.
     */
    bool isPreconditioningMethodSet() const;

    /*!
     * Retrieves the method that is to be used for preconditioning solving systems of linear equations.
     *
     * @return The method to use.
     */
    storm::solver::GmmxxLinearEquationSolverPreconditioner getPreconditioningMethod() const;

    /*!
     * Retrieves whether the restart iteration count has been set.
     *
     * @return True iff the restart iteration count has been set.
     */
    bool isRestartIterationCountSet() const;

    /*!
     * Retrieves the number of iterations after which restarted methods are to be restarted.
     *
     * @return The number of iterations after which to restart.
     */
    uint_fast64_t getRestartIterationCount() const;

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

    bool check() const override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string techniqueOptionName;
    static const std::string preconditionOptionName;
    static const std::string restartOptionName;
    static const std::string maximalIterationsOptionName;
    static const std::string maximalIterationsOptionShortName;
    static const std::string precisionOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
