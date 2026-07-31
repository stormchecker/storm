#pragma once

#include "cpphoafparser/consumer/hoa_consumer.hh"
#include "cpphoafparser/util/implicit_edge_helper.hh"
#include "storm/automata/APSet.h"
#include "storm/automata/DeterministicAutomaton.h"
#include "storm/automata/HOAHeader.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/SparseMatrix.h"
#include "storm/utility/macros.h"

#include <boost/optional.hpp>
#include <exception>

namespace storm {
namespace automata {

class HOAConsumerDAHeader : public cpphoafparser::HOAConsumer {
   protected:
    HOAHeader header;

   public:
    typedef std::shared_ptr<HOAConsumerDAHeader> ptr;

    struct header_parsing_done : public std::exception {};

    HOAHeader& getHeader() {
        return header;
    }

    virtual bool parserResolvesAliases() {
        return true;
    }

    /** Called by the parser for the "HOA: version" item [mandatory, once]. */
    virtual void notifyHeaderStart(const std::string& /*version*/) {
        // TODO: Check version
    }

    /** Called by the parser for the "States: int(numberOfStates)" item [optional, once]. */
    virtual void setNumberOfStates(unsigned int numberOfStates) {
        header.numberOfStates = numberOfStates;
    }

    /**
     * Called by the parser for each "Start: state-conj" item [optional, multiple].
     * @param stateConjunction a list of state indizes, interpreted as a conjunction
     **/
    virtual void addStartStates(const int_list& stateConjunction) {
        STORM_LOG_THROW(!header.startState, storm::exceptions::NotSupportedException,
                        "Parsing deterministic HOA automaton: Nondeterministic choice of start states not supported");
        STORM_LOG_THROW(stateConjunction.size() == 1, storm::exceptions::NotSupportedException,
                        "Parsing deterministic HOA automaton: Conjunctive choice of start states not supported");
        header.startState = stateConjunction.at(0);
    }

    /**
     * Called by the parser for the "AP: ap-def" item [optional, once].
     * @param aps the list of atomic propositions
     */
    virtual void setAPs(const std::vector<std::string>& aps) {
        for (const std::string& ap : aps) {
            header.apSet.add(ap);
        }
    }

    /**
     * Called by the parser for the "Acceptance: acceptance-def" item [mandatory, once].
     * @param numberOfSets the number of acceptance sets used to tag state / transition acceptance
     * @param accExpr a boolean expression over acceptance atoms
     **/
    virtual void setAcceptanceCondition(unsigned int numberOfSets, acceptance_expr::ptr accExpr) {
        header.numberOfAcceptanceSets = numberOfSets;
        header.acceptance_expression = accExpr;
    }

    /**
     * Called by the parser for each "acc-name: ..." item [optional, multiple].
     * @param name the provided name
     * @param extraInfo the additional information for this item
     * */
    virtual void provideAcceptanceName(const std::string& name, const std::vector<cpphoafparser::IntOrString>& extraInfo) {
        header.accName = name;
        header.accNameExtraInfo = extraInfo;
    }

    /**
     * Called by the parser for each "Alias: alias-def" item [optional, multiple].
     * Will be called no matter the return value of `parserResolvesAliases()`.
     *
     *  @param name the alias name (without @)
     *  @param labelExpr a boolean expression over labels
     **/
    virtual void addAlias(const std::string& name, label_expr::ptr labelExpr) {
        // IGNORE
        (void)name;
        (void)labelExpr;
    }

    /**
     * Called by the parser for the "name: ..." item [optional, once].
     **/
    virtual void setName(const std::string& /*name*/) {
        // IGNORE
    }

    /**
     * Called by the parser for the "tool: ..." item [optional, once].
     * @param name the tool name
     * @param version the tool version (option, empty pointer if not provided)
     **/
    virtual void setTool(const std::string& /*name*/, std::shared_ptr<std::string> /*version*/) {
        // IGNORE
    }

    /**
     * Called by the parser for the "properties: ..." item [optional, multiple].
     * @param properties a list of properties
     */
    virtual void addProperties(const std::vector<std::string>& /*properties*/) {
        // TODO: check supported
    }

    /**
     * Called by the parser for each unknown header item [optional, multiple].
     * @param name the name of the header (without ':')
     * @param content a list of extra information provided by the header
     */
    virtual void addMiscHeader(const std::string& /*name*/, const std::vector<cpphoafparser::IntOrString>& /*content*/) {
        // TODO: Check semantic headers
    }

    /**
     * Called by the parser to notify that the BODY of the automaton has started [mandatory, once].
     */
    virtual void notifyBodyStart() {
        throw header_parsing_done();
    }

    /**
     * Called by the parser for each "State: ..." item [multiple].
     * @param id the identifier for this state
     * @param info an optional string providing additional information about the state (empty pointer if not provided)
     * @param labelExpr an optional boolean expression over labels (state-labeled) (empty pointer if not provided)
     * @param accSignature an optional list of acceptance set indizes (state-labeled acceptance) (empty pointer if not provided)
     */
    virtual void addState(unsigned int /*id*/, std::shared_ptr<std::string> /*info*/, label_expr::ptr /*labelExpr*/,
                          std::shared_ptr<int_list> /*accSignature*/) {
        // IGNORE
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
    virtual void addEdgeImplicit(unsigned int /*stateId*/, const int_list& /*conjSuccessors*/, std::shared_ptr<int_list> /*accSignature*/) {
        // IGNORE
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
    virtual void addEdgeWithLabel(unsigned int /*stateId*/, label_expr::ptr /*labelExpr*/, const int_list& /*conjSuccessors*/,
                                  std::shared_ptr<int_list> /*accSignature*/) {
        // IGNORE
    }

    /**
     * Called by the parser to notify the consumer that the definition for state `stateId`
     * has ended [multiple].
     */
    virtual void notifyEndOfState(unsigned int /*stateId*/) {
        // IGNORE
    }

    /**
     * Called by the parser to notify the consumer that the automata definition has ended [mandatory, once].
     */
    virtual void notifyEnd() {}

    /**
     * Called by the parser to notify the consumer that an "ABORT" message has been encountered
     * (at any time, indicating error, the automaton should be discarded).
     */
    virtual void notifyAbort() {
        STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Parsing deterministic automaton: Automaton is incomplete (abort)");
    }

    /**
     * Is called whenever a condition is encountered that merits a (non-fatal) warning.
     * The consumer is free to handle this situation as it wishes.
     */
    virtual void notifyWarning(const std::string& warning) {
        // IGNORE
        (void)warning;
    }
};

}  // namespace automata
}  // namespace storm
