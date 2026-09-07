#include "LabelSubstitutionVisitor.h"

#include <boost/any.hpp>

#include "storm/logic/Formulas.h"

namespace storm {
namespace logic {

LabelSubstitutionVisitor::LabelSubstitutionVisitor(std::map<std::string, storm::expressions::Expression> const& labelToExpressionMapping)
    : labelToExpressionMapping(&labelToExpressionMapping), labelToLabelMapping(nullptr) {
    // Intentionally left empty.
}

LabelSubstitutionVisitor::LabelSubstitutionVisitor(std::map<std::string, std::string> const& labelToLabelMapping)
    : labelToExpressionMapping(nullptr), labelToLabelMapping(&labelToLabelMapping) {
    // Intentionally left empty.
}

std::shared_ptr<Formula> LabelSubstitutionVisitor::substitute(Formula const& f) const {
    boost::any result = f.accept(*this, boost::any());
    return boost::any_cast<std::shared_ptr<Formula>>(result);
}

boost::any LabelSubstitutionVisitor::visit(AtomicLabelFormula const& f, boost::any const&) const {
    if (labelToExpressionMapping) {
        auto it = labelToExpressionMapping->find(f.getLabel());
        if (it != labelToExpressionMapping->end()) {
            return std::static_pointer_cast<Formula>(std::make_shared<AtomicExpressionFormula>(it->second));
        } else {
            return std::static_pointer_cast<Formula>(std::make_shared<AtomicLabelFormula>(f.getLabel()));
        }
    } else {
        auto it = labelToLabelMapping->find(f.getLabel());
        if (it != labelToLabelMapping->end()) {
            return std::static_pointer_cast<Formula>(std::make_shared<AtomicLabelFormula>(it->second));
        } else {
            return std::static_pointer_cast<Formula>(std::make_shared<AtomicLabelFormula>(f.getLabel()));
        }
    }
}
}  // namespace logic
}  // namespace storm
