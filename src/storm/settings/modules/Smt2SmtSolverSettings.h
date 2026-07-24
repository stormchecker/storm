#pragma once

#include "storm/settings/modules/ModuleSettings.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for interaction with SMT-LIBv2 solvers.
 */
class Smt2SmtSolverSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of SmtlibSmtSolver settings that is managed by the given manager.
     *
     * @param settingsManager The responsible manager.
     */
    Smt2SmtSolverSettings();

    /*!
     * Retrieves whether the solver command has been set.
     *
     * @return True iff the solver command has been set.
     */
    bool isSolverCommandSet() const;

    /*!
     * Retrieves the solver command i.e. the path and the command line arguments for the solver.
     *
     * @return The solver command to be used
     */
    std::string getSolverCommand() const;

    /*!
     * Retrieves whether the SMT-LIBv2 script should be exportet to a file.
     *
     * @return True iff the SMT-LIBv2 script should be exportet to a file.
     */
    bool isExportSmtLibScriptSet() const;

    /*!
     * Retrieves the path where the SMT-LIBv2 script file should be exportet to.
     *
     * @return the path where the SMT-LIBv2 script file should be exportet to.
     */
    std::string getExportSmtLibScriptPath() const;

    bool check() const override;

    // The name of the module.
    static const std::string moduleName;

   private:
    // Define the string names of the options as constants.
    static const std::string solverCommandOption;
    static const std::string exportScriptOption;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
