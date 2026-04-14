#include "storm/logic/FragmentChecker.h"

#include <boost/any.hpp>
#include "storm/logic/Formulas.h"

namespace storm {
namespace logic {
class InheritedInformation {
   public:
    InheritedInformation(FragmentSpecification const& fragmentSpecification) : fragmentSpecification(fragmentSpecification) {
        // Intentionally left empty.
    }

    FragmentSpecification const& getSpecification() const {
        return fragmentSpecification;
    }

   private:
    FragmentSpecification const& fragmentSpecification;
};

bool FragmentChecker::conformsToSpecification(Formula const& f, FragmentSpecification const& specification) const {
    bool result = boost::any_cast<bool>(f.accept(*this, InheritedInformation(specification)));

    if (specification.isOperatorAtTopLevelRequired()) {
        result &= f.isOperatorFormula();
    }
    if (specification.isMultiObjectiveFormulaAtTopLevelRequired()) {
        result &= f.isMultiObjectiveFormula();
    }
    if (specification.isQuantileFormulaAtTopLevelRequired()) {
        result &= f.isQuantileFormula();
    }
    if (specification.isCvarFormulaAtTopLevelRequired()) {
        result &= f.isCvarFormula();
    }

    return result;
}

boost::any FragmentChecker::visit(AtomicExpressionFormula const&, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    return inherited.getSpecification().areAtomicExpressionFormulasAllowed();
}

boost::any FragmentChecker::visit(AtomicLabelFormula const&, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    return inherited.getSpecification().areAtomicLabelFormulasAllowed();
}

boost::any FragmentChecker::visit(BinaryBooleanStateFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areBinaryBooleanStateFormulasAllowed();
    result = result && boost::any_cast<bool>(f.getLeftSubformula().accept(*this, data));
    result = result && boost::any_cast<bool>(f.getRightSubformula().accept(*this, data));
    return result;
}

boost::any FragmentChecker::visit(BinaryBooleanPathFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areBinaryBooleanPathFormulasAllowed();
    result = result && boost::any_cast<bool>(f.getLeftSubformula().accept(*this, data));
    result = result && boost::any_cast<bool>(f.getRightSubformula().accept(*this, data));
    return result;
}

boost::any FragmentChecker::visit(BooleanLiteralFormula const&, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    return inherited.getSpecification().areBooleanLiteralFormulasAllowed();
}

boost::any FragmentChecker::visit(BoundedUntilFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areBoundedUntilFormulasAllowed();
    if (f.isMultiDimensional()) {
        result = result && inherited.getSpecification().areMultiDimensionalBoundedUntilFormulasAllowed();
    }

    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        auto tbr = f.getTimeBoundReference(i);
        if (tbr.isStepBound()) {
            result = result && inherited.getSpecification().areStepBoundedUntilFormulasAllowed();
        } else if (tbr.isTimeBound()) {
            result = result && inherited.getSpecification().areTimeBoundedUntilFormulasAllowed();
        } else {
            assert(tbr.isRewardBound());
            result = result && inherited.getSpecification().areRewardBoundedUntilFormulasAllowed();
            if (tbr.hasRewardAccumulation()) {
                result = result && inherited.getSpecification().isRewardAccumulationAllowed();
            }
        }
    }

    if (f.hasMultiDimensionalSubformulas()) {
        for (uint64_t i = 0; i < f.getDimension(); ++i) {
            if (!inherited.getSpecification().areNestedPathFormulasAllowed()) {
                result = result && !f.getLeftSubformula(i).isPathFormula();
                result = result && !f.getRightSubformula(i).isPathFormula();
            }
            result = result && boost::any_cast<bool>(f.getLeftSubformula(i).accept(*this, data));
            result = result && boost::any_cast<bool>(f.getRightSubformula(i).accept(*this, data));
        }
    } else {
        if (!inherited.getSpecification().areNestedPathFormulasAllowed()) {
            result = result && !f.getLeftSubformula().isPathFormula();
            result = result && !f.getRightSubformula().isPathFormula();
        }
        result = result && boost::any_cast<bool>(f.getLeftSubformula().accept(*this, data));
        result = result && boost::any_cast<bool>(f.getRightSubformula().accept(*this, data));
    }
    return result;
}

boost::any FragmentChecker::visit(ConditionalFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = true;
    if (f.isConditionalProbabilityFormula()) {
        result = result && inherited.getSpecification().areConditionalProbabilityFormulasAllowed();
    } else if (f.isConditionalRewardFormula()) {
        result = result && inherited.getSpecification().areConditionalRewardFormulasFormulasAllowed();
    }
    if (inherited.getSpecification().areOnlyEventuallyFormuluasInConditionalFormulasAllowed()) {
        if (f.isConditionalProbabilityFormula()) {
            result = result && f.getSubformula().isReachabilityProbabilityFormula() && f.getConditionFormula().isReachabilityProbabilityFormula();
        } else if (f.isConditionalRewardFormula()) {
            result = result && f.getSubformula().isReachabilityRewardFormula() && f.getConditionFormula().isEventuallyFormula();
        }
    }
    result = result && boost::any_cast<bool>(f.getSubformula().accept(*this, data));
    result = result && boost::any_cast<bool>(f.getConditionFormula().accept(*this, data));
    return result;
}

boost::any FragmentChecker::visit(CumulativeRewardFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);

    bool result = inherited.getSpecification().areCumulativeRewardFormulasAllowed();
    result = result && (!f.isMultiDimensional() || inherited.getSpecification().areMultiDimensionalCumulativeRewardFormulasAllowed());
    result = result && (!f.hasRewardAccumulation() || inherited.getSpecification().isRewardAccumulationAllowed());
    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        auto tbr = f.getTimeBoundReference(i);
        if (tbr.isStepBound()) {
            result = result && inherited.getSpecification().areStepBoundedCumulativeRewardFormulasAllowed();
        } else if (tbr.isTimeBound()) {
            result = result && inherited.getSpecification().areTimeBoundedCumulativeRewardFormulasAllowed();
        } else {
            assert(tbr.isRewardBound());
            result = result && inherited.getSpecification().areRewardBoundedCumulativeRewardFormulasAllowed();
            if (tbr.hasRewardAccumulation()) {
                result = result && inherited.getSpecification().isRewardAccumulationAllowed();
            }
        }
    }
    return result;
}

boost::any FragmentChecker::visit(CvarFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    if (!inherited.getSpecification().areCvarFormulasAllowed()) {
        return false;
    }
    return f.getSubformula().accept(*this, data);
}

boost::any FragmentChecker::visit(EventuallyFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = true;
    if (f.isReachabilityProbabilityFormula()) {
        result = inherited.getSpecification().areReachabilityProbabilityFormulasAllowed();
        if (!inherited.getSpecification().areNestedPathFormulasAllowed()) {
            result = result && !f.getSubformula().isPathFormula();
        }
        result = result && !f.hasRewardAccumulation();
    } else if (f.isReachabilityRewardFormula()) {
        result = result && inherited.getSpecification().areReachabilityRewardFormulasAllowed();
        result = result && f.getSubformula().isStateFormula();
        result = result && (!f.hasRewardAccumulation() || inherited.getSpecification().isRewardAccumulationAllowed());
    } else if (f.isReachabilityTimeFormula()) {
        result = result && inherited.getSpecification().areReachbilityTimeFormulasAllowed();
        result = result && f.getSubformula().isStateFormula();
        result = result && (!f.hasRewardAccumulation() || inherited.getSpecification().isRewardAccumulationAllowed());
    }
    result = result && boost::any_cast<bool>(f.getSubformula().accept(*this, data));
    return result;
}

boost::any FragmentChecker::visit(TimeOperatorFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areTimeOperatorsAllowed();
    result = result && (!f.hasQualitativeResult() || inherited.getSpecification().areQualitativeOperatorResultsAllowed());
    result = result && (!f.hasQuantitativeResult() || inherited.getSpecification().areQuantitativeOperatorResultsAllowed());
    result = result && f.getSubformula().isTimePathFormula();
    if (!inherited.getSpecification().areNestedOperatorsAllowed()) {
        result = result &&
                 boost::any_cast<bool>(f.getSubformula().accept(*this, InheritedInformation(inherited.getSpecification().copy().setOperatorsAllowed(false))));
    } else {
        result = result && boost::any_cast<bool>(f.getSubformula().accept(*this, data));
    }
    return result;
}

boost::any FragmentChecker::visit(GloballyFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areGloballyFormulasAllowed();
    if (!inherited.getSpecification().areNestedPathFormulasAllowed()) {
        result = result && !f.getSubformula().isPathFormula();
    }
    result&& boost::any_cast<bool>(f.getSubformula().accept(*this, data));
    return result;
}

boost::any FragmentChecker::visit(GameFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areGameFormulasAllowed();
    return result && boost::any_cast<bool>(f.getSubformula().accept(*this, data));
}

boost::any FragmentChecker::visit(InstantaneousRewardFormula const&, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    return inherited.getSpecification().areInstantaneousRewardFormulasAllowed();
}

boost::any FragmentChecker::visit(LongRunAverageOperatorFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areLongRunAverageOperatorsAllowed();
    result = result && f.getSubformula().isStateFormula();
    if (!inherited.getSpecification().areNestedOperatorsAllowed()) {
        result = result &&
                 boost::any_cast<bool>(f.getSubformula().accept(*this, InheritedInformation(inherited.getSpecification().copy().setOperatorsAllowed(false))));
    } else {
        result = result && boost::any_cast<bool>(f.getSubformula().accept(*this, data));
    }
    return result;
}

boost::any FragmentChecker::visit(LongRunAverageRewardFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = (!f.hasRewardAccumulation() || inherited.getSpecification().isRewardAccumulationAllowed());
    return result && inherited.getSpecification().areLongRunAverageRewardFormulasAllowed();
}

boost::any FragmentChecker::visit(MultiObjectiveFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);

    FragmentSpecification subFormulaFragment(inherited.getSpecification());
    if (!inherited.getSpecification().areNestedMultiObjectiveFormulasAllowed()) {
        subFormulaFragment.setMultiObjectiveFormulasAllowed(false);
    }
    if (!inherited.getSpecification().areNestedOperatorsInsideMultiObjectiveFormulasAllowed()) {
        subFormulaFragment.setNestedOperatorsAllowed(false);
    }

    bool result = inherited.getSpecification().areMultiObjectiveFormulasAllowed();
    for (auto const& subF : f.getSubformulas()) {
        if (inherited.getSpecification().areOperatorsAtTopLevelOfMultiObjectiveFormulasRequired()) {
            result = result && subF->isOperatorFormula();
        }
        result = result && boost::any_cast<bool>(subF->accept(*this, InheritedInformation(subFormulaFragment)));
    }
    return result;
}

boost::any FragmentChecker::visit(QuantileFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    if (!inherited.getSpecification().areQuantileFormulasAllowed()) {
        return false;
    }
    return f.getSubformula().accept(*this, data);
}

boost::any FragmentChecker::visit(NextFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areNextFormulasAllowed();
    if (!inherited.getSpecification().areNestedPathFormulasAllowed()) {
        result = result && !f.getSubformula().isPathFormula();
    }
    result&& boost::any_cast<bool>(f.getSubformula().accept(*this, data));
    return result;
}

boost::any FragmentChecker::visit(ProbabilityOperatorFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areProbabilityOperatorsAllowed();
    result = result && (!f.hasQualitativeResult() || inherited.getSpecification().areQualitativeOperatorResultsAllowed());
    result = result && (!f.hasQuantitativeResult() || inherited.getSpecification().areQuantitativeOperatorResultsAllowed());
    result = result && (f.getSubformula().isProbabilityPathFormula() || f.getSubformula().isConditionalProbabilityFormula() ||
                        f.getSubformula().isMultiObjectiveFormula());
    if (!inherited.getSpecification().areNestedOperatorsAllowed()) {
        result = result &&
                 boost::any_cast<bool>(f.getSubformula().accept(*this, InheritedInformation(inherited.getSpecification().copy().setOperatorsAllowed(false))));
    } else {
        result = result && boost::any_cast<bool>(f.getSubformula().accept(*this, data));
    }
    return result;
}

boost::any FragmentChecker::visit(RewardOperatorFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areRewardOperatorsAllowed();
    result = result && (!f.hasQualitativeResult() || inherited.getSpecification().areQualitativeOperatorResultsAllowed());
    result = result && (!f.hasQuantitativeResult() || inherited.getSpecification().areQuantitativeOperatorResultsAllowed());
    result = result && (f.getSubformula().isRewardPathFormula() || f.getSubformula().isConditionalRewardFormula());

    if (!inherited.getSpecification().areNestedOperatorsAllowed()) {
        result = result &&
                 boost::any_cast<bool>(f.getSubformula().accept(*this, InheritedInformation(inherited.getSpecification().copy().setOperatorsAllowed(false))));
    } else {
        result = result && boost::any_cast<bool>(f.getSubformula().accept(*this, data));
    }
    return result;
}

boost::any FragmentChecker::visit(TotalRewardFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = (!f.hasRewardAccumulation() || inherited.getSpecification().isRewardAccumulationAllowed());
    result = result && inherited.getSpecification().areTotalRewardFormulasAllowed();
    return result;
}

boost::any FragmentChecker::visit(UnaryBooleanStateFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areUnaryBooleanStateFormulasAllowed();
    result = result && boost::any_cast<bool>(f.getSubformula().accept(*this, data));
    return result;
}

boost::any FragmentChecker::visit(UnaryBooleanPathFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areUnaryBooleanPathFormulasAllowed();
    result = result && boost::any_cast<bool>(f.getSubformula().accept(*this, data));
    return result;
}

boost::any FragmentChecker::visit(UntilFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areUntilFormulasAllowed();
    if (!inherited.getSpecification().areNestedPathFormulasAllowed()) {
        result = result && !f.getLeftSubformula().isPathFormula();
        result = result && !f.getRightSubformula().isPathFormula();
    }
    result = result && boost::any_cast<bool>(f.getLeftSubformula().accept(*this, data));
    result = result && boost::any_cast<bool>(f.getRightSubformula().accept(*this, data));
    return result;
}

boost::any FragmentChecker::visit(HOAPathFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = inherited.getSpecification().areHOAPathFormulasAllowed();
    for (auto& mapped : f.getAPMapping()) {
        result = result && boost::any_cast<bool>(mapped.second->accept(*this, data));
    }
    return result;
}

boost::any FragmentChecker::visit(DiscountedCumulativeRewardFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);

    bool result = inherited.getSpecification().areDiscountedCumulativeRewardFormulasAllowed();
    result = result && (!f.isMultiDimensional() || inherited.getSpecification().areMultiDimensionalCumulativeRewardFormulasAllowed());
    result = result && (!f.hasRewardAccumulation() || inherited.getSpecification().isRewardAccumulationAllowed());
    for (uint64_t i = 0; i < f.getDimension(); ++i) {
        auto tbr = f.getTimeBoundReference(i);
        if (tbr.isStepBound()) {
            result = result && inherited.getSpecification().areStepBoundedCumulativeRewardFormulasAllowed();
        } else if (tbr.isTimeBound()) {
            result = result && inherited.getSpecification().areTimeBoundedCumulativeRewardFormulasAllowed();
        } else {
            assert(tbr.isRewardBound());
            result = result && inherited.getSpecification().areRewardBoundedCumulativeRewardFormulasAllowed();
            if (tbr.hasRewardAccumulation()) {
                result = result && inherited.getSpecification().isRewardAccumulationAllowed();
            }
        }
    }
    return result;
}

boost::any FragmentChecker::visit(DiscountedTotalRewardFormula const& f, boost::any const& data) const {
    InheritedInformation const& inherited = boost::any_cast<InheritedInformation const&>(data);
    bool result = (!f.hasRewardAccumulation() || inherited.getSpecification().isRewardAccumulationAllowed());
    result = result && inherited.getSpecification().areDiscountedTotalRewardFormulasAllowed();
    return result;
}
}  // namespace logic
}  // namespace storm
