#pragma once

#include <boost/optional.hpp>
#include <string>
#include <vector>
#include "cpphoafparser/consumer/hoa_consumer.hh"

#include "storm/automata/APSet.h"
#include "storm/automata/AcceptanceCondition.h"

namespace storm {
namespace automata {
class HOAHeader {
   public:
    boost::optional<unsigned int> startState;
    boost::optional<unsigned int> numberOfStates;
    APSet apSet;

    boost::optional<unsigned int> numberOfAcceptanceSets;
    cpphoafparser::HOAConsumer::acceptance_expr::ptr acceptance_expression;
    boost::optional<std::string> accName;
    boost::optional<std::vector<cpphoafparser::IntOrString>> accNameExtraInfo;

    AcceptanceCondition::ptr getAcceptanceCondition() {
        return AcceptanceCondition::ptr(new AcceptanceCondition(*numberOfStates, *numberOfAcceptanceSets, acceptance_expression));
    }
};
}  // namespace automata
}  // namespace storm
