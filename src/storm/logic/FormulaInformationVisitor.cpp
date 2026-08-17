#include "storm/logic/FormulaInformationVisitor.h"
#include <boost/any.hpp>

#include "storm/logic/Formulas.h"

namespace storm {
namespace logic {
FormulaInformation FormulaInformationVisitor::getInformation(Formula const& f, bool recurseIntoOperators) {
    FormulaInformationVisitor visitor(recurseIntoOperators);
    boost::any result = f.accept(visitor, boost::any());
    return boost::any_cast<FormulaInformation>(result);
}

FormulaInformationVisitor::FormulaInformationVisitor(bool recurseIntoOperators) : recurseIntoOperators(recurseIntoOperators) {}

boost::any FormulaInformationVisitor::visit(AtomicExpressionFormula const&, boost::any const&) const {
    return FormulaInformation();
}

boost::any FormulaInformationVisitor::visit(AtomicLabelFormula const&, boost::any const&) const {
    return FormulaInformation();
}

boost::any FormulaInformationVisitor::visit(BinaryBooleanStateFormula const& f, boost::any const& data) const {
    return boost::any_cast<FormulaInformation>(f.getLeftSubformula().accept(*this, data))
        .join(boost::any_cast<FormulaInformation>(f.getRightSubformula().accept(*this)));
}

boost::any FormulaInformationVisitor::visit(BinaryBooleanPathFormula const& f, boost::any const& data) const {
    FormulaInformation result = boost::any_cast<FormulaInformation>(f.getLeftSubformula().accept(*this, data))
                                    .join(boost::any_cast<FormulaInformation>(f.getRightSubformula().accept(*this)));
    return result.setContainsComplexPathFormula();
}

boost::any FormulaInformationVisitor::visit(BooleanLiteralFormula const&, boost::any const&) const {
    return FormulaInformation();
}

boost::any FormulaInformationVisitor::visit(BoundedUntilFormula const& f, boost::any const& data) const {
    FormulaInformation result;
    result.setContainsBoundedUntilFormula(true);
    for (unsigned i = 0; i < f.getDimension(); ++i) {
        if (f.getTimeBoundReference(i).isRewardBound()) {
            result.setContainsRewardBoundedFormula(true);
        }
    }

    if (f.hasMultiDimensionalSubformulas()) {
        for (unsigned i = 0; i < f.getDimension(); ++i) {
            result.join(boost::any_cast<FormulaInformation>(f.getLeftSubformula(i).accept(*this, data)));
            result.join(boost::any_cast<FormulaInformation>(f.getRightSubformula(i).accept(*this, data)));
            if (f.getLeftSubformula(i).isPathFormula() || f.getRightSubformula(i).isPathFormula()) {
                result.setContainsComplexPathFormula();
            }
        }
    } else {
        result.join(boost::any_cast<FormulaInformation>(f.getLeftSubformula().accept(*this, data)));
        result.join(boost::any_cast<FormulaInformation>(f.getRightSubformula().accept(*this, data)));
        if (f.getLeftSubformula().isPathFormula() || f.getRightSubformula().isPathFormula()) {
            result.setContainsComplexPathFormula();
        }
    }
    return result;
}

boost::any FormulaInformationVisitor::visit(ConditionalFormula const& f, boost::any const& data) const {
    return boost::any_cast<FormulaInformation>(f.getSubformula().accept(*this, data))
        .join(boost::any_cast<FormulaInformation>(f.getConditionFormula().accept(*this)));
}

boost::any FormulaInformationVisitor::visit(CumulativeRewardFormula const& f, boost::any const&) const {
    FormulaInformation result;
    result.setContainsCumulativeRewardFormula(true);
    for (unsigned i = 0; i < f.getDimension(); ++i) {
        if (f.getTimeBoundReference(i).isRewardBound()) {
            result.setContainsRewardBoundedFormula(true);
        }
    }
    return result;
}

boost::any FormulaInformationVisitor::visit(EventuallyFormula const& f, boost::any const& data) const {
    FormulaInformation result = boost::any_cast<FormulaInformation>(f.getSubformula().accept(*this, data));
    if (f.getSubformula().isPathFormula()) {
        result.setContainsComplexPathFormula();
    }
    return result;
}

boost::any FormulaInformationVisitor::visit(TimeOperatorFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any FormulaInformationVisitor::visit(GloballyFormula const& f, boost::any const& data) const {
    FormulaInformation result = boost::any_cast<FormulaInformation>(f.getSubformula().accept(*this, data));
    if (f.getSubformula().isPathFormula()) {
        result.setContainsComplexPathFormula();
    }
    return result;
}

boost::any FormulaInformationVisitor::visit(GameFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any FormulaInformationVisitor::visit(InstantaneousRewardFormula const&, boost::any const&) const {
    return FormulaInformation();
}

boost::any FormulaInformationVisitor::visit(LongRunAverageOperatorFormula const& f, boost::any const& data) const {
    FormulaInformation result;
    result.setContainsLongRunFormula(true);
    if (recurseIntoOperators) {
        result.join(boost::any_cast<FormulaInformation>(f.getSubformula().accept(*this, data)));
    }
    return result;
}

boost::any FormulaInformationVisitor::visit(LongRunAverageRewardFormula const&, boost::any const&) const {
    FormulaInformation result;
    result.setContainsLongRunFormula(true);
    return result;
}

boost::any FormulaInformationVisitor::visit(MultiObjectiveFormula const& f, boost::any const& data) const {
    FormulaInformation result;
    if (recurseIntoOperators) {
        for (auto const& subF : f.getSubformulas()) {
            result.join(boost::any_cast<FormulaInformation>(subF->accept(*this, data)));
        }
    }
    return result;
}

boost::any FormulaInformationVisitor::visit(QuantileFormula const& f, boost::any const& data) const {
    if (recurseIntoOperators) {
        return f.getSubformula().accept(*this, data);
    } else {
        return FormulaInformation();
    }
}

boost::any FormulaInformationVisitor::visit(NextFormula const& f, boost::any const& data) const {
    FormulaInformation result = boost::any_cast<FormulaInformation>(f.getSubformula().accept(*this, data)).setContainsNextFormula();
    if (f.getSubformula().isPathFormula()) {
        result.setContainsComplexPathFormula();
    }
    return result;
}

boost::any FormulaInformationVisitor::visit(ProbabilityOperatorFormula const& f, boost::any const& data) const {
    if (recurseIntoOperators) {
        return f.getSubformula().accept(*this, data);
    } else {
        return FormulaInformation();
    }
}

boost::any FormulaInformationVisitor::visit(RewardOperatorFormula const& f, boost::any const& data) const {
    if (recurseIntoOperators) {
        return boost::any_cast<FormulaInformation>(f.getSubformula().accept(*this, data)).setContainsRewardOperator();
    } else {
        return FormulaInformation();
    }
}

boost::any FormulaInformationVisitor::visit(TotalRewardFormula const&, boost::any const&) const {
    return FormulaInformation();
}

boost::any FormulaInformationVisitor::visit(UnaryBooleanStateFormula const& f, boost::any const& data) const {
    return f.getSubformula().accept(*this, data);
}

boost::any FormulaInformationVisitor::visit(UnaryBooleanPathFormula const& f, boost::any const& data) const {
    FormulaInformation result = boost::any_cast<FormulaInformation>(f.getSubformula().accept(*this, data));
    result.setContainsComplexPathFormula();
    return result;
}

boost::any FormulaInformationVisitor::visit(UntilFormula const& f, boost::any const& data) const {
    FormulaInformation result = boost::any_cast<FormulaInformation>(f.getLeftSubformula().accept(*this, data))
                                    .join(boost::any_cast<FormulaInformation>(f.getRightSubformula().accept(*this)));
    if (f.getLeftSubformula().isPathFormula() || f.getRightSubformula().isPathFormula()) {
        result.setContainsComplexPathFormula();
    }
    return result;
}

boost::any FormulaInformationVisitor::visit(HOAPathFormula const& f, boost::any const& data) const {
    FormulaInformation info;
    if (recurseIntoOperators) {
        for (auto& mapped : f.getAPMapping()) {
            info = info.join(boost::any_cast<FormulaInformation>(mapped.second->accept(*this, data)));
        }
    }
    return info;
}

boost::any FormulaInformationVisitor::visit(DiscountedCumulativeRewardFormula const& f, boost::any const&) const {
    FormulaInformation result;
    result.setContainsCumulativeRewardFormula(true);
    result.setContainsDiscountFormula(true);
    for (unsigned i = 0; i < f.getDimension(); ++i) {
        if (f.getTimeBoundReference(i).isRewardBound()) {
            result.setContainsRewardBoundedFormula(true);
        }
    }
    return result;
}

boost::any FormulaInformationVisitor::visit(DiscountedTotalRewardFormula const&, boost::any const&) const {
    FormulaInformation result;
    result.setContainsDiscountFormula(true);
    return result;
}

}  // namespace logic
}  // namespace storm
