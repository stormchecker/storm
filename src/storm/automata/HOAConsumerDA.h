#pragma once

#include "storm/automata/APSet.h"
#include "storm/automata/DeterministicAutomaton.h"
#include "storm/automata/HOAConsumerDAHeader.h"
#include "storm/exceptions/InvalidOperationException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/OutOfRangeException.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/solver/SmtSolver.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/utility/solver.h"

#include "cpphoafparser/consumer/hoa_consumer.hh"
#include "cpphoafparser/util/implicit_edge_helper.hh"

#include <boost/optional.hpp>
#include <exception>

namespace storm {
namespace automata {

class HOAConsumerDA : public HOAConsumerDAHeader {
   private:
    AcceptanceCondition::ptr acceptance;

    DeterministicAutomaton::ptr da;
    cpphoafparser::ImplicitEdgeHelper* helper = nullptr;

    std::shared_ptr<storm::expressions::ExpressionManager> expressionManager;
    std::vector<storm::expressions::Variable> apVariables;
    std::unique_ptr<storm::solver::SmtSolver> solver;

    storm::storage::BitVector seenEdges;

   public:
    typedef std::shared_ptr<HOAConsumerDA> ptr;

    HOAConsumerDA() : seenEdges(0) {
        expressionManager.reset(new storm::expressions::ExpressionManager());
        storm::utility::solver::SmtSolverFactory factory;
        solver = factory.create(*expressionManager);
    }

    ~HOAConsumerDA() {
        delete helper;
    }

    DeterministicAutomaton::ptr getDA() {
        return da;
    }

    /**
     * Called by the parser to notify that the BODY of the automaton has started [mandatory, once].
     */
    virtual void notifyBodyStart() {
        STORM_LOG_THROW(header.numberOfStates, storm::exceptions::WrongFormatException,
                        "Parsing deterministic HOA automaton: Missing number-of-states header.");

        acceptance = header.getAcceptanceCondition();
        da.reset(new DeterministicAutomaton(header.apSet, *header.numberOfStates, *header.startState, acceptance));

        helper = new cpphoafparser::ImplicitEdgeHelper(header.apSet.size());

        seenEdges.resize(*header.numberOfStates * helper->getEdgesPerState());

        for (const std::string& ap : header.apSet.getAPs()) {
            apVariables.push_back(expressionManager->declareBooleanVariable(ap));
        }
    }

    /**
     * Called by the parser for each "State: ..." item [multiple].
     * @param id the identifier for this state
     * @param info an optional string providing additional information about the state (empty pointer if not provided)
     * @param labelExpr an optional boolean expression over labels (state-labeled) (empty pointer if not provided)
     * @param accSignature an optional list of acceptance set indizes (state-labeled acceptance) (empty pointer if not provided)
     */
    virtual void addState(unsigned int id, std::shared_ptr<std::string> info, label_expr::ptr labelExpr, std::shared_ptr<int_list> accSignature) {
        if (accSignature) {
            for (unsigned int accSet : *accSignature) {
                acceptance->getAcceptanceSet(accSet).set(id);
            }
        }

        STORM_LOG_THROW(labelExpr.get() == nullptr, storm::exceptions::NotSupportedException,
                        "Parsing deterministic HOA automaton: State-labeled automata not supported.");

        helper->startOfState(id);
    }

    /**
     * Called by the parser for each implicit edge definition [multiple], i.e.,
     * where the edge label is deduced from the index of the edge.
     *
     * If the edges are provided in implicit form, after every `addState()` there should be 2^|AP| calls to
     * `addEdgeImplicit`. The corresponding boolean expression over labels / BitSet
     * can be obtained by calling BooleanExpression.fromImplicit(i-1) for the i-th call of this function per state.
     *
     * @param stateId the index of the 'from' state
     * @param conjSuccessors a list of successor state indizes, interpreted as a conjunction
     * @param accSignature an optional list of acceptance set indizes (transition-labeled acceptance) (empty pointer if not provided)
     */
    virtual void addEdgeImplicit(unsigned int stateId, const int_list& conjSuccessors, std::shared_ptr<int_list> accSignature) {
        std::size_t edgeIndex = helper->nextImplicitEdge();

        STORM_LOG_THROW(conjSuccessors.size() == 1, storm::exceptions::NotSupportedException,
                        "Parsing deterministic HOA automaton: Does not support alternation (conjunction of successor states).");

        STORM_LOG_THROW(!accSignature, storm::exceptions::NotSupportedException,
                        "Parsing deterministic HOA automaton: Does not support transition-based acceptance.");

        da->setSuccessor(stateId, edgeIndex, conjSuccessors.at(0));
        markEdgeAsSeen(stateId, edgeIndex);
    }

    /**
     * Called by the parser for each explicit edge definition [optional, multiple], i.e.,
     * where the label is either specified for the edge or as a state-label.
     * <br/>
     * @param stateId the index of the 'from' state
     * @param labelExpr a boolean expression over labels (empty pointer if none provided, only in case of state-labeled states)
     * @param conjSuccessors a list of successors state indizes, interpreted as a conjunction
     * @param accSignature an optional list of acceptance set indizes (transition-labeled acceptance) (empty pointer if none provided)
     */
    virtual void addEdgeWithLabel(unsigned int stateId, label_expr::ptr labelExpr, const int_list& conjSuccessors, std::shared_ptr<int_list> accSignature) {
        STORM_LOG_THROW(conjSuccessors.size() == 1, storm::exceptions::NotSupportedException,
                        "Parsing deterministic HOA automaton: Does not support alternation (conjunction of successor states).");

        STORM_LOG_THROW(!accSignature, storm::exceptions::NotSupportedException,
                        "Parsing deterministic HOA automaton: Does not support transition-based acceptance.");

        std::size_t successor = conjSuccessors.at(0);

        solver->reset();
        solver->add(labelToStormExpression(labelExpr));

        solver->allSat(apVariables, [this, stateId, successor](storm::expressions::SimpleValuation& valuation) {
            // construct edge index from valuation
            APSet::alphabet_element edgeIndex = header.apSet.elementAllFalse();
            for (std::size_t i = 0; i < apVariables.size(); i++) {
                if (valuation.getBooleanValue(apVariables[i])) {
                    edgeIndex = header.apSet.elementAddAP(edgeIndex, i);
                }
            }

            // require: edge already exists -> same successor
            STORM_LOG_THROW(!alreadyHaveEdge(stateId, edgeIndex) || da->getSuccessor(stateId, edgeIndex) == successor,
                            storm::exceptions::InvalidOperationException,
                            "HOA automaton: multiple definitions of successor for state " << stateId << " and edge " << edgeIndex << ".");

            // std::cout << stateId << " -(" << edgeIndex << ")-> " << successor << '\n';
            da->setSuccessor(stateId, edgeIndex, successor);
            markEdgeAsSeen(stateId, edgeIndex);

            // continue with next valuation
            return true;
        });
    }

    /**
     * Called by the parser to notify the consumer that the definition for state `stateId`
     * has ended [multiple].
     */
    virtual void notifyEndOfState(unsigned int /*stateId*/) {
        helper->endOfState();
    }

    /**
     * Called by the parser to notify the consumer that the automata definition has ended [mandatory, once].
     */
    virtual void notifyEnd() {
        // require that we have seen all edges, i.e., that the automaton is complete
        STORM_LOG_THROW(seenEdges.full(), storm::exceptions::InvalidOperationException, "HOA automaton has mismatch in number of edges, not complete?");
    }

    /**
     * Called by the parser to notify the consumer that an "ABORT" message has been encountered
     * (at any time, indicating error, the automaton should be discarded).
     */
    virtual void notifyAbort() {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Parsing deterministic automaton: Automaton is incomplete (abort).");
    }

    /**
     * Is called whenever a condition is encountered that merits a (non-fatal) warning.
     * The consumer is free to handle this situation as it wishes.
     */
    virtual void notifyWarning(const std::string& warning) {
        // IGNORE
        (void)warning;
    }

   private:
    storm::expressions::Expression labelToStormExpression(label_expr::ptr labelExpr) {
        switch (labelExpr->getType()) {
            case label_expr::EXP_AND:
                return labelToStormExpression(labelExpr->getLeft()) && labelToStormExpression(labelExpr->getRight());
            case label_expr::EXP_OR:
                return labelToStormExpression(labelExpr->getLeft()) || labelToStormExpression(labelExpr->getRight());
            case label_expr::EXP_NOT:
                return !labelToStormExpression(labelExpr->getLeft());
            case label_expr::EXP_TRUE:
                return expressionManager->boolean(true);
            case label_expr::EXP_FALSE:
                return expressionManager->boolean(false);
            case label_expr::EXP_ATOM: {
                unsigned int apIndex = labelExpr->getAtom().getAPIndex();
                STORM_LOG_THROW(apIndex < apVariables.size(), storm::exceptions::OutOfRangeException,
                                "HOA automaton refers to non-existing atomic proposition.");
                return apVariables.at(apIndex).getExpression();
            }
        }
        STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unknown label expression operator.");
    }

    bool alreadyHaveEdge(std::size_t stateId, std::size_t edgeIndex) {
        return seenEdges.get(stateId * helper->getEdgesPerState() + edgeIndex);
    }

    void markEdgeAsSeen(std::size_t stateId, std::size_t edgeIndex) {
        seenEdges.set(stateId * helper->getEdgesPerState() + edgeIndex);
    }
};

}  // namespace automata
}  // namespace storm
