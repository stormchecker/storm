#include "RewardOperatorFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
RewardOperatorFormula::RewardOperatorFormula(std::shared_ptr<Formula const> const& subformula, boost::optional<std::string> const& rewardModelName,
                                             OperatorInformation const& operatorInformation)
    : OperatorFormula(subformula, operatorInformation), rewardModelName(rewardModelName) {
    // Intentionally left empty.
}

bool RewardOperatorFormula::isRewardOperatorFormula() const {
    return true;
}

boost::any RewardOperatorFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::string const& RewardOperatorFormula::getRewardModelName() const {
    return this->rewardModelName.get();
}

bool RewardOperatorFormula::hasRewardModelName() const {
    return static_cast<bool>(rewardModelName);
}

boost::optional<std::string> const& RewardOperatorFormula::getOptionalRewardModelName() const {
    return this->rewardModelName;
}

void RewardOperatorFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    if (this->hasRewardModelName()) {
        referencedRewardModels.insert(this->getRewardModelName());
    } else {
        referencedRewardModels.insert("");
    }
    this->getSubformula().gatherReferencedRewardModels(referencedRewardModels);
}

std::ostream& RewardOperatorFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parentheses necessary
    out << "R";
    if (this->hasRewardModelName()) {
        out << "{\"" << this->getRewardModelName() << "\"}";
    }
    OperatorFormula::writeToStream(out);
    return out;
}
}  // namespace logic
}  // namespace storm
