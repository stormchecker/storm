#include "RefinementCommand.h"

namespace storm::gbar {
namespace abstraction {

RefinementCommand::RefinementCommand(uint64_t referencedPlayer1Choice, std::vector<storm::expressions::Expression> const& predicates)
    : referencedPlayer1Choice(referencedPlayer1Choice), predicates(predicates) {
    // Intentionally left empty.
}

RefinementCommand::RefinementCommand(std::vector<storm::expressions::Expression> const& predicates) : predicates(predicates) {
    // Intentionally left empty.
}

bool RefinementCommand::refersToPlayer1Choice() const {
    return static_cast<bool>(referencedPlayer1Choice);
}

uint64_t RefinementCommand::getReferencedPlayer1Choice() const {
    return referencedPlayer1Choice.get();
}

std::vector<storm::expressions::Expression> const& RefinementCommand::getPredicates() const {
    return predicates;
}

}  // namespace abstraction
}  // namespace storm::gbar
