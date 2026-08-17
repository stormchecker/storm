#include "storm-config.h"
#include "test/storm_gtest.h"

#include <sstream>
#include <string>

#include "storm/automata/DeterministicAutomaton.h"

TEST(DeterministicAutomaton, ParseAutomaton) {
    std::string aUb =
        "HOA: v1\n"
        "States: 3\n"
        "Start: 0\n"
        "acc-name: Rabin 1\n"
        "Acceptance: 2 (Fin(0) & Inf(1))\n"
        "AP: 2 \"a\" \"b\""
        "--BODY--\n"
        "State: 0 \"a U b\" { 0 }\n"
        "  2  /* !a  & !b */\n"
        "  0  /*  a  & !b */\n"
        "  1  /* !a  &  b */\n"
        "  1  /*  a  &  b */\n"
        "State: 1 { 1 }\n"
        "  1 1 1 1       /* four transitions on one line */\n"
        "State: 2 \"sink state\" { 0 }\n"
        "  2 2 2 2\n"
        "--END--\n";

    std::istringstream in = std::istringstream(aUb);
    storm::automata::DeterministicAutomaton::ptr da;
    ASSERT_NO_THROW(da = storm::automata::DeterministicAutomaton::parse(in));
    // da->printHOA(std::cout);
}
