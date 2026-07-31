#include "AcceptanceCondition.h"

#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/utility/macros.h"

namespace storm {
namespace automata {

AcceptanceCondition::AcceptanceCondition(std::size_t numberOfStates, unsigned int numberOfAcceptanceSets, acceptance_expr::ptr acceptance)
    : numberOfAcceptanceSets(numberOfAcceptanceSets), acceptance(acceptance) {
    // initialize acceptance sets
    for (unsigned int i = 0; i < numberOfAcceptanceSets; i++) {
        acceptanceSets.push_back(storm::storage::BitVector(numberOfStates));
    }
}

unsigned int AcceptanceCondition::getNumberOfAcceptanceSets() const {
    return numberOfAcceptanceSets;
}

storm::storage::BitVector& AcceptanceCondition::getAcceptanceSet(unsigned int index) {
    return acceptanceSets.at(index);
}

const storm::storage::BitVector& AcceptanceCondition::getAcceptanceSet(unsigned int index) const {
    return acceptanceSets.at(index);
}

AcceptanceCondition::acceptance_expr::ptr AcceptanceCondition::getAcceptanceExpression() const {
    return acceptance;
}

bool AcceptanceCondition::isAccepting(const storm::storage::StateBlock& scc) const {
    return isAccepting(scc, acceptance);
}

bool AcceptanceCondition::isAccepting(const storm::storage::StateBlock& scc, acceptance_expr::ptr expr) const {
    switch (expr->getType()) {
        case acceptance_expr::EXP_AND:
            return isAccepting(scc, expr->getLeft()) && isAccepting(scc, expr->getRight());
        case acceptance_expr::EXP_OR:
            return isAccepting(scc, expr->getLeft()) || isAccepting(scc, expr->getRight());
        case acceptance_expr::EXP_NOT:
            return !isAccepting(scc, expr->getLeft());
        case acceptance_expr::EXP_TRUE:
            return true;
        case acceptance_expr::EXP_FALSE:
            return false;
        case acceptance_expr::EXP_ATOM: {
            const cpphoafparser::AtomAcceptance& atom = expr->getAtom();
            const storm::storage::BitVector& acceptanceSet = acceptanceSets.at(atom.getAcceptanceSet());
            bool negated = atom.isNegated();
            bool rv;
            switch (atom.getType()) {
                case cpphoafparser::AtomAcceptance::TEMPORAL_INF:
                    rv = false;
                    for (auto& state : scc) {
                        if (acceptanceSet.get(state)) {
                            rv = true;
                            break;
                        }
                    }
                    break;
                case cpphoafparser::AtomAcceptance::TEMPORAL_FIN:
                    rv = true;
                    for (auto& state : scc) {
                        if (acceptanceSet.get(state)) {
                            rv = false;
                            break;
                        }
                    }
                    break;
            }

            return (negated ? !rv : rv);
        }
    }

    STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Missing case statement.");
}

std::vector<std::vector<AcceptanceCondition::acceptance_expr::ptr>> AcceptanceCondition::extractFromDNF() const {
    std::vector<std::vector<AcceptanceCondition::acceptance_expr::ptr>> dnf;

    extractFromDNFRecursion(getAcceptanceExpression(), dnf, true);

    return dnf;
}

void AcceptanceCondition::extractFromDNFRecursion(AcceptanceCondition::acceptance_expr::ptr e, std::vector<std::vector<acceptance_expr::ptr>>& dnf,
                                                  bool topLevel) const {
    if (topLevel) {
        if (e->isOR()) {
            if (e->getLeft()->isOR()) {
                extractFromDNFRecursion(e->getLeft(), dnf, true);
            } else {
                dnf.emplace_back();
                extractFromDNFRecursion(e->getLeft(), dnf, false);
            }

            if (e->getRight()->isOR()) {
                extractFromDNFRecursion(e->getRight(), dnf, true);
            } else {
                dnf.emplace_back();
                extractFromDNFRecursion(e->getRight(), dnf, false);
            }
        } else {
            dnf.emplace_back();
            extractFromDNFRecursion(e, dnf, false);
        }
    } else {
        if (e->isOR() || e->isNOT()) {
            STORM_LOG_THROW(false, storm::exceptions::InvalidOperationException, "Acceptance condition is not in DNF.");
        } else if (e->isAND()) {
            extractFromDNFRecursion(e->getLeft(), dnf, false);
            extractFromDNFRecursion(e->getRight(), dnf, false);
        } else {
            dnf.back().push_back(e);
        }
    }
}

AcceptanceCondition::ptr AcceptanceCondition::lift(std::size_t productNumberOfStates, std::function<std::size_t(std::size_t)> mapping) const {
    AcceptanceCondition::ptr lifted(new AcceptanceCondition(productNumberOfStates, numberOfAcceptanceSets, acceptance));
    for (unsigned int i = 0; i < numberOfAcceptanceSets; i++) {
        const storm::storage::BitVector& set = getAcceptanceSet(i);
        storm::storage::BitVector& liftedSet = lifted->getAcceptanceSet(i);

        for (std::size_t prodState = 0; prodState < productNumberOfStates; prodState++) {
            if (set.get(mapping(prodState))) {
                liftedSet.set(prodState);
            }
        }
    }

    return lifted;
}

}  // namespace automata
}  // namespace storm
