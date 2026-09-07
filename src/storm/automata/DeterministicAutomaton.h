#pragma once

#include <iostream>
#include <memory>

#include "storm/automata/APSet.h"

namespace storm {
namespace automata {
// fwd
class AcceptanceCondition;

class DeterministicAutomaton {
   public:
    typedef std::shared_ptr<DeterministicAutomaton> ptr;

    DeterministicAutomaton(APSet apSet, std::size_t numberOfStates, std::size_t initialState, std::shared_ptr<AcceptanceCondition> acceptance);

    const APSet& getAPSet() const;

    std::size_t getInitialState() const;

    std::size_t getSuccessor(std::size_t from, APSet::alphabet_element label) const;
    void setSuccessor(std::size_t from, APSet::alphabet_element label, std::size_t successor);

    std::size_t getNumberOfStates() const;
    std::size_t getNumberOfEdgesPerState() const;

    std::shared_ptr<AcceptanceCondition> getAcceptance() const;

    void printHOA(std::ostream& out) const;

    static DeterministicAutomaton::ptr parse(std::istream& in);
    static DeterministicAutomaton::ptr parseFromFile(const std::string& filename);

   private:
    APSet apSet;
    std::size_t numberOfStates;
    std::size_t initialState;
    std::size_t numberOfEdges;
    std::size_t edgesPerState;
    std::shared_ptr<AcceptanceCondition> acceptance;
    std::vector<std::size_t> successors;
};
}  // namespace automata
}  // namespace storm
