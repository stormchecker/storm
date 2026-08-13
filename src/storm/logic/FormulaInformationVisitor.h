#pragma once

#include "storm/logic/FormulaInformation.h"
#include "storm/logic/FormulaVisitor.h"

namespace storm {
namespace logic {

class FormulaInformationVisitor : public FormulaVisitor {
   public:
    static FormulaInformation getInformation(Formula const& f, bool recurseIntoOperators = true);

    explicit FormulaInformationVisitor(bool recurseIntoOperators);
    FormulaInformationVisitor(FormulaInformationVisitor const& other) = default;
    FormulaInformationVisitor(FormulaInformationVisitor&& other) = default;
    FormulaInformationVisitor& operator=(FormulaInformationVisitor const& other) = default;
    FormulaInformationVisitor& operator=(FormulaInformationVisitor&& other) = default;

    virtual boost::any visit(AtomicExpressionFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(AtomicLabelFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(BinaryBooleanStateFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(BinaryBooleanPathFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(BooleanLiteralFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(BoundedUntilFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(ConditionalFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(CumulativeRewardFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(CvarFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(EventuallyFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(TimeOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(GloballyFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(GameFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(InstantaneousRewardFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(LongRunAverageOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(LongRunAverageRewardFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(MultiObjectiveFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(QuantileFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(NextFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(ProbabilityOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(RewardOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(TotalRewardFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(UnaryBooleanStateFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(UnaryBooleanPathFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(UntilFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(HOAPathFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(DiscountedCumulativeRewardFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(DiscountedTotalRewardFormula const& f, boost::any const& data) const override;

   private:
    bool recurseIntoOperators;
};

}  // namespace logic
}  // namespace storm
