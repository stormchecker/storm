#include "Command.h"

namespace storm {
namespace prism {
Command::Command(uint_fast64_t globalIndex, bool markovian, uint_fast64_t actionIndex, std::string const& actionName,
                 storm::expressions::Expression const& guardExpression, std::vector<storm::prism::Update> const& updates, std::string const& filename,
                 uint_fast64_t lineNumber)
    : LocatedInformation(filename, lineNumber),
      actionIndex(actionIndex),
      markovian(markovian),
      actionName(actionName),
      guardExpression(guardExpression),
      updates(updates),
      globalIndex(globalIndex),
      labeled(actionName != "") {
    // Nothing to do here.
}

uint_fast64_t Command::getActionIndex() const {
    return this->actionIndex;
}

bool Command::isMarkovian() const {
    return this->markovian;
}

void Command::setMarkovian(bool value) {
    this->markovian = value;
}

std::string const& Command::getActionName() const {
    return this->actionName;
}

storm::expressions::Expression const& Command::getGuardExpression() const {
    return guardExpression;
}

std::size_t Command::getNumberOfUpdates() const {
    return this->updates.size();
}

storm::prism::Update const& Command::getUpdate(uint_fast64_t index) const {
    STORM_LOG_ASSERT(index < getNumberOfUpdates(), "Invalid index.");
    return this->updates[index];
}

std::vector<storm::prism::Update> const& Command::getUpdates() const {
    return this->updates;
}

std::vector<storm::prism::Update>& Command::getUpdates() {
    return this->updates;
}

uint_fast64_t Command::getGlobalIndex() const {
    return this->globalIndex;
}

Command Command::substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
    std::vector<Update> newUpdates;
    newUpdates.reserve(this->getNumberOfUpdates());
    for (auto const& update : this->getUpdates()) {
        newUpdates.emplace_back(update.substitute(substitution));
    }

    return Command(this->getGlobalIndex(), this->isMarkovian(), this->getActionIndex(), this->getActionName(),
                   this->getGuardExpression().substitute(substitution).simplify(), newUpdates, this->getFilename(), this->getLineNumber());
}

Command Command::substituteNonStandardPredicates() const {
    std::vector<Update> newUpdates;
    newUpdates.reserve(this->getNumberOfUpdates());
    for (auto const& update : this->getUpdates()) {
        newUpdates.emplace_back(update.substituteNonStandardPredicates());
    }

    return Command(this->getGlobalIndex(), this->isMarkovian(), this->getActionIndex(), this->getActionName(),
                   this->getGuardExpression().substituteNonStandardPredicates().simplify(), newUpdates, this->getFilename(), this->getLineNumber());
}

bool Command::isLabeled() const {
    return labeled;
}

bool Command::containsVariablesOnlyInUpdateProbabilities(std::set<storm::expressions::Variable> const& undefinedConstantVariables) const {
    if (this->getGuardExpression().containsVariable(undefinedConstantVariables)) {
        return false;
    }
    for (auto const& update : this->getUpdates()) {
        for (auto const& assignment : update.getAssignments()) {
            if (assignment.getExpression().containsVariable(undefinedConstantVariables)) {
                return false;
            }
        }

        // check likelihood, undefined constants may not occur in 'if' part of IfThenElseExpression
        if (update.getLikelihoodExpression().containsVariableInITEGuard(undefinedConstantVariables)) {
            return false;
        }
    }

    return true;
}

Command Command::simplify() const {
    std::vector<Update> newUpdates;
    newUpdates.reserve(this->getNumberOfUpdates());
    for (auto const& update : this->getUpdates()) {
        newUpdates.emplace_back(update.simplify());
    }
    auto simpleGuard = guardExpression.simplify().reduceNesting();
    return Command(this->globalIndex, this->markovian, this->getActionIndex(), this->getActionName(), simpleGuard, newUpdates, this->getFilename(),
                   this->getLineNumber());
}

Command Command::copyWithNewUpdates(std::vector<Update>&& newUpdates) const {
    return Command(this->globalIndex, this->markovian, this->getActionIndex(), this->getActionName(), guardExpression, newUpdates, this->getFilename(),
                   this->getLineNumber());
}

std::ostream& operator<<(std::ostream& stream, Command const& command) {
    if (command.isMarkovian()) {
        stream << "<" << command.getActionName() << "> ";
    } else {
        stream << "[" << command.getActionName() << "] ";
    }
    stream << command.getGuardExpression() << " -> ";
    for (uint_fast64_t i = 0; i < command.getUpdates().size(); ++i) {
        stream << command.getUpdate(i);
        if (i < command.getUpdates().size() - 1) {
            stream << " + ";
        }
    }
    stream << ";";
    return stream;
}
}  // namespace prism
}  // namespace storm
