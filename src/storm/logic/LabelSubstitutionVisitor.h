#pragma once

#include <map>

#include "storm/logic/CloneVisitor.h"

#include "storm/storage/expressions/Expression.h"

namespace storm {
namespace logic {

class LabelSubstitutionVisitor : public CloneVisitor {
   public:
    LabelSubstitutionVisitor(std::map<std::string, storm::expressions::Expression> const& labelToExpressionMapping);
    LabelSubstitutionVisitor(std::map<std::string, std::string> const& labelToLabelMapping);

    std::shared_ptr<Formula> substitute(Formula const& f) const;

    virtual boost::any visit(AtomicLabelFormula const& f, boost::any const& data) const override;

   private:
    std::map<std::string, storm::expressions::Expression> const* labelToExpressionMapping;
    std::map<std::string, std::string> const* labelToLabelMapping;
};

}  // namespace logic
}  // namespace storm
