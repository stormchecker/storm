#pragma once

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/logic/FormulaVisitor.h"
#include "storm/storage/expressions/ExpressionVisitor.h"
#include "storm/storage/jani/Model.h"
#include "storm/storage/jani/Property.h"
#include "storm/storage/jani/visitor/JaniExpressionVisitor.h"

namespace storm {
namespace jani {

typedef storm::json<storm::RationalNumber> ExportJsonType;

class ExpressionToJson : public storm::expressions::ExpressionVisitor, public storm::expressions::JaniExpressionVisitor {
   public:
    using storm::expressions::ExpressionVisitor::visit;
    static ExportJsonType translate(storm::expressions::Expression const& expr, std::vector<storm::jani::Constant> const& constants,
                                    VariableSet const& globalVariables, VariableSet const& localVariables,
                                    std::unordered_set<std::string> const& auxiliaryVariables);

    virtual boost::any visit(storm::expressions::IfThenElseExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::BinaryBooleanFunctionExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::BinaryNumericalFunctionExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::BinaryRelationExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::VariableExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::UnaryBooleanFunctionExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::UnaryNumericalFunctionExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::BooleanLiteralExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::IntegerLiteralExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::RationalLiteralExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::ValueArrayExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::ConstructorArrayExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::ArrayAccessExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::FunctionCallExpression const& expression, boost::any const& data);
    virtual boost::any visit(storm::expressions::TranscendentalNumberLiteralExpression const& expression, boost::any const& data);

   private:
    ExpressionToJson(std::vector<storm::jani::Constant> const& constants, VariableSet const& globalVariables, VariableSet const& localVariables,
                     std::unordered_set<std::string> const& auxiliaryVariables)
        : constants(constants), globalVariables(globalVariables), localVariables(localVariables), auxiliaryVariables(auxiliaryVariables) {}
    std::vector<storm::jani::Constant> const& constants;
    VariableSet const& globalVariables;
    VariableSet const& localVariables;
    std::unordered_set<std::string> auxiliaryVariables;
};

class FormulaToJaniJson : public storm::logic::FormulaVisitor {
   public:
    static ExportJsonType translate(storm::logic::Formula const& formula, storm::jani::Model const& model, storm::jani::ModelFeatures& modelFeatures);
    bool containsStateExitRewards() const;          // Returns true iff the  previously translated formula contained state exit rewards
    bool containsMultiObjectiveProperties() const;  // Returns true iff the  previously translated formula contained a multi-objective property
    virtual boost::any visit(storm::logic::AtomicExpressionFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::AtomicLabelFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::BinaryBooleanStateFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::BinaryBooleanPathFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::BooleanLiteralFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::BoundedUntilFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::ConditionalFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::CumulativeRewardFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::EventuallyFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::TimeOperatorFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::GameFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::GloballyFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::InstantaneousRewardFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::LongRunAverageOperatorFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::LongRunAverageRewardFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::MultiObjectiveFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::QuantileFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::NextFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::ProbabilityOperatorFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::RewardOperatorFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::TotalRewardFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::UnaryBooleanStateFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::UnaryBooleanPathFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::UntilFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::HOAPathFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::DiscountedCumulativeRewardFormula const& f, boost::any const& data) const;
    virtual boost::any visit(storm::logic::DiscountedTotalRewardFormula const& f, boost::any const& data) const;

   private:
    FormulaToJaniJson(storm::jani::Model const& model) : model(model), stateExitRewards(false), multiObjectiveProperties(false) {}

    ExportJsonType constructPropertyInterval(boost::optional<storm::expressions::Expression> const& lower, boost::optional<bool> const& lowerExclusive,
                                             boost::optional<storm::expressions::Expression> const& upper, boost::optional<bool> const& upperExclusive) const;

    ExportJsonType constructRewardAccumulation(storm::logic::RewardAccumulation const& rewardAccumulation) const;
    ExportJsonType constructRewardAccumulation(storm::logic::RewardAccumulation const& rewardAccumulation, std::string const& rewardModelName) const;
    ExportJsonType constructStandardRewardAccumulation(std::string const& rewardModelName) const;

    storm::jani::Model const& model;
    mutable bool stateExitRewards;
    mutable bool multiObjectiveProperties;
};

class JsonExporter {
   public:
    JsonExporter() = default;
    static void toFile(storm::jani::Model const& janiModel, std::vector<storm::jani::Property> const& formulas, std::string const& filepath,
                       bool checkValid = true, bool compact = false);
    static void toStream(storm::jani::Model const& janiModel, std::vector<storm::jani::Property> const& formulas, std::ostream& ostream,
                         bool checkValid = false, bool compact = false);

    static ExportJsonType getEdgeAsJson(storm::jani::Model const& janiModel, uint64_t automatonIndex, uint64_t edgeIndex, bool commentExpressions = true);

   private:
    void convertModel(storm::jani::Model const& model, bool commentExpressions = true);
    void convertProperties(std::vector<storm::jani::Property> const& formulas, storm::jani::Model const& model);

    ExportJsonType finalize();

    ExportJsonType jsonStruct;
    storm::jani::ModelFeatures modelFeatures;
};
}  // namespace jani
}  // namespace storm
