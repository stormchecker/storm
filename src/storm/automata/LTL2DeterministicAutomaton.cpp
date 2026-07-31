#include "storm/automata/LTL2DeterministicAutomaton.h"
#include "storm/adapters/SpotAdapter.h"
#include "storm/automata/DeterministicAutomaton.h"

#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/FileIoException.h"
#include "storm/exceptions/MissingLibraryException.h"
#include "storm/logic/Formula.h"
#include "storm/utility/macros.h"

#include <sys/wait.h>

namespace storm {
namespace automata {

std::shared_ptr<DeterministicAutomaton> LTL2DeterministicAutomaton::ltl2daSpot(storm::logic::Formula const& f, bool dnf) {
#ifdef STORM_HAVE_SPOT
    std::string prefixLtl = f.toPrefixString();

    spot::parsed_formula spotPrefixLtl = spot::parse_prefix_ltl(prefixLtl);
    if (!spotPrefixLtl.errors.empty()) {
        std::ostringstream errorMsg;
        spotPrefixLtl.format_errors(errorMsg);
        STORM_LOG_THROW(false, storm::exceptions::ExpressionEvaluationException, "Spot could not parse formula: " << prefixLtl << ": " << errorMsg.str());
    }
    spot::formula spotFormula = spotPrefixLtl.f;

    // Request a deterministic, complete automaton with state-based acceptance
    spot::translator trans = spot::translator();
    trans.set_type(spot::postprocessor::Generic);
    trans.set_pref(spot::postprocessor::Deterministic | spot::postprocessor::SBAcc | spot::postprocessor::Complete);
    STORM_LOG_INFO("Construct deterministic automaton for " << spotFormula);
    auto aut = trans.run(spotFormula);

    if (!(aut->get_acceptance().is_dnf()) && dnf) {
        STORM_LOG_INFO("Convert acceptance condition " << aut->get_acceptance() << " into DNF...");
        // Transform the acceptance condition in disjunctive normal form and merge all the Fin-sets of each clause
        aut = to_generalized_rabin(aut, true);
    }

    STORM_LOG_INFO("The deterministic automaton has acceptance condition:  " << aut->get_acceptance());

    STORM_LOG_INFO(aut->get_acceptance());

    std::stringstream autStream;
    // Print reachable states in HOA format, implicit edges (i), state-based acceptance (s)
    spot::print_hoa(autStream, aut, "is");

    storm::automata::DeterministicAutomaton::ptr da = DeterministicAutomaton::parse(autStream);

    return da;

#else
    (void)f;
    (void)dnf;
    STORM_LOG_THROW(false, storm::exceptions::MissingLibraryException, "Storm is compiled without Spot support.");
#endif
}

std::shared_ptr<DeterministicAutomaton> LTL2DeterministicAutomaton::ltl2daExternalTool(storm::logic::Formula const& f, std::string ltl2daTool) {
    std::string prefixLtl = f.toPrefixString();

    STORM_LOG_INFO("Calling external LTL->DA tool:   " << ltl2daTool << " '" << prefixLtl << "' da.hoa");

    pid_t pid;

    pid = fork();
    STORM_LOG_THROW(pid >= 0, storm::exceptions::FileIoException, "Could not construct deterministic automaton, fork failed");

    if (pid == 0) {
        // we are in the child process
        if (execlp(ltl2daTool.c_str(), ltl2daTool.c_str(), prefixLtl.c_str(), "da.hoa", NULL) < 0) {
            std::cerr << "ERROR: exec failed: " << strerror(errno) << '\n';
            std::exit(1);
        }
        // never reached
        return std::shared_ptr<DeterministicAutomaton>();
    } else {  // in the parent
        int status;

        // wait for completion
        while (wait(&status) != pid) {
            // Intentionally left emtpty
        }

        int rv;
        if (WIFEXITED(status)) {
            rv = WEXITSTATUS(status);
        } else {
            STORM_LOG_THROW(false, storm::exceptions::FileIoException, "Could not construct deterministic automaton: process aborted");
        }
        STORM_LOG_THROW(rv == 0, storm::exceptions::FileIoException,
                        "Could not construct deterministic automaton for " << prefixLtl << ", return code = " << rv);

        STORM_LOG_INFO("Reading automaton for " << prefixLtl << " from da.hoa");

        return DeterministicAutomaton::parseFromFile("da.hoa");
    }
}

}  // namespace automata

}  // namespace storm
