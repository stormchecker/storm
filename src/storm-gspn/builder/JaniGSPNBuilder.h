#pragma once

#include "storm-gspn/storage/gspn/GSPN.h"
#include "storm/expressions/ExpressionManager.h"
#include "storm/languages/jani/Model.h"
#include "storm/languages/jani/Property.h"

namespace storm {
namespace builder {
class JaniGSPNBuilder {
   public:
    JaniGSPNBuilder(storm::gspn::GSPN const& gspn) : gspn(gspn), expressionManager(gspn.getExpressionManager()) {}

    virtual ~JaniGSPNBuilder() {
        // Intentionally left empty.
    }

    storm::jani::Model* build(std::string const& automatonName = "gspn_automaton");

    storm::jani::Variable const& getPlaceVariable(uint64_t placeId) const {
        return *vars.at(placeId);
    }

    /*!
     * Get standard properties (reachability, time bounded reachability, expected time) for a given atomic formula.
     */
    std::vector<storm::jani::Property> getStandardProperties(storm::jani::Model* model, std::shared_ptr<storm::logic::AtomicExpressionFormula> atomicFormula,
                                                             std::string name, std::string description, bool maximal);

    /*!
     * Get standard properties (reachability, time bounded reachability, expected time) for deadlocks.
     */
    std::vector<storm::jani::Property> getDeadlockProperties(storm::jani::Model* model);

    /*!
     * Add transient variable representing given expression.
     */
    storm::jani::Variable const& addTransientVariable(storm::jani::Model* model, std::string name, storm::expressions::Expression expression);

   private:
    void addVariables(storm::jani::Model* model);

    uint64_t addLocation(storm::jani::Automaton& automaton);

    void addEdges(storm::jani::Automaton& automaton, uint64_t locId);

    storm::jani::Variable const& addDeadlockTransientVariable(storm::jani::Model* model, std::string name, bool ignoreCapacities = false,
                                                              bool ignoreInhibitorArcs = false, bool ignoreEmptyPlaces = false);

    const uint64_t janiVersion = 1;
    storm::gspn::GSPN const& gspn;
    std::map<uint64_t, storm::jani::Variable const*> vars;
    std::shared_ptr<storm::expressions::ExpressionManager> expressionManager;
};
}  // namespace builder
}  // namespace storm
