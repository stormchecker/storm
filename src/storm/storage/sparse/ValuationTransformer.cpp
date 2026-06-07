#include "storm/storage/sparse/ValuationTransformer.h"

#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/NotSupportedException.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/storage/expressions/ExpressionManager.h"
#include "storm/storage/umb/model/Valuations.h"
#include "storm/storage/umb/utility/ValuationDescriptionBuilder.h"
#include "storm/utility/constants.h"

namespace storm::storage::sparse {

ValuationTransformer::ValuationTransformer(Valuations const& oldValuations) : oldValuations(oldValuations) {
    // Intentionally left empty.
}

void ValuationTransformer::addExpression(storm::expressions::Variable const& var, storm::expressions::Expression const& expr) {
    STORM_LOG_THROW(var.getType() == expr.getType(), storm::exceptions::InvalidArgumentException,
                    "Variable " << var.getName() << " and expression " << expr << " must have the same type.");
    STORM_LOG_THROW(var.getManager() == expr.getManager(), storm::exceptions::InvalidArgumentException,
                    "Variable " << var.getName() << " and expression " << expr << " must have the same manager.");
    STORM_LOG_THROW(var.getManager() == oldValuations.getManager(), storm::exceptions::InvalidArgumentException,
                    "Variable " << var.getName() << " and old state valuations must have the same manager.");
    STORM_LOG_THROW(var.hasBooleanType() || var.hasIntegerType() || var.hasRationalType(), storm::exceptions::InvalidArgumentException,
                    "Unsupported variable type " << var.getType() << " for variable " << var.getName() << ".");
    variables.push_back(var);
    expressions.push_back(expr);
}

Valuations ValuationTransformer::build(bool extend) {
    STORM_LOG_THROW(oldValuations.getUmbValuations().numClasses() == 1, storm::exceptions::NotSupportedException,
                    "Valuation transformation is only supported for valuations with a single class.");
    storm::umb::Valuations result = [&]() {
        storm::umb::ValuationDescriptionBuilder descriptionBuilder(oldValuations.getManager().shared_from_this());
        if (extend) {
            descriptionBuilder.addVariables(oldValuations.getUmbValuations().getClassDescription());
        }
        for (auto const& v : variables) {
            if (v.hasBooleanType()) {
                descriptionBuilder.addBooleanVariable(v);
            } else if (v.hasIntegerType()) {
                descriptionBuilder.addIntegerVariable(v, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max());
            } else if (v.hasRationalType()) {
                descriptionBuilder.addRationalVariable(v, 128);
            } else {
                STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                                "Variable " << v.getName() << " has unsupported type " << v.getType() << ".");
            }
        }
        return storm::umb::Valuations(descriptionBuilder.buildClassDescription(), oldValuations.getManager().shared_from_this());
    }();
    result.resize(oldValuations.getNumberOfEntities());

    storm::expressions::ExpressionEvaluator<storm::RationalNumber> evaluator(oldValuations.getManager());
    for (uint64_t entity = 0; entity < oldValuations.getNumberOfEntities(); ++entity) {
        // Copy variables into the new state valuations and setup the expression evaluator for the current state.
        oldValuations.getUmbValuations().readCallback(entity, [&result, extend, &evaluator](auto const e, auto const& var, auto const& value) {
            using ValueType = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<ValueType, bool>) {
                evaluator.setBooleanValue(var, value);
            } else if constexpr (std::is_same_v<ValueType, int64_t> || std::is_same_v<ValueType, uint64_t>) {
                evaluator.setIntegerValue(var, value);
            } else if constexpr (std::is_same_v<ValueType, double> || std::is_same_v<ValueType, storm::RationalNumber>) {
                evaluator.setRationalValue(var, value);
            }
            if (extend) {
                result.writeValue(e, var, value);
            }
        });
        for (uint64_t i = 0; i < variables.size(); ++i) {
            auto const& var = variables[i];
            auto const& expr = expressions[i];
            if (var.hasBooleanType()) {
                result.writeValue(entity, var, evaluator.asBool(expr));
            } else if (var.hasIntegerType()) {
                result.writeValue(entity, var, evaluator.asInt(expr));
            } else if (var.hasRationalType()) {
                result.writeValue(entity, var, evaluator.asRational(expr));
            } else {
                STORM_LOG_THROW(false, storm::exceptions::InvalidArgumentException,
                                "Variable " << var.getName() << " has unsupported type " << var.getType() << ".");
            }
        }
    }
    return Valuations(std::move(result));
}
}  // namespace storm::storage::sparse