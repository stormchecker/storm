#pragma once

#include "storm-config.h"
#include "storm/settings/modules/ModuleSettings.h"

#include "storm/builder/ExplorationOrder.h"
#include "storm/utility/Engine.h"

namespace storm {
namespace solver {
enum class EquationSolverType;
enum class LpSolverType;
enum class MinMaxMethod;
enum class SmtSolverType;
}  // namespace solver

namespace dd {
enum class DdType;
}

namespace settings {
namespace modules {

/*!
 * This class represents the core settings.
 */
class CoreSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of core settings.
     */
    CoreSettings();

    /*!
     * Retrieves the selected equation solver.
     *
     * @return The selected convergence criterion.
     */
    storm::solver::EquationSolverType getEquationSolver() const;

    /*!
     * Retrieves whether a equation solver has been set.
     *
     * @return True iff an equation solver has been set.
     */
    bool isEquationSolverSet() const;

    /*!
     * Retrieves whether the equation solver has been set from its default value.
     *
     * @return True iff it has been set from its default value.
     */
    bool isEquationSolverSetFromDefaultValue() const;

    /*!
     * Retrieves the selected LP solver.
     *
     * @return The selected LP solver.
     */
    storm::solver::LpSolverType getLpSolver() const;

    /*!
     * Retrieves whether the lp solver has been set from its default value.
     *
     * @return True iff it has been set from its default value.
     */
    bool isLpSolverSetFromDefaultValue() const;

    /*!
     * Retrieves the selected SMT solver.
     *
     * @return The selected SMT solver.
     */
    storm::solver::SmtSolverType getSmtSolver() const;

    /*!
     * Retrieves the selected library for DD-related operations.
     *
     * @return The selected library.
     */
    storm::dd::DdType getDdLibraryType() const;

    /*!
     * Retrieves whether the selected DD library is set from its default value.
     *
     * @return True iff if it is set from its default value.
     */
    bool isDdLibraryTypeSetFromDefaultValue() const;

    /*!
     * Retrieves whether statistics are to be shown
     *
     * @return True iff statistics are to be shown
     */
    bool isShowStatisticsSet() const;

    /*!
     * Retrieves the selected engine.
     *
     * @return The selected engine.
     */
    storm::utility::Engine getEngine() const;

    /*!
     * Sets the engine for further usage.
     */
    void setEngine(storm::utility::Engine const& engine);

    bool check() const override;
    void finalize() override;

    // The name of the module.
    static const std::string moduleName;

   private:
    storm::utility::Engine engine;

    // Define the string names of the options as constants.
    static const std::string eqSolverOptionName;
    static const std::string lpSolverOptionName;
    static const std::string smtSolverOptionName;
    static const std::string statisticsOptionName;
    static const std::string statisticsOptionShortName;
    static const std::string engineOptionName;
    static const std::string engineOptionShortName;
    static const std::string ddLibraryOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
