#pragma once

#include "storm-config.h"
#include "storm/settings/modules/ModuleSettings.h"
#include "storm/solver/MultiplicationStyle.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the LRA solver settings.
 */
class LongRunAverageSolverSettings : public ModuleSettings {
   public:
    LongRunAverageSolverSettings();

    /*!
     * Retrieves the selected long run average method for deterministic models.
     */
    storm::solver::LraMethod getDetLraMethod() const;

    /*!
     * Retrieves whether the LraMethod for deterministic models was set from a default value.
     */
    bool isDetLraMethodSetFromDefaultValue() const;

    /*!
     * Retrieves the selected long run average method for nondeterministic models.
     */
    storm::solver::LraMethod getNondetLraMethod() const;

    /*!
     * Retrieves whether the LraMethod for nondeterministic models was set from a default value.
     */
    bool isNondetLraMethodSetFromDefaultValue() const;

    /*!
     * Retrieves whether a maximal iteration count for iterative methods was set.
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
     * Retrieves whether the convergence criterion has been set to relative.
     */
    bool isRelativePrecision() const;

    /*!
     * Retrieves a factor that describes how the system is made aperiodic (if necessary by the method)
     */
    double getAperiodicFactor() const;

    // The name of the module.
    static const std::string moduleName;

   private:
    static const std::string detLraMethodOptionName;
    static const std::string nondetLraMethodOptionName;
    static const std::string maximalIterationsOptionName;
    static const std::string maximalIterationsOptionShortName;
    static const std::string precisionOptionName;
    static const std::string absoluteOptionName;
    static const std::string aperiodicFactorOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
