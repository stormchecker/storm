#include "storm/logic/HOAPathFormula.h"

#include <boost/any.hpp>
#include <ostream>

#include "storm/automata/DeterministicAutomaton.h"
#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/IllegalArgumentException.h"
#include "storm/exceptions/InvalidPropertyException.h"
#include "storm/io/file.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/utility/macros.h"

namespace storm {
namespace logic {
HOAPathFormula::HOAPathFormula(const std::string& automatonFile, FormulaContext context) : automatonFile(automatonFile), context(context) {
    STORM_LOG_THROW(context == FormulaContext::Probability, storm::exceptions::InvalidPropertyException, "Invalid context for formula.");
}

FormulaContext const& HOAPathFormula::getContext() const {
    return context;
}

const std::string& HOAPathFormula::getAutomatonFile() const {
    return automatonFile;
}

const HOAPathFormula::ap_to_formula_map& HOAPathFormula::getAPMapping() const {
    return apToFormulaMap;
}

void HOAPathFormula::addAPMapping(const std::string& ap, const std::shared_ptr<Formula const>& formula) {
    STORM_LOG_THROW(apToFormulaMap.find(ap) == apToFormulaMap.end(), storm::exceptions::IllegalArgumentException,
                    "HOA path formula: Mapping for atomic proposition \"" + ap + "\" already exists.");
    apToFormulaMap[ap] = formula;
}

bool HOAPathFormula::isProbabilityPathFormula() const {
    return true;
}

bool HOAPathFormula::isHOAPathFormula() const {
    return true;
}

bool HOAPathFormula::hasQuantitativeResult() const {
    return true;
}

bool HOAPathFormula::hasQualitativeResult() const {
    return false;
}

boost::any HOAPathFormula::accept(FormulaVisitor const& visitor, boost::any const& data) const {
    return visitor.visit(*this, data);
}

void HOAPathFormula::gatherAtomicExpressionFormulas(std::vector<std::shared_ptr<AtomicExpressionFormula const>>& atomicExpressionFormulas) const {
    for (auto& mapped : getAPMapping()) {
        mapped.second->gatherAtomicExpressionFormulas(atomicExpressionFormulas);
    }
}

void HOAPathFormula::gatherAtomicLabelFormulas(std::vector<std::shared_ptr<AtomicLabelFormula const>>& atomicLabelFormulas) const {
    for (auto& mapped : getAPMapping()) {
        mapped.second->gatherAtomicLabelFormulas(atomicLabelFormulas);
    }
}

void HOAPathFormula::gatherReferencedRewardModels(std::set<std::string>& referencedRewardModels) const {
    for (auto& mapped : getAPMapping()) {
        mapped.second->gatherReferencedRewardModels(referencedRewardModels);
    }
}

storm::automata::DeterministicAutomaton::ptr HOAPathFormula::readAutomaton() const {
    std::ifstream in;
    storm::io::openFile(automatonFile, in);
    storm::automata::DeterministicAutomaton::ptr automaton = storm::automata::DeterministicAutomaton::parse(in);
    storm::io::closeFile(in);
    for (auto& ap : automaton->getAPSet().getAPs()) {
        STORM_LOG_THROW(apToFormulaMap.find(ap) != apToFormulaMap.end(), storm::exceptions::ExpressionEvaluationException,
                        "For '" << automatonFile << "' HOA automaton, expression for atomic proposition '" << ap << "' is missing.");
    }
    return automaton;
}

std::ostream& HOAPathFormula::writeToStream(std::ostream& out, bool /* allowParentheses */) const {
    // No parentheses necessary
    out << "HOA: { ";
    out << "\"" << automatonFile << "\"";
    for (auto& mapping : apToFormulaMap) {
        out << ", \"" << mapping.first << "\" -> ";
        mapping.second->writeToStream(out);
    }
    out << " }";
    return out;
}
}  // namespace logic
}  // namespace storm
