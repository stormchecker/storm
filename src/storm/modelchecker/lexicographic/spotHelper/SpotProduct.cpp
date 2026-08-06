#include "storm/modelchecker/lexicographic/spotHelper/SpotProduct.h"

#include <deque>

#include "storm/adapters/SpotAdapter.h"
#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/logic/Formulas.h"
#include "storm/models/sparse/Mdp.h"
#include "storm/utility/macros.h"

namespace storm::modelchecker::helper::lexicographic::spothelper {

typedef std::pair<unsigned, unsigned> product_state;

struct product_state_hash {
#ifdef STORM_HAVE_SPOT
    size_t operator()(product_state s) const noexcept {
        return spot::wang32_hash(s.first ^ spot::wang32_hash(s.second));
    }
#else
    size_t operator()(product_state) const {
        STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Spot support.");
    }
#endif
};

enum acc_op { and_acc, or_acc, xor_acc, xnor_acc };
typedef std::vector<std::pair<unsigned, unsigned>> product_states;

std::shared_ptr<storm::automata::DeterministicAutomaton> ltl2daSpotProduct(storm::logic::MultiObjectiveFormula const& formula,
                                                                           storm::logic::ExtractMaximalStateFormulasVisitor::ApToFormulaMap& extracted,
                                                                           std::vector<uint64_t>& acceptanceConditions) {
#ifdef STORM_HAVE_SPOT
    bool first = true;
    spot::twa_graph_ptr productAutomaton;
    spot::bdd_dict_ptr dict = spot::make_bdd_dict();
    uint64_t countAccept = 0;
    // iterate over all subformulae
    for (const std::shared_ptr<const storm::logic::Formula>& subFormula : formula.getSubformulas()) {
        // get the formula in the right format
        STORM_LOG_ASSERT(subFormula->isProbabilityOperatorFormula(), "subformula " << *subFormula << " has unexpected type.");
        auto const& pathFormula = subFormula->asProbabilityOperatorFormula().getSubformula().asPathFormula();

        // get map of state-expressions to propositions
        std::shared_ptr<storm::logic::Formula> ltlFormula1 = storm::logic::ExtractMaximalStateFormulasVisitor::extract(pathFormula, extracted);

        // parse the formula in spot-format
        std::string prefixLtl = ltlFormula1->toPrefixString();
        spot::parsed_formula spotPrefixLtl = spot::parse_prefix_ltl(prefixLtl);
        if (!spotPrefixLtl.errors.empty()) {
            std::ostringstream errorMsg;
            spotPrefixLtl.format_errors(errorMsg);
            STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException,
                            "Spot could not parse formula: " << prefixLtl << ": " << errorMsg.str() << ".");
        }
        spot::formula spotFormula = spotPrefixLtl.f;

        // Request a deterministic, complete automaton with state-based acceptance with parity-acceptance condition (should result in Streett)
        spot::translator trans = spot::translator(dict);
        trans.set_type(spot::postprocessor::Parity);
        trans.set_pref(spot::postprocessor::Deterministic | spot::postprocessor::SBAcc | spot::postprocessor::Complete | spot::postprocessor::Colored);
        // aut contains the Spot-automaton
        auto aut = trans.run(spotFormula);

        acceptanceConditions.push_back(countAccept);
        countAccept += aut->get_acceptance().top_conjuncts().size();
        if (first) {
            // the first automaton does not need to be merged with the product automaton
            productAutomaton = aut;
            first = false;
            continue;
        } else {
            // create a product of the the new automaton and the already existing product automaton
            spot::const_twa_graph_ptr left = aut;
            spot::const_twa_graph_ptr right = productAutomaton;
            unsigned left_state = left->get_init_state_number();
            unsigned right_state = right->get_init_state_number();

            auto res = spot::make_twa_graph(left->get_dict());
            res->copy_ap_of(left);
            res->copy_ap_of(right);

            auto left_num = left->num_sets();
            auto& left_acc = left->get_acceptance();
            auto right_acc = right->get_acceptance() << left_num;
            right_acc &= left_acc;

            res->set_acceptance(left_num + right->num_sets(), right_acc);

            auto merge_acc = [&](spot::acc_cond::mark_t ml, spot::acc_cond::mark_t mr) { return ml | (mr << left_num); };
            std::unordered_map<product_state, unsigned, product_state_hash> s2n;
            std::deque<std::pair<product_state, unsigned>> todo;

            auto v = new product_states;
            res->set_named_prop("product-states", v);

            auto new_state = [&](unsigned left_state, unsigned right_state) -> unsigned {
                product_state x(left_state, right_state);
                auto p = s2n.emplace(x, 0);
                if (p.second)  // This is a new state
                {
                    p.first->second = res->new_state();
                    todo.emplace_back(x, p.first->second);
                    assert(p.first->second == v->size());
                    v->emplace_back(x);
                }
                return p.first->second;
            };

            res->set_init_state(new_state(left_state, right_state));
            while (!todo.empty()) {
                auto top = todo.front();
                todo.pop_front();
                for (auto& l : left->out(top.first.first))
                    for (auto& r : right->out(top.first.second)) {
                        auto cond = l.cond & r.cond;
                        if (cond == bddfalse)
                            continue;
                        auto dst = new_state(l.dst, r.dst);
                        res->new_edge(top.second, dst, cond, merge_acc(l.acc, r.acc));
                        // If right is deterministic, we can abort immediately!
                    }
            }

            if (res->acc().is_f()) {
                assert(res->num_edges() == 0);
                res->prop_universal(true);
                res->prop_complete(false);
                res->prop_stutter_invariant(true);
                res->prop_terminal(true);
                res->prop_state_acc(true);
            } else {
                // The product of two non-deterministic automata could be
                // deterministic.  Likewise for non-complete automata.
                if (left->prop_universal() && right->prop_universal())
                    res->prop_universal(true);
                if (left->prop_complete() && right->prop_complete())
                    res->prop_complete(true);
                if (left->prop_stutter_invariant() && right->prop_stutter_invariant())
                    res->prop_stutter_invariant(true);
                if (left->prop_inherently_weak() && right->prop_inherently_weak())
                    res->prop_inherently_weak(true);
                if (left->prop_weak() && right->prop_weak())
                    res->prop_weak(true);
                if (left->prop_terminal() && right->prop_terminal())
                    res->prop_terminal(true);
                res->prop_state_acc(left->prop_state_acc() && right->prop_state_acc());
            }
            productAutomaton = res;
        }
    }
    acceptanceConditions.push_back(countAccept);

    if (!(productAutomaton->get_acceptance().is_cnf())) {
        // Transform the acceptance condition in disjunctive normal form and merge all the Fin-sets of each clause
        productAutomaton = to_generalized_streett(productAutomaton, true);
    }
    std::stringstream autStream;
    // Print reachable states in HOA format, implicit edges (i), state-based acceptance (s)
    spot::print_hoa(autStream, productAutomaton, "is");

    // parse the automaton into storm-format
    storm::automata::DeterministicAutomaton::ptr da = storm::automata::DeterministicAutomaton::parse(autStream);

    return da;
#else
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Spot support.");
    (void)formula;
    (void)extracted;
    (void)acceptanceConditions;
#endif
}

}  // namespace storm::modelchecker::helper::lexicographic::spothelper
