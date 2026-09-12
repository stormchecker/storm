#include "storm/logic/TraverseFormulaVisitor.h"

#include <boost/any.hpp>
#include <cstdint>
#include <utility>

#include "storm/logic/Formulas.h"

namespace storm::logic {

TraverseFormulaVisitor::TraverseFormulaVisitor(std::function<bool(Formula const&)> callback) : callback(std::move(callback)) {}

void TraverseFormulaVisitor::traverse(Formula const& f) const {
    f.accept(*this, boost::any());
}

boost::any TraverseFormulaVisitor::visit(AtomicExpressionFormula const& f, boost::any const&) const {
    callback(f);
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(AtomicLabelFormula const& f, boost::any const&) const {
    callback(f);
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(BinaryBooleanStateFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getLeftSubformula().accept(*this, data);
        f.getRightSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(BinaryBooleanPathFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getLeftSubformula().accept(*this, data);
        f.getRightSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(BooleanLiteralFormula const& f, boost::any const&) const {
    callback(f);
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(BoundedUntilFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        if (f.hasMultiDimensionalSubformulas()) {
            for (uint64_t i = 0; i < f.getDimension(); ++i) {
                f.getLeftSubformula(i).accept(*this, data);
                f.getRightSubformula(i).accept(*this, data);
            }
        } else {
            // All dimensions share the same subformulas, which we thus only visit once.
            f.getLeftSubformula().accept(*this, data);
            f.getRightSubformula().accept(*this, data);
        }
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(ConditionalFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
        f.getConditionFormula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(CumulativeRewardFormula const& f, boost::any const&) const {
    callback(f);
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(EventuallyFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(TimeOperatorFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(GloballyFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(GameFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(InstantaneousRewardFormula const& f, boost::any const&) const {
    callback(f);
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(LongRunAverageOperatorFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(LongRunAverageRewardFormula const& f, boost::any const&) const {
    callback(f);
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(MultiObjectiveFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        for (auto const& subformula : f.getSubformulas()) {
            subformula->accept(*this, data);
        }
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(QuantileFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(NextFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(ProbabilityOperatorFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(RewardOperatorFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(TotalRewardFormula const& f, boost::any const&) const {
    callback(f);
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(UnaryBooleanStateFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(UnaryBooleanPathFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(UntilFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        f.getLeftSubformula().accept(*this, data);
        f.getRightSubformula().accept(*this, data);
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(HOAPathFormula const& f, boost::any const& data) const {
    if (callback(f)) {
        for (auto const& mapped : f.getAPMapping()) {
            mapped.second->accept(*this, data);
        }
    }
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(DiscountedCumulativeRewardFormula const& f, boost::any const&) const {
    callback(f);
    return boost::any();
}

boost::any TraverseFormulaVisitor::visit(DiscountedTotalRewardFormula const& f, boost::any const&) const {
    callback(f);
    return boost::any();
}

}  // namespace storm::logic
