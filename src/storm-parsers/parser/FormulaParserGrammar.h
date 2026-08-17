#pragma once

#include <boost/variant.hpp>
#include <fstream>
#include <memory>

#include "storm-parsers/parser/ConstantDataType.h"
#include "storm-parsers/parser/ExpressionParser.h"
#include "storm-parsers/parser/SpiritErrorHandler.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/results/FilterType.h"
#include "storm/storage/expressions/ExpressionEvaluator.h"
#include "storm/storage/jani/Property.h"

namespace storm {
namespace logic {
class Formula;
}

namespace parser {

class FormulaParserGrammar : public qi::grammar<Iterator, std::vector<storm::jani::Property>(), Skipper> {
   public:
    FormulaParserGrammar(std::shared_ptr<storm::expressions::ExpressionManager const> const& manager);
    FormulaParserGrammar(std::shared_ptr<storm::expressions::ExpressionManager> const& manager);

    FormulaParserGrammar(FormulaParserGrammar const& other) = delete;
    FormulaParserGrammar& operator=(FormulaParserGrammar const& other) = delete;

    /*!
     * Adds an identifier and the expression it is supposed to be replaced with. This can, for example be used
     * to substitute special identifiers in the formula by expressions.
     *
     * @param identifier The identifier that is supposed to be substituted.
     * @param expression The expression it is to be substituted with.
     */
    void addIdentifierExpression(std::string const& identifier, storm::expressions::Expression const& expression);

    qi::symbols<char, storm::expressions::Expression> const& getIdentifiers() const;

   private:
    void initialize();

    struct keywordsStruct : qi::symbols<char, uint_fast64_t> {
        keywordsStruct() {
            add("true", 1)("false", 2)("min", 3)("max", 4)("F", 5)("G", 6)("X", 7)("U", 8)("C", 9)("I", 10)("P", 11)("R", 12)("S", 13);
        }
    };
    // A parser used for recognizing the standard keywords (that also apply to e.g. PRISM). These shall not coincide with expression variables
    keywordsStruct keywords_;

    struct nonStandardKeywordsStruct : qi::symbols<char, uint_fast64_t> {
        nonStandardKeywordsStruct() {
            add("T", 1)("LRA", 2)("MP", 3)("multi", 4)("quantile", 5)("HOA", 6);
        }
    };
    // A parser used for recognizing non-standard Storm-specific keywords.
    // For compatibility, we still try to parse expression variables whose identifier is such a keyword and just issue a warning.
    nonStandardKeywordsStruct nonStandardKeywords_;

    struct relationalOperatorStruct : qi::symbols<char, storm::logic::ComparisonType> {
        relationalOperatorStruct() {
            add(">=", storm::logic::ComparisonType::GreaterEqual)(">", storm::logic::ComparisonType::Greater)("<=", storm::logic::ComparisonType::LessEqual)(
                "<", storm::logic::ComparisonType::Less);
        }
    };

    // A parser used for recognizing the operators at the "relational" precedence level.
    relationalOperatorStruct relationalOperator_;

    struct optimalityOperatorStruct : qi::symbols<char, storm::OptimizationDirection> {
        optimalityOperatorStruct() {
            add("min", storm::OptimizationDirection::Minimize)("max", storm::OptimizationDirection::Maximize);
        }
    };

    // A parser used for recognizing the optimality operators.
    optimalityOperatorStruct optimalityOperator_;

    struct filterTypeStruct : qi::symbols<char, storm::modelchecker::FilterType> {
        filterTypeStruct() {
            add("min", storm::modelchecker::FilterType::MIN)("max", storm::modelchecker::FilterType::MAX)("sum", storm::modelchecker::FilterType::SUM)(
                "avg", storm::modelchecker::FilterType::AVG)("count", storm::modelchecker::FilterType::COUNT)(
                "forall", storm::modelchecker::FilterType::FORALL)("exists", storm::modelchecker::FilterType::EXISTS)(
                "argmin", storm::modelchecker::FilterType::ARGMIN)("argmax", storm::modelchecker::FilterType::ARGMAX)("values",
                                                                                                                      storm::modelchecker::FilterType::VALUES);
        }
    };

    // A parser used for recognizing the filter type.
    filterTypeStruct filterType_;

    struct operatorKeyword : qi::symbols<char, storm::logic::FormulaContext> {
        operatorKeyword() {
            add("P", storm::logic::FormulaContext::Probability)("R", storm::logic::FormulaContext::Reward)("T", storm::logic::FormulaContext::Time)(
                "LRA", storm::logic::FormulaContext::LongRunAverage)("S", storm::logic::FormulaContext::LongRunAverage);
        }
    };
    operatorKeyword operatorKeyword_;

    enum class FormulaKind {
        State,  /// PCTL*-like (boolean) state formula
        Path,   /// PCTL*-like (boolean) path formula (include state formulae)
    };
    qi::rule<Iterator, qi::unused_type(FormulaKind), Skipper> isPathFormula;

    // The manager used to parse expressions.
    std::shared_ptr<storm::expressions::ExpressionManager const> constManager;
    std::shared_ptr<storm::expressions::ExpressionManager> manager;

    // Parser and manager used for recognizing expressions.
    storm::parser::ExpressionParser expressionParser;

    // A symbol table that is a mapping from identifiers that can be used in expressions to the expressions
    // they are to be replaced with.
    qi::symbols<char, storm::expressions::Expression> identifiers_;

    // Rules

    // Auxiliary helpers
    qi::rule<Iterator, qi::unused_type(std::shared_ptr<storm::logic::Formula const>, std::string), Skipper> noAmbiguousNonAssociativeOperator;
    qi::rule<Iterator, std::string(), Skipper> identifier;
    qi::rule<Iterator, std::string(), Skipper> label;
    qi::rule<Iterator, std::string(), Skipper> quotedString;

    // PCTL-like Operator Formulas
    qi::rule<Iterator, storm::logic::OperatorInformation(), qi::locals<boost::optional<storm::OptimizationDirection>>, Skipper> operatorInformation;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> operatorSubFormula;
    qi::rule<Iterator, std::string(storm::logic::FormulaContext), Skipper> rewardModelName;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), qi::locals<storm::logic::FormulaContext>, Skipper> operatorFormula;

    // Atomic propositions
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> labelFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> expressionFormula;

    // Propositional logic operators
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(FormulaKind, storm::logic::FormulaContext), Skipper> basicPropositionalFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(FormulaKind, storm::logic::FormulaContext), Skipper> negationPropositionalFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(FormulaKind, storm::logic::FormulaContext), Skipper> andLevelPropositionalFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(FormulaKind, storm::logic::FormulaContext), Skipper> orLevelPropositionalFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(FormulaKind, storm::logic::FormulaContext), Skipper> propositionalFormula;

    // Path operators
    qi::rule<Iterator, std::shared_ptr<storm::logic::TimeBoundReference>, Skipper> timeBoundReference;
    qi::rule<
        Iterator,
        std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>, std::shared_ptr<storm::logic::TimeBoundReference>>(),
        qi::locals<bool, bool>, Skipper>
        timeBound;
    qi::rule<Iterator,
             std::vector<std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>,
                                    std::shared_ptr<storm::logic::TimeBoundReference>>>(),
             qi::locals<bool, bool>, Skipper>
        timeBounds;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> eventuallyFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> nextFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> globallyFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> hoaPathFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> multiBoundedPathFormulaOperand;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> multiBoundedPathFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> prefixOperatorPathFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> basicPathFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> untilLevelPathFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(storm::logic::FormulaContext), Skipper> pathFormula;

    // Quantitative path operators (reward)
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> longRunAverageRewardFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> instantaneousRewardFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> cumulativeRewardFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> totalRewardFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> discountedCumulativeRewardFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> discountedTotalRewardFormula;

    // Game Formulae
    qi::rule<Iterator, storm::logic::PlayerCoalition(), qi::locals<std::vector<std::variant<std::string, storm::storage::PlayerIndex>>>, Skipper>
        playerCoalition;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> gameFormula;

    // Multi-objective, quantiles
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> multiOperatorFormula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> multiLexOperatorFormula;
    qi::rule<Iterator, storm::expressions::Variable(), Skipper> quantileBoundVariable;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> quantileFormula;

    // General formulae
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(FormulaKind, storm::logic::FormulaContext), Skipper> formula;
    qi::rule<Iterator, std::shared_ptr<storm::logic::Formula const>(), Skipper> topLevelFormula;

    // Properties
    qi::rule<Iterator, std::string(), Skipper> formulaName;
    qi::rule<Iterator, storm::jani::Property(), Skipper> filterProperty;

    qi::rule<Iterator, qi::unused_type(), qi::locals<ConstantDataType>, Skipper> constantDefinition;

    // Start symbol
    qi::rule<Iterator, std::vector<storm::jani::Property>(), Skipper> start;

    void addHoaAPMapping(storm::logic::Formula const& hoaFormula, const std::string& ap, std::shared_ptr<storm::logic::Formula const>& expression) const;

    storm::logic::PlayerCoalition createPlayerCoalition(std::vector<std::variant<std::string, storm::storage::PlayerIndex>> const& playerIds) const;
    std::shared_ptr<storm::logic::Formula const> createGameFormula(storm::logic::PlayerCoalition const& coalition,
                                                                   std::shared_ptr<storm::logic::Formula const> const& subformula) const;

    bool areConstantDefinitionsAllowed() const;
    void addConstant(std::string const& name, ConstantDataType type, boost::optional<storm::expressions::Expression> const& expression);

    std::shared_ptr<storm::logic::TimeBoundReference> createTimeBoundReference(storm::logic::TimeBoundType const& type,
                                                                               boost::optional<std::string> const& rewardModelName) const;
    std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>, std::shared_ptr<storm::logic::TimeBoundReference>>
    createTimeBoundFromInterval(storm::expressions::Expression const& lowerBound, storm::expressions::Expression const& upperBound,
                                std::shared_ptr<storm::logic::TimeBoundReference> const& timeBoundReference) const;
    std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>, std::shared_ptr<storm::logic::TimeBoundReference>>
    createTimeBoundFromSingleBound(storm::expressions::Expression const& bound, bool upperBound, bool strict,
                                   std::shared_ptr<storm::logic::TimeBoundReference> const& timeBoundReference) const;

    // Methods that actually create the expression objects.
    std::shared_ptr<storm::logic::Formula const> createInstantaneousRewardFormula(storm::expressions::Expression const& timeBound) const;
    std::shared_ptr<storm::logic::Formula const> createCumulativeRewardFormula(
        std::vector<std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>,
                               std::shared_ptr<storm::logic::TimeBoundReference>>> const& timeBounds) const;
    std::shared_ptr<storm::logic::Formula const> createDiscountedCumulativeRewardFormula(
        storm::expressions::Expression const& discountFactor,
        std::vector<std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>,
                               std::shared_ptr<storm::logic::TimeBoundReference>>> const& timeBounds) const;
    std::shared_ptr<storm::logic::Formula const> createTotalRewardFormula() const;
    std::shared_ptr<storm::logic::Formula const> createDiscountedTotalRewardFormula(storm::expressions::Expression const& discountFactor) const;
    std::shared_ptr<storm::logic::Formula const> createLongRunAverageRewardFormula() const;
    std::shared_ptr<storm::logic::Formula const> createAtomicExpressionFormula(storm::expressions::Expression const& expression) const;
    std::shared_ptr<storm::logic::Formula const> createBooleanLiteralFormula(bool literal) const;
    std::shared_ptr<storm::logic::Formula const> createAtomicLabelFormula(std::string const& label) const;
    std::shared_ptr<storm::logic::Formula const> createEventuallyFormula(
        boost::optional<std::vector<std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>,
                                               std::shared_ptr<storm::logic::TimeBoundReference>>>> const& timeBounds,
        storm::logic::FormulaContext context, std::shared_ptr<storm::logic::Formula const> const& subformula) const;
    std::shared_ptr<storm::logic::Formula const> createGloballyFormula(std::shared_ptr<storm::logic::Formula const> const& subformula) const;
    std::shared_ptr<storm::logic::Formula const> createNextFormula(std::shared_ptr<storm::logic::Formula const> const& subformula) const;
    std::shared_ptr<storm::logic::Formula const> createUntilFormula(
        std::shared_ptr<storm::logic::Formula const> const& leftSubformula,
        boost::optional<std::vector<std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>,
                                               std::shared_ptr<storm::logic::TimeBoundReference>>>> const& timeBounds,
        std::shared_ptr<storm::logic::Formula const> const& rightSubformula);
    std::shared_ptr<storm::logic::Formula const> createHOAPathFormula(const std::string& automataFile) const;
    std::shared_ptr<storm::logic::Formula const> createConditionalFormula(std::shared_ptr<storm::logic::Formula const> const& leftSubformula,
                                                                          boost::optional<std::shared_ptr<storm::logic::Formula const>> const& rightSubformula,
                                                                          storm::logic::FormulaContext context) const;
    storm::logic::OperatorInformation createOperatorInformation(boost::optional<storm::OptimizationDirection> const& optimizationDirection,
                                                                boost::optional<storm::logic::ComparisonType> const& comparisonType,
                                                                boost::optional<storm::expressions::Expression> const& threshold) const;
    std::shared_ptr<storm::logic::Formula const> createOperatorFormula(storm::logic::FormulaContext const& context,
                                                                       boost::optional<std::string> const& rewardModelName,
                                                                       storm::logic::OperatorInformation const& operatorInformation,
                                                                       std::shared_ptr<storm::logic::Formula const> const& subformula);
    std::shared_ptr<storm::logic::Formula const> createLongRunAverageOperatorFormula(storm::logic::OperatorInformation const& operatorInformation,
                                                                                     std::shared_ptr<storm::logic::Formula const> const& subformula) const;
    std::shared_ptr<storm::logic::Formula const> createRewardOperatorFormula(boost::optional<std::string> const& rewardModelName,
                                                                             storm::logic::OperatorInformation const& operatorInformation,
                                                                             std::shared_ptr<storm::logic::Formula const> const& subformula) const;
    std::shared_ptr<storm::logic::Formula const> createTimeOperatorFormula(storm::logic::OperatorInformation const& operatorInformation,
                                                                           std::shared_ptr<storm::logic::Formula const> const& subformula) const;
    std::shared_ptr<storm::logic::Formula const> createProbabilityOperatorFormula(storm::logic::OperatorInformation const& operatorInformation,
                                                                                  std::shared_ptr<storm::logic::Formula const> const& subformula);
    std::shared_ptr<storm::logic::Formula const> createBinaryBooleanStateFormula(std::shared_ptr<storm::logic::Formula const> const& leftSubformula,
                                                                                 std::shared_ptr<storm::logic::Formula const> const& rightSubformula,
                                                                                 storm::logic::BinaryBooleanStateFormula::OperatorType operatorType);
    std::shared_ptr<storm::logic::Formula const> createBinaryBooleanPathFormula(std::shared_ptr<storm::logic::Formula const> const& leftSubformula,
                                                                                std::shared_ptr<storm::logic::Formula const> const& rightSubformula,
                                                                                storm::logic::BinaryBooleanPathFormula::OperatorType operatorType);
    std::shared_ptr<storm::logic::Formula const> createBinaryBooleanStateOrPathFormula(std::shared_ptr<storm::logic::Formula const> const& leftSubformula,
                                                                                       std::shared_ptr<storm::logic::Formula const> const& rightSubformula,
                                                                                       storm::logic::BinaryBooleanOperatorType operatorType);
    std::shared_ptr<storm::logic::Formula const> createUnaryBooleanStateFormula(
        std::shared_ptr<storm::logic::Formula const> const& subformula,
        boost::optional<storm::logic::UnaryBooleanStateFormula::OperatorType> const& operatorType);
    std::shared_ptr<storm::logic::Formula const> createUnaryBooleanPathFormula(
        std::shared_ptr<storm::logic::Formula const> const& subformula,
        boost::optional<storm::logic::UnaryBooleanPathFormula::OperatorType> const& operatorType);
    std::shared_ptr<storm::logic::Formula const> createUnaryBooleanStateOrPathFormula(
        std::shared_ptr<storm::logic::Formula const> const& subformula, boost::optional<storm::logic::UnaryBooleanOperatorType> const& operatorType);
    bool isValidMultiBoundedPathFormulaOperand(std::shared_ptr<storm::logic::Formula const> const& operand);
    std::shared_ptr<storm::logic::Formula const> createMultiBoundedPathFormula(std::vector<std::shared_ptr<storm::logic::Formula const>> const& subformulas);
    std::shared_ptr<storm::logic::Formula const> createMultiOperatorFormula(std::vector<std::shared_ptr<storm::logic::Formula const>> const& subformulas,
                                                                            storm::logic::MultiObjectiveFormula::Type type);
    storm::expressions::Variable createQuantileBoundVariables(boost::optional<storm::solver::OptimizationDirection> const& dir,
                                                              std::string const& variableName);
    std::shared_ptr<storm::logic::Formula const> createQuantileFormula(std::vector<storm::expressions::Variable> const& boundVariables,
                                                                       std::shared_ptr<storm::logic::Formula const> const& subformula);

    std::set<storm::expressions::Variable> getUndefinedConstants(std::shared_ptr<storm::logic::Formula const> const& formula) const;
    storm::jani::Property createProperty(boost::optional<std::string> const& propertyName, storm::modelchecker::FilterType const& filterType,
                                         std::shared_ptr<storm::logic::Formula const> const& formula,
                                         std::shared_ptr<storm::logic::Formula const> const& states);
    storm::jani::Property createPropertyWithDefaultFilterTypeAndStates(boost::optional<std::string> const& propertyName,
                                                                       std::shared_ptr<storm::logic::Formula const> const& formula);

    bool isBooleanReturnType(std::shared_ptr<storm::logic::Formula const> const& formula, bool raiseErrorMessage = false);
    bool raiseAmbiguousNonAssociativeOperatorError(std::shared_ptr<storm::logic::Formula const> const& formula, std::string const& op);

    // An error handler function.
    phoenix::function<SpiritErrorHandler> handler;

    uint64_t propertyCount;

    std::set<storm::expressions::Variable> undefinedConstants;
    std::set<storm::expressions::Variable> quantileFormulaVariables;
};

}  // namespace parser
}  // namespace storm
