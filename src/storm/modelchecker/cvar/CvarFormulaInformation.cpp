#include "storm/modelchecker/cvar/CvarFormulaInformation.h"

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/EventuallyFormula.h"
#include "storm/logic/RewardOperatorFormula.h"
#include "storm/utility/logging.h"
#include "storm/utility/macros.h"

namespace storm {
namespace modelchecker {
namespace cvar {
CvarFormulaInformation extractCvarFormulaInformation(storm::logic::CvarFormula const& formula) {
    storm::logic::Formula const& embeddedFormula = formula.getSubformula();
    STORM_LOG_THROW(embeddedFormula.isRewardOperatorFormula(), storm::exceptions::InvalidPropertyException,
                    "CVaR formulas currently require an embedded reward operator formula.");

    auto const& rewardOperator = embeddedFormula.asRewardOperatorFormula();
    STORM_LOG_THROW(rewardOperator.hasOptimalityType(), storm::exceptions::InvalidPropertyException,
                    "The embedded reward operator formula of a CVaR query must specify whether to minimize or maximize.");
    STORM_LOG_THROW(!rewardOperator.hasBound(), storm::exceptions::InvalidPropertyException,
                    "The embedded reward operator formula of a CVaR query must not specify a threshold.");

    storm::logic::Formula const& rewardPathFormula = rewardOperator.getSubformula();
    STORM_LOG_THROW(rewardPathFormula.isReachabilityRewardFormula(), storm::exceptions::InvalidPropertyException,
                    "CVaR queries currently only support weighted reachability objectives.");
    STORM_LOG_THROW(rewardPathFormula.isEventuallyFormula(), storm::exceptions::InvalidPropertyException,
                    "CVaR queries currently only support reachability reward formulas of the form Rmin/max=? [ F target ].");

    auto const& eventuallyFormula = rewardPathFormula.asEventuallyFormula();
    STORM_LOG_THROW(eventuallyFormula.getSubformula().isStateFormula(), storm::exceptions::InvalidPropertyException,
                    "The target of the embedded reachability reward formula of a CVaR query must be a state formula.");

    return {formula.getAlpha(), rewardOperator.getOptimalityType(), rewardOperator.getOptionalRewardModelName(),
            eventuallyFormula.getSubformula().asSharedPointer()};
}
}  // namespace cvar
}  // namespace modelchecker
}  // namespace storm
