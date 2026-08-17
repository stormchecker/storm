#include "storm-gamebased-ar/abstraction/StateSetAbstractor.h"

#include "storm-gamebased-ar/abstraction/AbstractionInformation.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/dd/DdManager.h"
#include "storm/utility/macros.h"
#include "storm/utility/solver.h"

namespace storm::gbar {
namespace abstraction {

template<storm::dd::DdType DdType, typename ValueType>
StateSetAbstractor<DdType, ValueType>::StateSetAbstractor(AbstractionInformation<DdType>& abstractionInformation,
                                                          std::vector<storm::expressions::Expression> const& statePredicates,
                                                          std::shared_ptr<storm::utility::solver::SmtSolverFactory> const& smtSolverFactory)
    : smtSolver(smtSolverFactory->create(abstractionInformation.getExpressionManager())),
      abstractionInformation(abstractionInformation),
      localExpressionInformation(abstractionInformation),
      relevantPredicatesAndVariables(),
      concretePredicateVariables(),
      forceRecomputation(true),
      cachedBdd(abstractionInformation.getDdManager().getBddOne()),
      constraint(abstractionInformation.getDdManager().getBddOne()) {
    for (auto const& constraint : abstractionInformation.getConstraints()) {
        smtSolver->add(constraint);
    }

    // Assert all state predicates.
    for (auto const& predicate : statePredicates) {
        smtSolver->add(predicate);

        // Extract the variables of the predicate, so we know which variables were used when abstracting.
        std::set<storm::expressions::Variable> usedVariables = predicate.getVariables();
        concretePredicateVariables.insert(usedVariables.begin(), usedVariables.end());
    }
    localExpressionInformation.relate(concretePredicateVariables);
}

template<storm::dd::DdType DdType, typename ValueType>
void StateSetAbstractor<DdType, ValueType>::addMissingPredicates(std::set<uint_fast64_t> const& newRelevantPredicateIndices) {
    std::vector<std::pair<storm::expressions::Variable, uint_fast64_t>> newPredicateVariables =
        this->getAbstractionInformation().declareNewVariables(relevantPredicatesAndVariables, newRelevantPredicateIndices);

    for (auto const& element : newPredicateVariables) {
        smtSolver->add(storm::expressions::iff(element.first, this->getAbstractionInformation().getPredicateByIndex(element.second)));
        decisionVariables.push_back(element.first);
    }

    relevantPredicatesAndVariables.insert(relevantPredicatesAndVariables.end(), newPredicateVariables.begin(), newPredicateVariables.end());
    std::sort(relevantPredicatesAndVariables.begin(), relevantPredicatesAndVariables.end(),
              [](std::pair<storm::expressions::Variable, uint_fast64_t> const& firstPair,
                 std::pair<storm::expressions::Variable, uint_fast64_t> const& secondPair) { return firstPair.second < secondPair.second; });
}

template<storm::dd::DdType DdType, typename ValueType>
void StateSetAbstractor<DdType, ValueType>::refine(std::vector<uint_fast64_t> const& newPredicates) {
    // Make the partition aware of the new predicates, which may make more predicates relevant to the abstraction.
    for (auto const& predicateIndex : newPredicates) {
        localExpressionInformation.addExpression(predicateIndex);
    }

    std::set<uint_fast64_t> newRelevantPredicateIndices = localExpressionInformation.getRelatedExpressions(concretePredicateVariables);

    // Since the number of relevant predicates is monotonic, we can simply check for the size here.
    STORM_LOG_ASSERT(newRelevantPredicateIndices.size() >= relevantPredicatesAndVariables.size(), "Illegal size of relevant predicates.");
    if (newRelevantPredicateIndices.size() > relevantPredicatesAndVariables.size()) {
        // Before adding the missing predicates, we need to remove the constraint BDD.
        this->popConstraintBdd();

        // If we need to recompute the BDD, we start by introducing decision variables and the corresponding
        // constraints in the SMT problem.
        addMissingPredicates(newRelevantPredicateIndices);

        // Then re-add the constraint BDD.
        this->pushConstraintBdd();
        forceRecomputation = true;
    }
}

template<storm::dd::DdType DdType, typename ValueType>
void StateSetAbstractor<DdType, ValueType>::constrain(storm::expressions::Expression const& constraint) {
    smtSolver->add(constraint);
}

template<storm::dd::DdType DdType, typename ValueType>
void StateSetAbstractor<DdType, ValueType>::constrain(storm::dd::Bdd<DdType> const& newConstraint) {
    // If the constraint is different from the last one, we add it to the solver.
    if (newConstraint != this->constraint) {
        constraint = newConstraint;
        this->pushConstraintBdd();
    }
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> StateSetAbstractor<DdType, ValueType>::getStateBdd(storm::solver::SmtSolver::ModelReference const& model) const {
    storm::dd::Bdd<DdType> result = this->getAbstractionInformation().getDdManager().getBddOne();
    for (auto const& variableIndexPair : relevantPredicatesAndVariables) {
        if (model.getBooleanValue(variableIndexPair.first)) {
            result &= this->getAbstractionInformation().encodePredicateAsSource(variableIndexPair.second);
        } else {
            result &= !this->getAbstractionInformation().encodePredicateAsSource(variableIndexPair.second);
        }
    }
    return result;
}

template<storm::dd::DdType DdType, typename ValueType>
void StateSetAbstractor<DdType, ValueType>::recomputeCachedBdd() {
    STORM_LOG_TRACE("Recomputing BDD for state set abstraction.");

    storm::dd::Bdd<DdType> result = this->getAbstractionInformation().getDdManager().getBddZero();
    smtSolver->allSat(decisionVariables, [&result, this](storm::solver::SmtSolver::ModelReference const& model) {
        result |= getStateBdd(model);
        return true;
    });

    cachedBdd = result;
}

template<storm::dd::DdType DdType, typename ValueType>
void StateSetAbstractor<DdType, ValueType>::popConstraintBdd() {
    // If the last constraint was not the constant one BDD, we need to pop the constraint from the solver.
    if (this->constraint.isOne()) {
        return;
    }
    smtSolver->pop();
}

template<storm::dd::DdType DdType, typename ValueType>
void StateSetAbstractor<DdType, ValueType>::pushConstraintBdd() {
    if (this->constraint.isOne()) {
        return;
    }

    // Create a new backtracking point before adding the constraint.
    smtSolver->push();

    // Create the constraint.
    std::pair<std::vector<storm::expressions::Expression>, std::unordered_map<uint_fast64_t, storm::expressions::Variable>> result =
        constraint.toExpression(this->getAbstractionInformation().getExpressionManager());

    // Then add the constraint.
    for (auto const& expression : result.first) {
        smtSolver->add(expression);
    }

    // Finally associate the level variables with the predicates.
    for (auto const& indexVariablePair : result.second) {
        smtSolver->add(
            storm::expressions::iff(indexVariablePair.second, this->getAbstractionInformation().getPredicateForDdVariableIndex(indexVariablePair.first)));
    }
}

template<storm::dd::DdType DdType, typename ValueType>
storm::dd::Bdd<DdType> StateSetAbstractor<DdType, ValueType>::getAbstractStates() {
    if (forceRecomputation) {
        this->recomputeCachedBdd();
    }
    return cachedBdd;
}

template<storm::dd::DdType DdType, typename ValueType>
AbstractionInformation<DdType>& StateSetAbstractor<DdType, ValueType>::getAbstractionInformation() {
    return abstractionInformation.get();
}

template<storm::dd::DdType DdType, typename ValueType>
AbstractionInformation<DdType> const& StateSetAbstractor<DdType, ValueType>::getAbstractionInformation() const {
    return abstractionInformation.get();
}

template class StateSetAbstractor<storm::dd::DdType::CUDD, double>;
template class StateSetAbstractor<storm::dd::DdType::Sylvan, double>;
template class StateSetAbstractor<storm::dd::DdType::Sylvan, storm::RationalNumber>;
}  // namespace abstraction
}  // namespace storm::gbar
