#include "FormulaParserGrammar.h"

#include <memory>

#include "storm/storage/expressions/ExpressionManager.h"

namespace storm {
namespace parser {

FormulaParserGrammar::FormulaParserGrammar(std::shared_ptr<storm::expressions::ExpressionManager const> const& manager)
    : FormulaParserGrammar::base_type(start), constManager(manager), manager(nullptr), expressionParser(*manager, keywords_, true, true), propertyCount(0) {
    initialize();
}

FormulaParserGrammar::FormulaParserGrammar(std::shared_ptr<storm::expressions::ExpressionManager> const& manager)
    : FormulaParserGrammar::base_type(start), constManager(manager), manager(manager), expressionParser(*manager, keywords_, true, true), propertyCount(0) {
    initialize();
}

qi::symbols<char, storm::expressions::Expression> const& FormulaParserGrammar::getIdentifiers() const {
    return identifiers_;
}

void FormulaParserGrammar::initialize() {
    // Register all variables so we can parse them in the expressions.
    for (auto variableTypePair : *constManager) {
        addIdentifierExpression(variableTypePair.first.getName(), variableTypePair.first);
    }
    // Set the identifier mapping to actually generate expressions.
    expressionParser.setIdentifierMapping(&identifiers_);

    keywords_.name("keyword");
    nonStandardKeywords_.name("non-standard Storm-specific keyword");
    relationalOperator_.name("relational operator");
    optimalityOperator_.name("optimality operator");
    operatorKeyword_.name("Operator keyword");
    filterType_.name("filter type");

    // Auxiliary helpers
    isPathFormula = qi::eps(qi::_r1 == FormulaKind::Path);
    noAmbiguousNonAssociativeOperator =
        !(qi::lit(qi::_r2)[qi::_pass = phoenix::bind(&FormulaParserGrammar::raiseAmbiguousNonAssociativeOperatorError, phoenix::ref(*this), qi::_r1, qi::_r2)]);
    noAmbiguousNonAssociativeOperator.name("no ambiguous non-associative operator");
    identifier %= qi::as_string[qi::raw[qi::lexeme[((qi::alpha | qi::char_('_') | qi::char_('.')) >> *(qi::alnum | qi::char_('_')))]]];
    identifier.name("identifier");
    label %= qi::as_string[qi::raw[qi::lexeme[((qi::alpha | qi::char_('_')) >> *(qi::alnum | qi::char_('_')))]]];
    label.name("label");
    quotedString %= qi::as_string[qi::lexeme[qi::omit[qi::char_('"')] > qi::raw[*(!qi::char_('"') >> qi::char_)] > qi::omit[qi::lit('"')]]];
    quotedString.name("quoted string");

    // PCTL-like Operator Formulas
    operatorInformation =
        (-optimalityOperator_)[qi::_a = qi::_1] >>
        ((qi::lit("=") >
          qi::lit("?"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createOperatorInformation, phoenix::ref(*this), qi::_a, boost::none, boost::none)] |
         (relationalOperator_ >
          expressionParser)[qi::_val = phoenix::bind(&FormulaParserGrammar::createOperatorInformation, phoenix::ref(*this), qi::_a, qi::_1, qi::_2)]);
    operatorInformation.name("operator information");
    operatorSubFormula =
        (((qi::eps(qi::_r1 == storm::logic::FormulaContext::Probability) > formula(FormulaKind::Path, qi::_r1)) |
          (qi::eps(qi::_r1 == storm::logic::FormulaContext::Reward) >
           (longRunAverageRewardFormula | eventuallyFormula(qi::_r1) | discountedCumulativeRewardFormula | discountedTotalRewardFormula |
            cumulativeRewardFormula | instantaneousRewardFormula | totalRewardFormula)) |
          (qi::eps(qi::_r1 == storm::logic::FormulaContext::Time) > eventuallyFormula(qi::_r1)) |
          (qi::eps(qi::_r1 == storm::logic::FormulaContext::LongRunAverage) > formula(FormulaKind::State, storm::logic::FormulaContext::LongRunAverage))) >>
         -(qi::lit("||") >
           formula(FormulaKind::Path, storm::logic::FormulaContext::Probability)))[qi::_val = phoenix::bind(&FormulaParserGrammar::createConditionalFormula,
                                                                                                            phoenix::ref(*this), qi::_1, qi::_2, qi::_r1)];
    operatorSubFormula.name("operator subformula");
    rewardModelName = qi::eps(qi::_r1 == storm::logic::FormulaContext::Reward) >> (qi::lit("{\"") > label > qi::lit("\"}"));
    rewardModelName.name("reward model name");
    operatorFormula =
        (operatorKeyword_[qi::_a = qi::_1] > -rewardModelName(qi::_a) > operatorInformation > qi::lit("[") > operatorSubFormula(qi::_a) >
         qi::lit("]"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createOperatorFormula, phoenix::ref(*this), qi::_a, qi::_2, qi::_3, qi::_4)];
    operatorFormula.name("operator formula");

    // Atomic propositions
    labelFormula =
        (qi::lit("\"") >> label >> qi::lit("\""))[qi::_val = phoenix::bind(&FormulaParserGrammar::createAtomicLabelFormula, phoenix::ref(*this), qi::_1)];
    labelFormula.name("label formula");
    expressionFormula = expressionParser[qi::_val = phoenix::bind(&FormulaParserGrammar::createAtomicExpressionFormula, phoenix::ref(*this), qi::_1)];
    expressionFormula.name("expression formula");

    basicPropositionalFormula =
        expressionFormula  // try as expression first, e.g. '(false)=false' is an atomic expression formula. Also should be checked before operator formulas and
                           // others. Otherwise, e.g. variable "Random" would be parsed as reward operator 'R' (followed by andom)
        | (qi::lit("(") >>
           (formula(qi::_r1, qi::_r2) > qi::lit(")")))  // If we're starting with '(' but this is not an expression, we must have a formula inside the brackets
        | labelFormula | negationPropositionalFormula(qi::_r1, qi::_r2) | operatorFormula |
        (isPathFormula(qi::_r1) >> prefixOperatorPathFormula(qi::_r2))  // Needed for e.g. F "a" & X "a" = F ("a" & (X "a"))
        | multiLexOperatorFormula  // Has to come after prefixOperatorPathFormula to avoid confusion with multiBoundedPathFormula
        | multiOperatorFormula     // Has to come after multilex to avoid failing to parse multilex
        | quantileFormula | gameFormula;

    // Propositional Logic operators
    // To correctly parse the operator precedences (! binds stronger than & binds stronger than |), we run through different "precedence levels" starting with
    // the strongest binding operator.
    negationPropositionalFormula =
        (qi::lit("!") >
         basicPropositionalFormula(qi::_r1, qi::_r2))[qi::_val = phoenix::bind(&FormulaParserGrammar::createUnaryBooleanStateOrPathFormula, phoenix::ref(*this),
                                                                               qi::_1, storm::logic::UnaryBooleanOperatorType::Not)];
    basicPropositionalFormula.name("basic propositional formula");
    andLevelPropositionalFormula =
        basicPropositionalFormula(qi::_r1, qi::_r2)[qi::_val = qi::_1] >>
        *(qi::lit("&") > basicPropositionalFormula(
                             qi::_r1, qi::_r2)[qi::_val = phoenix::bind(&FormulaParserGrammar::createBinaryBooleanStateOrPathFormula, phoenix::ref(*this),
                                                                        qi::_val, qi::_1, storm::logic::BinaryBooleanStateFormula::OperatorType::And)]);
    andLevelPropositionalFormula.name("and precedence level propositional formula");
    orLevelPropositionalFormula =
        andLevelPropositionalFormula(qi::_r1, qi::_r2)[qi::_val = qi::_1] >>
        *((!qi::lit("||") >> qi::lit("|"))  // Make sure to not confuse with conditional operator "||"
          > andLevelPropositionalFormula(
                qi::_r1, qi::_r2)[qi::_val = phoenix::bind(&FormulaParserGrammar::createBinaryBooleanStateOrPathFormula, phoenix::ref(*this), qi::_val, qi::_1,
                                                           storm::logic::BinaryBooleanStateFormula::OperatorType::Or)]);
    orLevelPropositionalFormula.name("or precedence level propositional formula");
    propositionalFormula = orLevelPropositionalFormula(qi::_r1, qi::_r2);

    // Path operators
    // Again need to parse precedences correctly. Propositional formulae bind stronger than temporal operators.
    basicPathFormula = propositionalFormula(FormulaKind::Path, qi::_r1)  // Bracketed case is handled here as well
                       | prefixOperatorPathFormula(
                             qi::_r1);  // Needs to be checked *after* atomic expression formulas. Otherwise e.g. variable Fail would be parsed as "F (ail)"
    prefixOperatorPathFormula =
        eventuallyFormula(qi::_r1) | nextFormula(qi::_r1) | globallyFormula(qi::_r1) | hoaPathFormula(qi::_r1) | multiBoundedPathFormula(qi::_r1);
    basicPathFormula.name("basic path formula");
    timeBoundReference =
        (-qi::lit("rew") >>
         rewardModelName(storm::logic::FormulaContext::Reward))[qi::_val = phoenix::bind(&FormulaParserGrammar::createTimeBoundReference, phoenix::ref(*this),
                                                                                         storm::logic::TimeBoundType::Reward, qi::_1)] |
        (qi::lit("rew") >>
         -rewardModelName(storm::logic::FormulaContext::Reward))[qi::_val = phoenix::bind(&FormulaParserGrammar::createTimeBoundReference, phoenix::ref(*this),
                                                                                          storm::logic::TimeBoundType::Reward, qi::_1)] |
        (qi::lit("steps"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createTimeBoundReference, phoenix::ref(*this), storm::logic::TimeBoundType::Steps,
                                                    boost::none)] |
        (-qi::lit("time"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createTimeBoundReference, phoenix::ref(*this), storm::logic::TimeBoundType::Time,
                                                    boost::none)];
    timeBoundReference.name("time bound reference");
    timeBound = ((timeBoundReference >> qi::lit("[")) > expressionParser > qi::lit(",") > expressionParser >
                 qi::lit("]"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createTimeBoundFromInterval, phoenix::ref(*this), qi::_2, qi::_3, qi::_1)] |
                (timeBoundReference >> (qi::lit("<=")[(qi::_a = true, qi::_b = false)] | qi::lit("<")[(qi::_a = true, qi::_b = true)] |
                                        qi::lit(">=")[(qi::_a = false, qi::_b = false)] | qi::lit(">")[(qi::_a = false, qi::_b = true)]) >>
                 expressionParser)[qi::_val = phoenix::bind(&FormulaParserGrammar::createTimeBoundFromSingleBound, phoenix::ref(*this), qi::_2, qi::_a, qi::_b,
                                                            qi::_1)] |
                (timeBoundReference >> qi::lit("=") >>
                 expressionParser)[qi::_val = phoenix::bind(&FormulaParserGrammar::createTimeBoundFromInterval, phoenix::ref(*this), qi::_2, qi::_2, qi::_1)];
    timeBound.name("time bound");
    timeBounds = (timeBound % qi::lit(",")) | (((-qi::lit("^") >> qi::lit("{")) >> (timeBound % qi::lit(","))) >> qi::lit("}"));
    timeBounds.name("time bounds");
    eventuallyFormula =
        (qi::lit("F") > (-timeBounds) >
         basicPathFormula(qi::_r1))[qi::_val = phoenix::bind(&FormulaParserGrammar::createEventuallyFormula, phoenix::ref(*this), qi::_1, qi::_r1, qi::_2)];
    eventuallyFormula.name("eventually formula");
    nextFormula = (qi::lit("X") > basicPathFormula(qi::_r1))[qi::_val = phoenix::bind(&FormulaParserGrammar::createNextFormula, phoenix::ref(*this), qi::_1)];
    nextFormula.name("next formula");
    globallyFormula =
        (qi::lit("G") > basicPathFormula(qi::_r1))[qi::_val = phoenix::bind(&FormulaParserGrammar::createGloballyFormula, phoenix::ref(*this), qi::_1)];
    globallyFormula.name("globally formula");
    hoaPathFormula =
        qi::lit("HOA:") > qi::lit("{") > quotedString[qi::_val = phoenix::bind(&FormulaParserGrammar::createHOAPathFormula, phoenix::ref(*this), qi::_1)] >>
        *(qi::lit(",") > quotedString > qi::lit("->") >
          formula(FormulaKind::State, qi::_r1))[phoenix::bind(&FormulaParserGrammar::addHoaAPMapping, phoenix::ref(*this), *qi::_val, qi::_1, qi::_2)] >
        qi::lit("}");
    multiBoundedPathFormulaOperand = pathFormula(
        qi::_r1)[qi::_pass = phoenix::bind(&FormulaParserGrammar::isValidMultiBoundedPathFormulaOperand, phoenix::ref(*this), qi::_1)][qi::_val = qi::_1];
    multiBoundedPathFormulaOperand.name("multi bounded path formula operand");
    multiBoundedPathFormula = ((qi::lit("multi") > qi::lit("(")) >> (multiBoundedPathFormulaOperand(qi::_r1) % qi::lit(",")) >>
                               qi::lit(")"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createMultiBoundedPathFormula, phoenix::ref(*this), qi::_1)];
    multiBoundedPathFormula.name("multi bounded path formula");
    untilLevelPathFormula =
        basicPathFormula(qi::_r1)[qi::_val = qi::_1] >>
        -((qi::lit("U") > (-timeBounds) >
           basicPathFormula(qi::_r1))[qi::_val = phoenix::bind(&FormulaParserGrammar::createUntilFormula, phoenix::ref(*this), qi::_val, qi::_1, qi::_2)]) >>
        (qi::eps > noAmbiguousNonAssociativeOperator(qi::_val, std::string("U")));  // Do not parse a U b U c
    untilLevelPathFormula.name("until precedence level path formula");
    pathFormula = untilLevelPathFormula(qi::_r1);
    pathFormula.name("path formula");

    // Quantitative path formulae (reward)
    discountedTotalRewardFormula =
        (qi::lit("Cdiscount=") >>
         expressionParser)[qi::_val = phoenix::bind(&FormulaParserGrammar::createDiscountedTotalRewardFormula, phoenix::ref(*this), qi::_1)];
    discountedTotalRewardFormula.name("discounted total reward formula");
    discountedCumulativeRewardFormula =
        (qi::lit("C") >> timeBounds >> qi::lit("discount=") >>
         expressionParser)[qi::_val = phoenix::bind(&FormulaParserGrammar::createDiscountedCumulativeRewardFormula, phoenix::ref(*this), qi::_2, qi::_1)];
    discountedCumulativeRewardFormula.name("discounted cumulative reward formula");
    longRunAverageRewardFormula = (qi::lit("LRA") | qi::lit("S") |
                                   qi::lit("MP"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createLongRunAverageRewardFormula, phoenix::ref(*this))];
    longRunAverageRewardFormula.name("long run average reward formula");
    instantaneousRewardFormula =
        (qi::lit("I=") > expressionParser)[qi::_val = phoenix::bind(&FormulaParserGrammar::createInstantaneousRewardFormula, phoenix::ref(*this), qi::_1)];
    instantaneousRewardFormula.name("instantaneous reward formula");
    cumulativeRewardFormula =
        (qi::lit("C") >> timeBounds)[qi::_val = phoenix::bind(&FormulaParserGrammar::createCumulativeRewardFormula, phoenix::ref(*this), qi::_1)];
    cumulativeRewardFormula.name("cumulative reward formula");
    totalRewardFormula = (qi::lit("C"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createTotalRewardFormula, phoenix::ref(*this))];
    totalRewardFormula.name("total reward formula");

    // Game Formulae
    playerCoalition = (-((identifier[phoenix::push_back(qi::_a, qi::_1)] | qi::uint_[phoenix::push_back(qi::_a, qi::_1)]) %
                         ','))[qi::_val = phoenix::bind(&FormulaParserGrammar::createPlayerCoalition, phoenix::ref(*this), qi::_a)];
    playerCoalition.name("player coalition");
    gameFormula = (qi::lit("<<") > playerCoalition > qi::lit(">>") >
                   operatorFormula)[qi::_val = phoenix::bind(&FormulaParserGrammar::createGameFormula, phoenix::ref(*this), qi::_1, qi::_2)];
    gameFormula.name("game formula");

    // Multi-objective, quantiles
    multiOperatorFormula = (qi::lit("multi") > qi::lit("(") > (operatorFormula % qi::lit(",")) >
                            qi::lit(")"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createMultiOperatorFormula, phoenix::ref(*this), qi::_1,
                                                                   storm::logic::MultiObjectiveFormula::Type::Tradeoff)];
    multiOperatorFormula.name("multi-objective operator formula");
    multiLexOperatorFormula = (qi::lit("multilex") > qi::lit("(") > (operatorFormula % qi::lit(",")) >
                               qi::lit(")"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createMultiOperatorFormula, phoenix::ref(*this), qi::_1,
                                                                      storm::logic::MultiObjectiveFormula::Type::Lexicographic)];
    multiLexOperatorFormula.name("multi-objective lexicographic operator formula");
    quantileBoundVariable = (-optimalityOperator_ >> identifier >>
                             qi::lit(","))[qi::_val = phoenix::bind(&FormulaParserGrammar::createQuantileBoundVariables, phoenix::ref(*this), qi::_1, qi::_2)];
    quantileBoundVariable.name("quantile bound variable");
    quantileFormula = (qi::lit("quantile") > qi::lit("(") > *(quantileBoundVariable) >
                       operatorFormula[qi::_pass = phoenix::bind(&FormulaParserGrammar::isBooleanReturnType, phoenix::ref(*this), qi::_1, true)] >
                       qi::lit(")"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createQuantileFormula, phoenix::ref(*this), qi::_1, qi::_2)];
    quantileFormula.name("Quantile formula");

    // General formulae
    formula = (isPathFormula(qi::_r1) >> pathFormula(qi::_r2) | propositionalFormula(qi::_r1, qi::_r2));
    formula.name("formula");

    topLevelFormula = formula(FormulaKind::State, storm::logic::FormulaContext::Undefined);
    topLevelFormula.name("top-level formula");

    formulaName = qi::lit("\"") >> identifier >> qi::lit("\"") >> qi::lit(":");
    formulaName.name("formula name");

    constantDefinition =
        (qi::lit("const") > -(qi::lit("int")[qi::_a = ConstantDataType::Integer] | qi::lit("bool")[qi::_a = ConstantDataType::Bool] |
                              qi::lit("double")[qi::_a = ConstantDataType::Rational]) >>
         identifier >> -(qi::lit("=") > expressionParser))[phoenix::bind(&FormulaParserGrammar::addConstant, phoenix::ref(*this), qi::_1, qi::_a, qi::_2)];
    constantDefinition.name("constant definition");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-shift-op-parentheses"

    filterProperty =
        (-formulaName >> qi::lit("filter") > qi::lit("(") > filterType_ > qi::lit(",") > topLevelFormula > qi::lit(",") >
         formula(FormulaKind::State, storm::logic::FormulaContext::Undefined) >
         qi::lit(")"))[qi::_val = phoenix::bind(&FormulaParserGrammar::createProperty, phoenix::ref(*this), qi::_1, qi::_2, qi::_3, qi::_4)] |
        (-formulaName >>
         topLevelFormula)[qi::_val = phoenix::bind(&FormulaParserGrammar::createPropertyWithDefaultFilterTypeAndStates, phoenix::ref(*this), qi::_1, qi::_2)];
    filterProperty.name("filter property");

#pragma clang diagnostic pop

    start = (qi::eps >> filterProperty[phoenix::push_back(qi::_val, qi::_1)] |
             qi::eps(phoenix::bind(&FormulaParserGrammar::areConstantDefinitionsAllowed, phoenix::ref(*this))) >> constantDefinition | qi::eps) %
                +(qi::char_("\n;")) >>
            qi::skip(storm::spirit_encoding::space_type() | qi::lit("//") >> *(qi::char_ - (qi::eol | qi::eoi)))[qi::eps] >> qi::eoi;
    start.name("start");

    // Enable the following lines to print debug output for most the rules.
    //            debug(rewardModelName)
    //            debug(operatorFormula)
    //            debug(labelFormula)
    //            debug(expressionFormula)
    //            debug(basicPropositionalFormula)
    //            debug(negationPropositionalFormula)
    //            debug(andLevelPropositionalFormula)
    //            debug(orLevelPropositionalFormula)
    //            debug(propositionalFormula)
    //            debug(timeBoundReference)
    //            debug(timeBound)
    //            debug(timeBounds)
    //            debug(eventuallyFormula)
    //            debug(nextFormula)
    //            debug(globallyFormula)
    //            debug(hoaPathFormula)
    //            debug(multiBoundedPathFormula)
    //            debug(prefixOperatorPathFormula)
    //            debug(basicPathFormula)
    //            debug(untilLevelPathFormula)
    //            debug(pathFormula)
    //            debug(longRunAverageRewardFormula)
    //            debug(instantaneousRewardFormula)
    //            debug(cumulativeRewardFormula)
    //            debug(totalRewardFormula)
    //            debug(playerCoalition)
    //            debug(gameFormula)
    //            debug(multiOperatorFormula)
    //            debug(multiLexOperatorFormula)
    //            debug(quantileBoundVariable)
    //            debug(quantileFormula)
    //            debug(formula)
    //            debug(topLevelFormula)
    //            debug(formulaName)
    //            debug(filterProperty)
    //            debug(constantDefinition )
    //            debug(start)
    //            debug(discountedCumulativeRewardFormula)
    //            debug(discountedTotalRewardFormula)

    // Enable error reporting.
    qi::on_error<qi::fail>(rewardModelName, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(operatorFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(labelFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(expressionFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(basicPropositionalFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(negationPropositionalFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(andLevelPropositionalFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(orLevelPropositionalFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(propositionalFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(timeBoundReference, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(timeBound, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(timeBounds, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(eventuallyFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(nextFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(globallyFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(hoaPathFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(multiBoundedPathFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(prefixOperatorPathFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(basicPathFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(untilLevelPathFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(pathFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(longRunAverageRewardFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(instantaneousRewardFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(cumulativeRewardFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(totalRewardFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(discountedCumulativeRewardFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(discountedTotalRewardFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(playerCoalition, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(gameFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(multiOperatorFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(multiLexOperatorFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(quantileBoundVariable, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(quantileFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(formula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(topLevelFormula, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(formulaName, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(filterProperty, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(constantDefinition, handler(qi::_1, qi::_2, qi::_3, qi::_4));
    qi::on_error<qi::fail>(start, handler(qi::_1, qi::_2, qi::_3, qi::_4));
}

void FormulaParserGrammar::addIdentifierExpression(std::string const& identifier, storm::expressions::Expression const& expression) {
    STORM_LOG_WARN_COND(keywords_.find(identifier) == nullptr,
                        "Identifier `" << identifier << "' coincides with a reserved keyword or operator. Property expressions using the variable or constant '"
                                       << identifier << "' will not be parsed correctly.");
    STORM_LOG_WARN_COND(nonStandardKeywords_.find(identifier) == nullptr,
                        "Identifier `" << identifier << "' coincides with a reserved keyword or operator. Property expressions using the variable or constant '"
                                       << identifier << "' might not be parsed correctly.");
    this->identifiers_.add(identifier, expression);
}

void FormulaParserGrammar::addConstant(std::string const& name, ConstantDataType type, boost::optional<storm::expressions::Expression> const& expression) {
    STORM_LOG_ASSERT(manager, "Mutable expression manager required to define new constants.");
    storm::expressions::Variable newVariable;
    STORM_LOG_THROW(!manager->hasVariable(name), storm::exceptions::WrongFormatException,
                    "Invalid constant definition '" << name << "' in property: variable already exists.");

    if (type == ConstantDataType::Bool) {
        newVariable = manager->declareBooleanVariable(name);
    } else if (type == ConstantDataType::Integer) {
        newVariable = manager->declareIntegerVariable(name);
    } else {
        newVariable = manager->declareRationalVariable(name);
    }

    if (expression) {
        addIdentifierExpression(name, expression.get());
    } else {
        undefinedConstants.insert(newVariable);
        addIdentifierExpression(name, newVariable);
    }
}

bool FormulaParserGrammar::areConstantDefinitionsAllowed() const {
    return static_cast<bool>(manager);
}

std::shared_ptr<storm::logic::TimeBoundReference> FormulaParserGrammar::createTimeBoundReference(storm::logic::TimeBoundType const& type,
                                                                                                 boost::optional<std::string> const& rewardModelName) const {
    if (type == storm::logic::TimeBoundType::Reward) {
        return std::make_shared<storm::logic::TimeBoundReference>(rewardModelName);
    } else {
        return std::make_shared<storm::logic::TimeBoundReference>(type);
    }
}

std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>, std::shared_ptr<storm::logic::TimeBoundReference>>
FormulaParserGrammar::createTimeBoundFromInterval(storm::expressions::Expression const& lowerBound, storm::expressions::Expression const& upperBound,
                                                  std::shared_ptr<storm::logic::TimeBoundReference> const& timeBoundReference) const {
    // As soon as it somehow does not break everything anymore, I will change return types here.

    storm::logic::TimeBound lower(false, lowerBound);
    storm::logic::TimeBound upper(false, upperBound);
    return std::make_tuple(lower, upper, timeBoundReference);
}

std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>, std::shared_ptr<storm::logic::TimeBoundReference>>
FormulaParserGrammar::createTimeBoundFromSingleBound(storm::expressions::Expression const& bound, bool upperBound, bool strict,
                                                     std::shared_ptr<storm::logic::TimeBoundReference> const& timeBoundReference) const {
    // As soon as it somehow does not break everything anymore, I will change return types here.
    if (upperBound) {
        return std::make_tuple(boost::none, storm::logic::TimeBound(strict, bound), timeBoundReference);
    } else {
        return std::make_tuple(storm::logic::TimeBound(strict, bound), boost::none, timeBoundReference);
    }
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createInstantaneousRewardFormula(storm::expressions::Expression const& timeBound) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::InstantaneousRewardFormula(timeBound));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createCumulativeRewardFormula(
    std::vector<std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>,
                           std::shared_ptr<storm::logic::TimeBoundReference>>> const& timeBounds) const {
    std::vector<storm::logic::TimeBound> bounds;
    std::vector<storm::logic::TimeBoundReference> timeBoundReferences;
    for (auto const& timeBound : timeBounds) {
        STORM_LOG_THROW(!std::get<0>(timeBound), storm::exceptions::WrongFormatException, "Cumulative reward formulas with lower time bound are not allowed.");
        STORM_LOG_THROW(std::get<1>(timeBound), storm::exceptions::WrongFormatException, "Cumulative reward formulas require an upper bound.");
        bounds.push_back(std::get<1>(timeBound).get());
        timeBoundReferences.emplace_back(*std::get<2>(timeBound));
    }
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::CumulativeRewardFormula(bounds, timeBoundReferences));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createDiscountedCumulativeRewardFormula(
    storm::expressions::Expression const& discountFactor,
    std::vector<std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>,
                           std::shared_ptr<storm::logic::TimeBoundReference>>> const& timeBounds) const {
    std::vector<storm::logic::TimeBound> bounds;
    std::vector<storm::logic::TimeBoundReference> timeBoundReferences;
    for (auto const& timeBound : timeBounds) {
        STORM_LOG_THROW(!std::get<0>(timeBound), storm::exceptions::WrongFormatException, "Cumulative reward formulas with lower time bound are not allowed.");
        STORM_LOG_THROW(std::get<1>(timeBound), storm::exceptions::WrongFormatException, "Cumulative reward formulas require an upper bound.");
        bounds.push_back(std::get<1>(timeBound).get());
        timeBoundReferences.emplace_back(*std::get<2>(timeBound));
    }
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::DiscountedCumulativeRewardFormula(discountFactor, bounds, timeBoundReferences));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createTotalRewardFormula() const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::TotalRewardFormula());
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createDiscountedTotalRewardFormula(
    storm::expressions::Expression const& discountFactor) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::DiscountedTotalRewardFormula(discountFactor));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createLongRunAverageRewardFormula() const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::LongRunAverageRewardFormula());
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createAtomicExpressionFormula(storm::expressions::Expression const& expression) const {
    STORM_LOG_THROW(expression.hasBooleanType(), storm::exceptions::WrongFormatException,
                    "Expected expression " + expression.toString() + " to be of boolean type.");
    if (expression.isLiteral()) {
        return createBooleanLiteralFormula(expression.evaluateAsBool());
    }
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::AtomicExpressionFormula(expression));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createBooleanLiteralFormula(bool literal) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::BooleanLiteralFormula(literal));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createAtomicLabelFormula(std::string const& label) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::AtomicLabelFormula(label));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createEventuallyFormula(
    boost::optional<std::vector<std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>,
                                           std::shared_ptr<storm::logic::TimeBoundReference>>>> const& timeBounds,
    storm::logic::FormulaContext context, std::shared_ptr<storm::logic::Formula const> const& subformula) const {
    if (timeBounds && !timeBounds.get().empty()) {
        std::vector<boost::optional<storm::logic::TimeBound>> lowerBounds, upperBounds;
        std::vector<storm::logic::TimeBoundReference> timeBoundReferences;
        for (auto const& timeBound : timeBounds.get()) {
            lowerBounds.push_back(std::get<0>(timeBound));
            upperBounds.push_back(std::get<1>(timeBound));
            timeBoundReferences.emplace_back(*std::get<2>(timeBound));
        }
        return std::shared_ptr<storm::logic::Formula const>(
            new storm::logic::BoundedUntilFormula(createBooleanLiteralFormula(true), subformula, lowerBounds, upperBounds, timeBoundReferences));
    } else {
        return std::shared_ptr<storm::logic::Formula const>(new storm::logic::EventuallyFormula(subformula, context));
    }
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createGloballyFormula(std::shared_ptr<storm::logic::Formula const> const& subformula) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::GloballyFormula(subformula));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createNextFormula(std::shared_ptr<storm::logic::Formula const> const& subformula) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::NextFormula(subformula));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createUntilFormula(
    std::shared_ptr<storm::logic::Formula const> const& leftSubformula,
    boost::optional<std::vector<std::tuple<boost::optional<storm::logic::TimeBound>, boost::optional<storm::logic::TimeBound>,
                                           std::shared_ptr<storm::logic::TimeBoundReference>>>> const& timeBounds,
    std::shared_ptr<storm::logic::Formula const> const& rightSubformula) {
    if (timeBounds && !timeBounds.get().empty()) {
        std::vector<boost::optional<storm::logic::TimeBound>> lowerBounds, upperBounds;
        std::vector<storm::logic::TimeBoundReference> timeBoundReferences;
        for (auto const& timeBound : timeBounds.get()) {
            lowerBounds.push_back(std::get<0>(timeBound));
            upperBounds.push_back(std::get<1>(timeBound));
            timeBoundReferences.emplace_back(*std::get<2>(timeBound));
        }
        return std::shared_ptr<storm::logic::Formula const>(
            new storm::logic::BoundedUntilFormula(leftSubformula, rightSubformula, lowerBounds, upperBounds, timeBoundReferences));
    } else {
        return std::shared_ptr<storm::logic::Formula const>(new storm::logic::UntilFormula(leftSubformula, rightSubformula));
    }
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createHOAPathFormula(std::string const& automatonFile) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::HOAPathFormula(automatonFile));
}

void FormulaParserGrammar::addHoaAPMapping(storm::logic::Formula const& hoaFormula, const std::string& ap,
                                           std::shared_ptr<storm::logic::Formula const>& expression) const {
    // taking a const Formula reference and doing static_ and const_cast from Formula to allow non-const access to
    // qi::_val of the hoaPathFormula rule
    storm::logic::HOAPathFormula& hoaFormula_ = static_cast<storm::logic::HOAPathFormula&>(const_cast<storm::logic::Formula&>(hoaFormula));
    hoaFormula_.addAPMapping(ap, expression);
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createConditionalFormula(
    std::shared_ptr<storm::logic::Formula const> const& leftSubformula, boost::optional<std::shared_ptr<storm::logic::Formula const>> const& rightSubformula,
    storm::logic::FormulaContext context) const {
    if (rightSubformula) {
        return std::shared_ptr<storm::logic::Formula const>(new storm::logic::ConditionalFormula(leftSubformula, rightSubformula.get(), context));
    } else {
        // If there is no rhs, just return the lhs
        return leftSubformula;
    }
}

storm::logic::OperatorInformation FormulaParserGrammar::createOperatorInformation(boost::optional<storm::OptimizationDirection> const& optimizationDirection,
                                                                                  boost::optional<storm::logic::ComparisonType> const& comparisonType,
                                                                                  boost::optional<storm::expressions::Expression> const& threshold) const {
    if (comparisonType && threshold) {
        storm::expressions::ExpressionEvaluator<storm::RationalNumber> evaluator(*constManager);
        return storm::logic::OperatorInformation(optimizationDirection, storm::logic::Bound(comparisonType.get(), threshold.get()));
    } else {
        return storm::logic::OperatorInformation(optimizationDirection, boost::none);
    }
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createOperatorFormula(storm::logic::FormulaContext const& context,
                                                                                         boost::optional<std::string> const& rewardModelName,
                                                                                         storm::logic::OperatorInformation const& operatorInformation,
                                                                                         std::shared_ptr<storm::logic::Formula const> const& subformula) {
    switch (context) {
        case storm::logic::FormulaContext::Probability:
            STORM_LOG_ASSERT(!rewardModelName, "Probability operator with reward information parsed.");
            return createProbabilityOperatorFormula(operatorInformation, subformula);
        case storm::logic::FormulaContext::Reward:
            return createRewardOperatorFormula(rewardModelName, operatorInformation, subformula);
        case storm::logic::FormulaContext::LongRunAverage:
            STORM_LOG_ASSERT(!rewardModelName, "LRA operator with reward information parsed.");
            return createLongRunAverageOperatorFormula(operatorInformation, subformula);
        case storm::logic::FormulaContext::Time:
            STORM_LOG_ASSERT(!rewardModelName, "Time operator with reward model name parsed.");
            return createTimeOperatorFormula(operatorInformation, subformula);
        default:
            STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Unexpected formula context.");
    }
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createLongRunAverageOperatorFormula(
    storm::logic::OperatorInformation const& operatorInformation, std::shared_ptr<storm::logic::Formula const> const& subformula) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::LongRunAverageOperatorFormula(subformula, operatorInformation));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createRewardOperatorFormula(
    boost::optional<std::string> const& rewardModelName, storm::logic::OperatorInformation const& operatorInformation,
    std::shared_ptr<storm::logic::Formula const> const& subformula) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::RewardOperatorFormula(subformula, rewardModelName, operatorInformation));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createTimeOperatorFormula(
    storm::logic::OperatorInformation const& operatorInformation, std::shared_ptr<storm::logic::Formula const> const& subformula) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::TimeOperatorFormula(subformula, operatorInformation));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createProbabilityOperatorFormula(
    storm::logic::OperatorInformation const& operatorInformation, std::shared_ptr<storm::logic::Formula const> const& subformula) {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::ProbabilityOperatorFormula(subformula, operatorInformation));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createBinaryBooleanStateFormula(
    std::shared_ptr<storm::logic::Formula const> const& leftSubformula, std::shared_ptr<storm::logic::Formula const> const& rightSubformula,
    storm::logic::BinaryBooleanStateFormula::OperatorType operatorType) {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::BinaryBooleanStateFormula(operatorType, leftSubformula, rightSubformula));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createUnaryBooleanStateFormula(
    std::shared_ptr<storm::logic::Formula const> const& subformula, boost::optional<storm::logic::UnaryBooleanStateFormula::OperatorType> const& operatorType) {
    if (operatorType) {
        return std::shared_ptr<storm::logic::Formula const>(new storm::logic::UnaryBooleanStateFormula(operatorType.get(), subformula));
    } else {
        return subformula;
    }
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createBinaryBooleanPathFormula(
    std::shared_ptr<storm::logic::Formula const> const& leftSubformula, std::shared_ptr<storm::logic::Formula const> const& rightSubformula,
    storm::logic::BinaryBooleanPathFormula::OperatorType operatorType) {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::BinaryBooleanPathFormula(operatorType, leftSubformula, rightSubformula));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createUnaryBooleanPathFormula(
    std::shared_ptr<storm::logic::Formula const> const& subformula, boost::optional<storm::logic::UnaryBooleanPathFormula::OperatorType> const& operatorType) {
    if (operatorType) {
        return std::shared_ptr<storm::logic::Formula const>(new storm::logic::UnaryBooleanPathFormula(operatorType.get(), subformula));
    } else {
        return subformula;
    }
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createBinaryBooleanStateOrPathFormula(
    std::shared_ptr<storm::logic::Formula const> const& leftSubformula, std::shared_ptr<storm::logic::Formula const> const& rightSubformula,
    storm::logic::BinaryBooleanOperatorType operatorType) {
    if (leftSubformula->isStateFormula() && rightSubformula->isStateFormula()) {
        return createBinaryBooleanStateFormula(leftSubformula, rightSubformula, operatorType);
    } else if (leftSubformula->isPathFormula() || rightSubformula->isPathFormula()) {
        return createBinaryBooleanPathFormula(leftSubformula, rightSubformula, operatorType);
    }
    STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Subformulas have unexpected type.");
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createUnaryBooleanStateOrPathFormula(
    std::shared_ptr<storm::logic::Formula const> const& subformula, boost::optional<storm::logic::UnaryBooleanOperatorType> const& operatorType) {
    if (subformula->isStateFormula()) {
        return createUnaryBooleanStateFormula(subformula, operatorType);
    } else if (subformula->isPathFormula()) {
        return createUnaryBooleanPathFormula(subformula, operatorType);
    }
    STORM_LOG_THROW(false, storm::exceptions::WrongFormatException, "Subformulas have unexpected type.");
}

bool FormulaParserGrammar::isValidMultiBoundedPathFormulaOperand(std::shared_ptr<storm::logic::Formula const> const& operand) {
    if (operand->isBoundedUntilFormula()) {
        if (!operand->asBoundedUntilFormula().isMultiDimensional()) {
            return true;
        }
        STORM_LOG_ERROR("Composition of multidimensional bounded until formula must consist of single dimension subformulas. Got '" << *operand
                                                                                                                                    << "' instead.");
    }
    return false;
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createMultiBoundedPathFormula(
    std::vector<std::shared_ptr<storm::logic::Formula const>> const& subformulas) {
    std::vector<std::shared_ptr<storm::logic::Formula const>> leftSubformulas, rightSubformulas;
    std::vector<boost::optional<storm::logic::TimeBound>> lowerBounds, upperBounds;
    std::vector<storm::logic::TimeBoundReference> timeBoundReferences;
    for (auto const& subformula : subformulas) {
        STORM_LOG_THROW(subformula->isBoundedUntilFormula(), storm::exceptions::WrongFormatException,
                        "Multi-path formulas require bounded until (or eventually) subformulae. Got '" << *subformula << "' instead.");
        auto const& f = subformula->asBoundedUntilFormula();
        STORM_LOG_THROW(!f.isMultiDimensional(), storm::exceptions::WrongFormatException,
                        "Composition of multidimensional bounded until formula must consist of single dimension subformulas. Got '" << f << "' instead.");
        leftSubformulas.push_back(f.getLeftSubformula().asSharedPointer());
        rightSubformulas.push_back(f.getRightSubformula().asSharedPointer());
        if (f.hasLowerBound()) {
            lowerBounds.emplace_back(storm::logic::TimeBound(f.isLowerBoundStrict(), f.getLowerBound()));
        } else {
            lowerBounds.emplace_back();
        }
        if (f.hasUpperBound()) {
            upperBounds.emplace_back(storm::logic::TimeBound(f.isUpperBoundStrict(), f.getUpperBound()));
        } else {
            upperBounds.emplace_back();
        }
        timeBoundReferences.push_back(f.getTimeBoundReference());
    }
    return std::shared_ptr<storm::logic::Formula const>(
        new storm::logic::BoundedUntilFormula(leftSubformulas, rightSubformulas, lowerBounds, upperBounds, timeBoundReferences));
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createMultiOperatorFormula(
    std::vector<std::shared_ptr<storm::logic::Formula const>> const& subformulas, storm::logic::MultiObjectiveFormula::Type type) {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::MultiObjectiveFormula(subformulas, type));
}

storm::expressions::Variable FormulaParserGrammar::createQuantileBoundVariables(boost::optional<storm::solver::OptimizationDirection> const& dir,
                                                                                std::string const& variableName) {
    STORM_LOG_ASSERT(manager, "Mutable expression manager required to define quantile bound variable.");
    storm::expressions::Variable var;
    if (manager->hasVariable(variableName)) {
        var = manager->getVariable(variableName);
        STORM_LOG_THROW(quantileFormulaVariables.count(var) > 0, storm::exceptions::WrongFormatException,
                        "Invalid quantile variable name '" << variableName << "' in quantile formula: variable already exists.");
    } else {
        var = manager->declareRationalVariable(variableName);
        quantileFormulaVariables.insert(var);
    }
    STORM_LOG_WARN_COND(!dir.is_initialized(), "Optimization direction '"
                                                   << dir.get() << "' for quantile variable " << variableName
                                                   << " is ignored. This information will be derived from the subformula of the quantile.");
    addIdentifierExpression(variableName, var);
    return var;
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createQuantileFormula(std::vector<storm::expressions::Variable> const& boundVariables,
                                                                                         std::shared_ptr<storm::logic::Formula const> const& subformula) {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::QuantileFormula(boundVariables, subformula));
}

std::set<storm::expressions::Variable> FormulaParserGrammar::getUndefinedConstants(std::shared_ptr<storm::logic::Formula const> const& formula) const {
    std::set<storm::expressions::Variable> result;
    std::set<storm::expressions::Variable> usedVariables = formula->getUsedVariables();
    std::set_intersection(usedVariables.begin(), usedVariables.end(), undefinedConstants.begin(), undefinedConstants.end(),
                          std::inserter(result, result.begin()));
    return result;
}

storm::jani::Property FormulaParserGrammar::createProperty(boost::optional<std::string> const& propertyName, storm::modelchecker::FilterType const& filterType,
                                                           std::shared_ptr<storm::logic::Formula const> const& formula,
                                                           std::shared_ptr<storm::logic::Formula const> const& states) {
    storm::jani::FilterExpression filterExpression(formula, filterType, states);

    ++propertyCount;
    if (propertyName) {
        return storm::jani::Property(propertyName.get(), filterExpression, this->getUndefinedConstants(formula));
    } else {
        return storm::jani::Property(std::to_string(propertyCount - 1), filterExpression, this->getUndefinedConstants(formula));
    }
}

storm::jani::Property FormulaParserGrammar::createPropertyWithDefaultFilterTypeAndStates(boost::optional<std::string> const& propertyName,
                                                                                         std::shared_ptr<storm::logic::Formula const> const& formula) {
    ++propertyCount;
    if (propertyName) {
        return storm::jani::Property(propertyName.get(), formula, this->getUndefinedConstants(formula));
    } else {
        return storm::jani::Property(std::to_string(propertyCount), formula, this->getUndefinedConstants(formula));
    }
}

storm::logic::PlayerCoalition FormulaParserGrammar::createPlayerCoalition(
    std::vector<std::variant<std::string, storm::storage::PlayerIndex>> const& playerIds) const {
    return storm::logic::PlayerCoalition(playerIds);
}

std::shared_ptr<storm::logic::Formula const> FormulaParserGrammar::createGameFormula(storm::logic::PlayerCoalition const& coalition,
                                                                                     std::shared_ptr<storm::logic::Formula const> const& subformula) const {
    return std::shared_ptr<storm::logic::Formula const>(new storm::logic::GameFormula(coalition, subformula));
}

bool FormulaParserGrammar::isBooleanReturnType(std::shared_ptr<storm::logic::Formula const> const& formula, bool raiseErrorMessage) {
    if (formula->hasQualitativeResult()) {
        return true;
    }
    STORM_LOG_ERROR_COND(!raiseErrorMessage, "Formula " << *formula << " does not have a Boolean return type.");
    return false;
}

bool FormulaParserGrammar::raiseAmbiguousNonAssociativeOperatorError(std::shared_ptr<storm::logic::Formula const> const& formula, std::string const& op) {
    STORM_LOG_ERROR("Ambiguous use of non-associative operator '" << op << "' in formula '" << *formula << " U ... '");
    return true;
}

}  // namespace parser
}  // namespace storm
