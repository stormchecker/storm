#pragma once

#include <memory>
#include "storm/storage/umb/model/ValuationDescription.h"
#include "storm/utility/NumberTraits.h"

namespace storm {
namespace expressions {
class Variable;
class ExpressionManager;
}  // namespace expressions

namespace umb {
class ValuationDescriptionBuilder {
   public:
    using Integer = storm::NumberTraits<storm::RationalNumber>::IntegerType;

    ValuationDescriptionBuilder(std::shared_ptr<storm::expressions::ExpressionManager const> const& expressionManager);

    storm::expressions::ExpressionManager const& getManager() const;

    /*!
     * Adds a new boolean variable to the builder.
     */
    void addBooleanVariable(storm::expressions::Variable const& variable, bool optional = false);

    /*!
     * Adds a new integer variable to the builder.
     */
    void addIntegerVariable(storm::expressions::Variable const& variable, int64_t const lowerBound, int64_t const upperBound, bool optional = false);

    /*!
     * Adds a new integer variable to the builder.
     */
    void addIntegerVariable(storm::expressions::Variable const& variable, Integer const lowerBound, Integer const upperBound, bool optional = false);

    /*!
     * Adds a new double variable to the builder.
     */
    void addDoubleVariable(storm::expressions::Variable const& variable, bool optional = false);

    /*!
     * Adds a new rational variable to the builder.
     */
    void addRationalVariable(storm::expressions::Variable const& variable, uint64_t bitSize, bool optional = false);

    /*!
     * Adds a new string variable to the builder.
     */
    void addStringVariable(storm::expressions::Variable const& variable, bool optional = false);

    /*!
     * Adds the given variable.
     */
    void addVariable(storm::umb::ValuationClassDescription::Variable const& variable);

    /*! Adds all variables from the given description.
     * If addPadding is true, padding entries in the description are added, otherwise they are ignored.
     */
    void addVariables(storm::umb::ValuationClassDescription const& description, bool addPadding = false);

    /*!
     * Creates the finalized state valuations object.
     */
    ValuationClassDescription buildClassDescription();

   private:
    void finalize();
    std::shared_ptr<storm::expressions::ExpressionManager const> const manager;
    storm::umb::ValuationClassDescription descr;
};

}  // namespace umb
}  // namespace storm
