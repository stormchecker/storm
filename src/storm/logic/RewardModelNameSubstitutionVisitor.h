#pragma once

#include <map>

#include "storm/logic/CloneVisitor.h"

#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace logic {

class RewardModelNameSubstitutionVisitor : public CloneVisitor {
   public:
    RewardModelNameSubstitutionVisitor(std::map<std::string, std::string> const& rewardModelNameMapping);

    std::shared_ptr<Formula> substitute(Formula const& f) const;

    virtual boost::any visit(BoundedUntilFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(CumulativeRewardFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(RewardOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(DiscountedCumulativeRewardFormula const& f, boost::any const& data) const override;

   private:
    std::string const& getNewName(std::string const& oldName) const;

    std::map<std::string, std::string> const& rewardModelNameMapping;
};

}  // namespace logic
}  // namespace storm
