#pragma once

#include "storm/settings/modules/ModuleSettings.h"

#include "storm/solver/stateelimination/EliminationMethod.h"
#include "storm/solver/stateelimination/EliminationOrder.h"

namespace storm {
namespace settings {
namespace modules {

/*!
 * This class represents the settings for the elimination-based procedures.
 */
class EliminationSettings : public ModuleSettings {
   public:
    /*!
     * Creates a new set of parametric model checking settings.
     */
    EliminationSettings();

    /*!
     * Retrieves the selected elimination method.
     *
     * @return The selected elimination method.
     */
    storm::solver::stateelimination::EliminationMethod getEliminationMethod() const;

    /*!
     * Retrieves the selected elimination order.
     *
     * @return The selected elimination order.
     */
    storm::solver::stateelimination::EliminationOrder getEliminationOrder() const;

    /*!
     * Retrieves whether the option to eliminate entry states in the very end is set.
     *
     * @return True iff the option is set.
     */
    bool isEliminateEntryStatesLastSet() const;

    /*!
     * Retrieves the maximal size of an SCC on which state elimination is to be directly applied.
     *
     * @return The maximal size of an SCC on which state elimination is to be directly applied.
     */
    uint_fast64_t getMaximalSccSize() const;

    /*!
     * Retrieves whether the dedicated model checker is to be used instead of the general on.
     *
     * @return True iff the option was set.
     */
    bool isUseDedicatedModelCheckerSet() const;

    const static std::string moduleName;

   private:
    const static std::string eliminationMethodOptionName;
    const static std::string eliminationOrderOptionName;
    const static std::string entryStatesLastOptionName;
    const static std::string maximalSccSizeOptionName;
    const static std::string useDedicatedModelCheckerOptionName;
};

}  // namespace modules
}  // namespace settings
}  // namespace storm
