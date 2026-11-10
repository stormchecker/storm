#include "Property.h"

namespace storm {
namespace jani {

std::ostream& operator<<(std::ostream& os, FilterExpression const& fe) {
    return os << "Obtain " << toString(fe.getFilterType()) << " of the '" << *fe.getStatesFormula() << "'-states with values described by '" << *fe.getFormula()
              << "'";
}

Property::Property(std::string const& name, std::shared_ptr<storm::logic::Formula const> const& formula,
                   std::set<storm::expressions::Variable> const& undefinedConstants, std::string const& comment)
    : name(name), comment(comment), filterExpression(FilterExpression(formula)), undefinedConstants(undefinedConstants) {
    // Intentionally left empty.
}

Property::Property(std::string const& name, FilterExpression const& fe, std::set<storm::expressions::Variable> const& undefinedConstants,
                   std::string const& comment)
    : name(name), comment(comment), filterExpression(fe), undefinedConstants(undefinedConstants) {
    // Intentionally left empty.
}

std::string const& Property::getName() const {
    return this->name;
}

std::string const& Property::getComment() const {
    return this->comment;
}

std::string Property::asPrismSyntax() const {
    std::stringstream stream;
    if (!this->getName().empty()) {
        stream << "\"" << this->getName() << "\": ";
    }
    auto fe = this->getFilter();
    if (fe.isDefault()) {
        stream << *fe.getFormula();
    } else {
        stream << "filter(" << storm::modelchecker::toString(fe.getFilterType()) << ", " << *fe.getFormula();
        if (fe.getStatesFormula() && !fe.getStatesFormula()->isInitialFormula()) {
            stream << ", " << *fe.getFormula();
        }
        stream << ")";
    }
    stream << ";";

    if (!this->getComment().empty()) {
        stream << " // " << this->getComment();
    }
    return stream.str();
}

Property Property::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    std::set<storm::expressions::Variable> remainingUndefinedConstants;
    for (auto const& constant : undefinedConstants) {
        if (substitution.find(constant) == substitution.end()) {
            remainingUndefinedConstants.insert(constant);
        }
    }
    return Property(name, filterExpression.substitute(substitution), remainingUndefinedConstants, comment);
}

Property Property::substitute(std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const& substitutionFunction) const {
    std::set<storm::expressions::Variable> remainingUndefinedConstants;
    for (auto const& constant : undefinedConstants) {
        substitutionFunction(constant.getExpression()).getBaseExpression().gatherVariables(remainingUndefinedConstants);
    }
    return Property(name, filterExpression.substitute(substitutionFunction), remainingUndefinedConstants, comment);
}

Property Property::substituteLabels(std::map<std::string, std::string> const& substitution) const {
    return Property(name, filterExpression.substituteLabels(substitution), undefinedConstants, comment);
}

Property Property::substituteRewardModelNames(std::map<std::string, std::string> const& rewardModelNameSubstitution) const {
    return Property(name, filterExpression.substituteRewardModelNames(rewardModelNameSubstitution), undefinedConstants, comment);
}

Property Property::substituteTranscendentalNumbers() const {
    return Property(name, filterExpression.substituteTranscendentalNumbers(), undefinedConstants, comment);
}

Property Property::clone() const {
    return Property(name, filterExpression.clone(), undefinedConstants, comment);
}

FilterExpression const& Property::getFilter() const {
    return this->filterExpression;
}

std::shared_ptr<storm::logic::Formula const> Property::getRawFormula() const {
    return this->filterExpression.getFormula();
}

std::set<storm::expressions::Variable> const& Property::getUndefinedConstants() const {
    return undefinedConstants;
}

bool Property::containsUndefinedConstants() const {
    return !undefinedConstants.empty();
}

std::set<storm::expressions::Variable> Property::getUsedVariablesAndConstants() const {
    auto res = getUndefinedConstants();
    getFilter().getFormula()->gatherUsedVariables(res);
    getFilter().getStatesFormula()->gatherUsedVariables(res);
    return res;
}

std::set<std::string> Property::getUsedLabels() const {
    std::set<std::string> res;
    auto labFormSet = getFilter().getFormula()->getAtomicLabelFormulas();
    for (auto const& f : labFormSet) {
        res.insert(f->getLabel());
    }
    labFormSet = getFilter().getStatesFormula()->getAtomicLabelFormulas();
    for (auto const& f : labFormSet) {
        res.insert(f->getLabel());
    }
    return res;
}

void Property::gatherReferencedRewardModels(std::set<std::string>& rewardModelNames) const {
    getFilter().getFormula()->gatherReferencedRewardModels(rewardModelNames);
    getFilter().getStatesFormula()->gatherReferencedRewardModels(rewardModelNames);
}

std::ostream& operator<<(std::ostream& os, Property const& p) {
    return os << "(" << p.getName() << "): " << p.getFilter();
}

}  // namespace jani
}  // namespace storm
