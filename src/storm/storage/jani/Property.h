#pragma once

#include <functional>

#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/logic/CloneVisitor.h"
#include "storm/logic/Formulas.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/modelchecker/results/FilterType.h"
#include "storm/storage/expressions/Variable.h"
#include "storm/utility/macros.h"

namespace storm {
namespace jani {

/**
 *  Property intervals as per Jani Specification.
 *  Currently mainly used to help parsing.
 */
struct PropertyInterval {
    storm::expressions::Expression lowerBound;
    bool lowerBoundStrict = false;
    storm::expressions::Expression upperBound;
    bool upperBoundStrict = false;

    bool hasLowerBound() const {
        return lowerBound.isInitialized();
    }

    bool hasUpperBound() const {
        return upperBound.isInitialized();
    }
};

class FilterExpression {
   public:
    FilterExpression() = default;

    explicit FilterExpression(std::shared_ptr<storm::logic::Formula const> formula,
                              storm::modelchecker::FilterType ft = storm::modelchecker::FilterType::VALUES,
                              std::shared_ptr<storm::logic::Formula const> const& statesFormula = std::make_shared<storm::logic::AtomicLabelFormula>("init"))
        : formula(formula), ft(ft), statesFormula(statesFormula) {
        STORM_LOG_THROW(statesFormula->isInFragment(storm::logic::propositional()), storm::exceptions::InvalidArgumentException,
                        "Can only filter by propositional formula.");
    }

    std::shared_ptr<storm::logic::Formula const> const& getFormula() const {
        return formula;
    }

    std::shared_ptr<storm::logic::Formula const> const& getStatesFormula() const {
        return statesFormula;
    }

    storm::modelchecker::FilterType getFilterType() const {
        return ft;
    }

    bool isDefault() const {
        return (ft == storm::modelchecker::FilterType::VALUES) && statesFormula && statesFormula->isInitialFormula();
    }

    FilterExpression substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const {
        return FilterExpression(formula->substitute(substitution), ft, statesFormula->substitute(substitution));
    }

    FilterExpression substitute(std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const& substitutionFunction) const {
        return FilterExpression(formula->substitute(substitutionFunction), ft, statesFormula->substitute(substitutionFunction));
    }

    FilterExpression substituteLabels(std::map<std::string, std::string> const& labelSubstitution) const {
        return FilterExpression(formula->substitute(labelSubstitution), ft, statesFormula->substitute(labelSubstitution));
    }

    FilterExpression substituteRewardModelNames(std::map<std::string, std::string> const& rewardModelNameSubstitution) const {
        return FilterExpression(formula->substituteRewardModelNames(rewardModelNameSubstitution), ft,
                                statesFormula->substituteRewardModelNames(rewardModelNameSubstitution));
    }

    FilterExpression substituteTranscendentalNumbers() const {
        return FilterExpression(formula->substituteTranscendentalNumbers(), ft, statesFormula->substituteTranscendentalNumbers());
    }

    FilterExpression clone() const {
        storm::logic::CloneVisitor cv;
        return FilterExpression(cv.clone(*formula), ft, cv.clone(*statesFormula));
    }

   private:
    // For now, we assume that the states are always the initial states.
    std::shared_ptr<storm::logic::Formula const> formula;
    storm::modelchecker::FilterType ft;
    std::shared_ptr<storm::logic::Formula const> statesFormula;
};

std::ostream& operator<<(std::ostream& os, FilterExpression const& fe);

class Property {
   public:
    Property() = default;

    /**
     * Constructs the property
     * @param name the name
     * @param formula the formula representation
     * @param undefinedConstants the undefined constants used in the property
     * @param comment An optional comment
     */
    Property(std::string const& name, std::shared_ptr<storm::logic::Formula const> const& formula,
             std::set<storm::expressions::Variable> const& undefinedConstants, std::string const& comment = "");

    /**
     * Constructs the property
     * @param name the name
     * @param formula the formula representation
     * @param comment An optional comment
     */
    Property(std::string const& name, FilterExpression const& fe, std::set<storm::expressions::Variable> const& undefinedConstants,
             std::string const& comment = "");

    /**
     * Get the provided name
     * @return the name
     */
    std::string const& getName() const;

    /**
     * Get the provided comment, if any
     * @return the comment
     */
    std::string const& getComment() const;

    Property substitute(std::map<storm::expressions::Variable, storm::expressions::Expression> const& substitution) const;
    Property substitute(std::function<storm::expressions::Expression(storm::expressions::Expression const&)> const& substitutionFunction) const;
    Property substituteLabels(std::map<std::string, std::string> const& labelSubstitution) const;
    Property substituteRewardModelNames(std::map<std::string, std::string> const& rewardModelNameSubstitution) const;
    Property substituteTranscendentalNumbers() const;
    Property clone() const;

    FilterExpression const& getFilter() const;

    std::string asPrismSyntax() const;

    std::set<storm::expressions::Variable> const& getUndefinedConstants() const;
    bool containsUndefinedConstants() const;
    std::set<storm::expressions::Variable> getUsedVariablesAndConstants() const;
    std::set<std::string> getUsedLabels() const;
    void gatherReferencedRewardModels(std::set<std::string>& rewardModelNames) const;

    std::shared_ptr<storm::logic::Formula const> getRawFormula() const;

   private:
    std::string name;
    std::string comment;
    FilterExpression filterExpression;
    std::set<storm::expressions::Variable> undefinedConstants;
};

std::ostream& operator<<(std::ostream& os, Property const& p);
}  // namespace jani
}  // namespace storm
