#pragma once

#include "storm/settings/modules/ModuleSettings.h"

#include "storm/solver/SolverSelectionOptions.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for the native equation solver.
 */
class TopologicalEquationSolverSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of native equation solver settings.
     */
    TopologicalEquationSolverSettings();

    /*!
     * Retrieves whether the underlying equation solver type has been set.
     *
     * @return True iff the linear equation system technique has been set.
     */
    bool isUnderlyingEquationSolverTypeSet() const;

    /*!
     * Retrieves whether the underlying equation solver type is set from its default value.
     *
     * @return True iff it was set from its default value.
     */
    bool isUnderlyingEquationSolverTypeSetFromDefaultValue() const;

    /*!
     * Retrieves the method that is to be used for solving systems of linear equations.
     *
     * @return The method to use.
     */
    storm::solver::EquationSolverType getUnderlyingEquationSolverType() const;

    /*!
     * Retrieves whether the underlying equation solver type has been set.
     *
     * @return True iff the linear equation system technique has been set.
     */
    bool isUnderlyingMinMaxMethodSet() const;

    /*!
     * Retrieves whether the underlying minmax method is set from its default value.
     *
     * @return True iff it was set from its default value.
     */
    bool isUnderlyingMinMaxMethodSetFromDefaultValue() const;

    /*!
     * Retrieves the method that is to be used for solving systems of linear equations.
     *
     * @return The method to use.
     */
    storm::solver::MinMaxMethod getUnderlyingMinMaxMethod() const;

    /*!
     * If true, the relevant states of each SCC are computed and passed to the underlying equation solver.
     */
    bool isExtendRelevantValues() const;

    bool check() const override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string underlyingEquationSolverOptionName;
    static const std::string underlyingMinMaxMethodOptionName;
    static const std::string extendRelevantValuesOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
