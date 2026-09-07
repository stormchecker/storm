#include "TotalRewardFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/logic/FormulaVisitor.h"

namespace storm {
namespace logic {
TotalRewardFormula::TotalRewardFormula(boost::optional<RewardAccumulation> rewardAccumulation) : rewardAccumulation(rewardAccumulation) {
    // Intentionally left empty.
}

bool TotalRewardFormula::isTotalRewardFormula() const {
    return true;
}

bool TotalRewardFormula::isRewardPathFormula() const {
    return true;
}

bool TotalRewardFormula::hasRewardAccumulation() const {
    return rewardAccumulation.is_initialized();
}

RewardAccumulation const& TotalRewardFormula::getRewardAccumulation() const {
    return rewardAccumulation.get();
}

std::shared_ptr<TotalRewardFormula const> TotalRewardFormula::stripRewardAccumulation() const {
    return std::make_shared<TotalRewardFormula>();
}

boost::any TotalRewardFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

std::ostream& TotalRewardFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parentheses necessary
    out << "C";
    if (hasRewardAccumulation()) {
        out << "[" << getRewardAccumulation() << "]";
    }
    return out;
}
}  // namespace logic
}  // namespace storm
