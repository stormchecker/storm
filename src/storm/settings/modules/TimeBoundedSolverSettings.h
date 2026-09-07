#pragma once

#include "storm-config.h"
#include "storm/settings/modules/ModuleSettings.h"
#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the min/max solver settings.
 */
class TimeBoundedSolverSettings : public ModuleSettings {
   public:
    TimeBoundedSolverSettings();

    /*!
     * Retrieves whether solving technique for time bounded reachability on Markov Automata has been set from its default value.
     */
    bool isMaMethodSetFromDefaultValue() const;

    /*!
     * Retrieves the selected solving technique for time bounded reachability on Markov Automata.
     */
    storm::solver::MaBoundedReachabilityMethod getMaMethod() const;

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
     * Retrieves whether the convergence criterion has been set to relative or absolute.
     */
    bool isRelativePrecision() const;

    /*!
     * Retrieves the truncation factor used for unifPlus
     */
    double getUnifPlusKappa() const;

    // The name of the module.
    static const std::string moduleName;

   private:
    static const std::string maMethodOptionName;
    static const std::string precisionOptionName;
    static const std::string absoluteOptionName;
    static const std::string unifPlusKappaOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
