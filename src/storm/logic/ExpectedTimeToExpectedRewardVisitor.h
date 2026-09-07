#pragma once

#include <map>

#include "storm/logic/CloneVisitor.h"
#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace logic {

class ExpectedTimeToExpectedRewardVisitor : public CloneVisitor {
   public:
    ExpectedTimeToExpectedRewardVisitor(std::string const& timeRewardModelName);

    std::shared_ptr<Formula> substitute(Formula const& f) const;

    virtual boost::any visit(TimeOperatorFormula const& f, boost::any const& data) const override;

   private:
    std::string const& timeRewardModelName;
};

}  // namespace logic
}  // namespace storm
