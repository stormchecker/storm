#pragma once

#include <map>
#include <vector>
#include "storm/logic/CloneVisitor.h"

namespace storm {
namespace logic {

class ExtractMaximalStateFormulasVisitor : public CloneVisitor {
   public:
    typedef std::map<std::string, std::shared_ptr<Formula const>> ApToFormulaMap;

    /*!
     * Finds state subformulae in f and replaces them by atomic propositions.
     * @param extractedFormulas will be the mapping of atomic propositions to the subformulae they replace
     * @return the formula with replaced state subformulae
     * @note identical subformulae will be replaced by the same atomic proposition
     */
    static std::shared_ptr<Formula> extract(PathFormula const& f, ApToFormulaMap& extractedFormulas);

    virtual boost::any visit(BinaryBooleanPathFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(BoundedUntilFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(EventuallyFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(GloballyFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(NextFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(UnaryBooleanPathFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(UntilFormula const& f, boost::any const& data) const override;

    virtual boost::any visit(TimeOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(LongRunAverageOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(MultiObjectiveFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(CvarFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(ProbabilityOperatorFormula const& f, boost::any const& data) const override;
    virtual boost::any visit(RewardOperatorFormula const& f, boost::any const& data) const override;

   private:
    ExtractMaximalStateFormulasVisitor(ApToFormulaMap& extractedFormulas);

    std::shared_ptr<Formula> extract(std::shared_ptr<Formula> f) const;
    void incrementNestingLevel() const;
    void decrementNestingLevel() const;

    ApToFormulaMap& extractedFormulas;
    // A mapping from formula-strings to labels in order to use the same label for the equivalent formulas (as strings)
    mutable std::map<std::string, std::string> cachedFormulas;
    std::size_t nestingLevel;
};

}  // namespace logic
}  // namespace storm
