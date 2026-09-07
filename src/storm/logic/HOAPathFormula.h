#pragma once

#include <map>
#include <memory>
#include <string>

#include "storm/logic/FormulaContext.h"
#include "storm/logic/PathFormula.h"

namespace storm {
namespace automata {
// fwd
class DeterministicAutomaton;
}  // namespace automata

namespace logic {
class HOAPathFormula : public PathFormula {
   public:
    typedef std::map<std::string, std::shared_ptr<Formula const>> ap_to_formula_map;

    HOAPathFormula(const std::string& automatonFile, FormulaContext context = FormulaContext::Probability);

    virtual ~HOAPathFormula() {
        // Intentionally left empty.
    }

    FormulaContext const& getContext() const;
    const std::string& getAutomatonFile() const;
    const ap_to_formula_map& getAPMapping() const;

    void addAPMapping(const std::string& ap, const std::shared_ptr<Formula const>& formula);

    virtual bool isHOAPathFormula() const override;
    virtual bool isProbabilityPathFormula() const override;
    virtual bool hasQuantitativeResult() const override;
    virtual bool hasQualitativeResult() const override;

    virtual boost::any accept(FormulaVisitor const& visitor, boost::any const& data) const override;

    virtual void gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const override;
    virtual void gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const override;
    virtual void gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const override;

    virtual std::ostream& writeToStream(std::ostream& out, bool allowParentheses = false) const override;

    std::shared_ptr<storm::automata::DeterministicAutomaton> readAutomaton() const;

   private:
    std::string automatonFile;
    ap_to_formula_map apToFormulaMap;

    FormulaContext context;
};
}  // namespace logic
}  // namespace storm
