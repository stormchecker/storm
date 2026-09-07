#pragma once

#include "storm/storage/expressions/SubstitutionVisitor.h"
#include "storm/storage/jani/expressions/JaniExpressions.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"

namespace storm {

namespace jani {
storm::expressions::Expression substituteJaniExpression(storm::expressions::Expression const& expression,
                                                        std::map<storm::expressions::Variable, storm::expressions::Expression> const& identifierToExpressionMap,
                                                        bool const substituteTranscendentalNumbers);
storm::expressions::Expression substituteJaniExpression(
    storm::expressions::Expression const& expression,
    std::unordered_map<storm::expressions::Variable, storm::expressions::Expression> const& identifierToExpressionMap,
    bool const substituteTranscendentalNumbers);
}  // namespace jani

namespace expressions {
template<typename MapType>
class JaniExpressionSubstitutionVisitor : public SubstitutionVisitor<MapType>, public JaniExpressionVisitor {
   public:
    /*!
     * Creates a new substitution visitor that uses the given map to replace variables.
     *
     * @param variableToExpressionMapping A mapping from variables to expressions.
     * @param substituteTranscendentalNumbers Enables transcendental numbers substitution
     */
    JaniExpressionSubstitutionVisitor(MapType const& variableToExpressionMapping, bool const substituteTranscendentalNumbers);
    using SubstitutionVisitor<MapType>::visit;

    virtual boost::any visit(ValueArrayExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(ConstructorArrayExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(ArrayAccessExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(FunctionCallExpression const& expression, boost::any const& data) override;
    virtual boost::any visit(TranscendentalNumberLiteralExpression const& expression, boost::any const& data) override;

   protected:
    const bool shallSubstituteTranscendentalNumbers;
};
}  // namespace expressions
}  // namespace storm