#pragma once

#include <boost/variant.hpp>
#include <queue>
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"

// This class implements "location elimination", a technique that can be applied to Jani models to reduce the size of the resulting DTMCs. The basic idea is
// to alternate between unfolding a variable into the location space and eliminating certain locations of the model in a way that preserves reachability
// properties.
// The technique is described in more detail in "Out of Control: Reducing Probabilistic Models by Control-State Elimination" by T. Winkler et al
// (https://arxiv.org/pdf/2011.00983.pdf)
//
// This class keeps track of the model and some additional state, while the actual actions (such as unfolding a variable or eliminating a location) are
// each performed in a separate class (e.g. UnfoldAction or EliminateAction).

namespace storm {
namespace jani {
class JaniLocalEliminator {
   private:
    class AutomatonInfo {
       public:
        explicit AutomatonInfo();
        std::set<uint64_t> potentiallyPartOfProp;
        bool hasSink;
        uint64_t sinkIndex;
    };

   public:
    class Session {
       public:
        explicit Session(Model model, Property property, bool flatten = true);
        Model &getModel();
        void setModel(const Model &model);
        Property &getProperty();
        bool getFinished() const;
        void setFinished(bool finished);

        AutomatonInfo &getAutomatonInfo(const std::string &name);
        void buildAutomataInfo();
        void flatten_automata();
        void addMissingGuards(const std::string &automatonName);

        expressions::Expression getNewGuard(const Edge &edge, const EdgeDestination &dest, const Edge &outgoing);
        expressions::Expression getProbability(const EdgeDestination &first, const EdgeDestination &then);
        OrderedAssignments executeInSequence(const EdgeDestination &first, const EdgeDestination &then, std::set<std::string> &rewardVariables);
        bool isEliminable(const std::string &automatonName, std::string const &locationName);
        bool hasLoops(const std::string &automatonName, std::string const &locationName);
        bool hasNamedActions(const std::string &automatonName, std::string const &locationName);
        bool isPossiblyInitial(const std::string &automatonName, std::string const &locationName);
        bool isPartOfProp(const std::string &automatonName, std::string const &locationName);
        bool isPartOfProp(const std::string &automatonName, uint64_t locationIndex);
        bool computeIsPartOfProp(const std::string &automatonName, const std::string &locationName);
        bool computeIsPartOfProp(const std::string &automatonName, uint64_t locationIndex);
        bool computeIsPartOfProp(const std::map<expressions::Variable, expressions::Expression> &substitutionMap);
        void setPartOfProp(const std::string &automatonName, const std::string &locationName, bool isPartOfProp);
        void setPartOfProp(const std::string &automatonName, uint64_t locationIndex, bool isPartOfProp);
        void clearIsPartOfProp(const std::string &automatonName);
        bool isVariablePartOfProperty(const std::string &expressionVariableName);

        bool isRewardFormula;
        std::set<std::string> rewardModels;

       private:
        Model model;
        Property property;
        bool finished;

        std::map<std::string, AutomatonInfo> automataInfo;
        std::set<uint_fast64_t> expressionVarsInProperty;
    };

   public:
    class Action {
       public:
        virtual ~Action() = default;
        virtual std::string getDescription() = 0;
        virtual void doAction(Session &session) = 0;
    };

    class EliminationScheduler {
       public:
        EliminationScheduler();
        std::unique_ptr<Action> getNextAction();
        void addAction(std::unique_ptr<Action> action);

       private:
        std::queue<std::unique_ptr<Action>> actionQueue;
    };

    EliminationScheduler scheduler;
    explicit JaniLocalEliminator(Model const &original, storm::jani::Property &property, bool addMissingGuards = false);
    explicit JaniLocalEliminator(Model const &original, std::vector<storm::jani::Property> &properties, bool addMissingGuards = false);
    static Model eliminateAutomatically(const Model &model, std::vector<jani::Property> properties, uint64_t locationHeuristic, uint64_t edgesHeuristic);
    void eliminate(bool flatten = true);
    Model const &getResult();

   private:
    Model const &original;
    Model newModel;
    Property property;
    bool addMissingGuards;

    void setProperty(storm::jani::Property &property);
};
}  // namespace jani
}  // namespace storm
