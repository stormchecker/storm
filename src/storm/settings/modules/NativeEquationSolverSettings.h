#pragma once

#include "storm/settings/modules/ModuleSettings.h"

#include "storm/solver/MultiplicationStyle.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for the native equation solver.
 */
class NativeEquationSolverSettings : public ModuleSettings {
   public:
    // An enumeration of all available convergence criteria.
    enum class ConvergenceCriterion { Absolute, Relative };

    /*!
     * Creates a new set of native equation solver settings.
     */
    NativeEquationSolverSettings();

    /*!
     * Retrieves whether the linear equation system technique has been set.
     *
     * @return True iff the linear equation system technique has been set.
     */
    bool isLinearEquationSystemTechniqueSet() const;

    /*!
     * Retrieves whether the linear equation system technique is set from its default value.
     *
     * @return True iff it was set from its default value.
     */
    bool isLinearEquationSystemTechniqueSetFromDefaultValue() const;

    /*!
     * Retrieves the method that is to be used for solving systems of linear equations.
     *
     * @return The method to use.
     */
    storm::solver::NativeLinearEquationSolverMethod getLinearEquationSystemMethod() const;

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
     * Retrieves the value of omega to be used for the SOR method.
     *
     * @return The value of omega to be used.
     */
    double getOmega() const;

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

    /*!
     * Retrievew whether updates in interval iteration have to be made symmetrically
     */
    bool isForceIntervalIterationSymmetricUpdatesSet() const;

    /*!
     * Retrieves the multiplication style to use in the power method.
     *
     * @return The multiplication style.
     */
    storm::solver::MultiplicationStyle getPowerMethodMultiplicationStyle() const;

    bool check() const override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string techniqueOptionName;
    static const std::string omegaOptionName;
    static const std::string maximalIterationsOptionName;
    static const std::string maximalIterationsOptionShortName;
    static const std::string precisionOptionName;
    static const std::string absoluteOptionName;
    static const std::string intervalIterationSymmetricUpdatesOptionName;
    static const std::string powerMethodMultiplicationStyleOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
