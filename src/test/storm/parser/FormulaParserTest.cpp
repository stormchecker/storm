#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/FormulaParser.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/automata/DeterministicAutomaton.h"
#include "storm/exceptions/ExpressionEvaluationException.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/storage/expressions/ExpressionManager.h"

TEST(FormulaParserTest, LabelTest) {
    storm::parser::FormulaParser formulaParser;

    std::string input = "\"label\"";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isAtomicLabelFormula());
}

TEST(FormulaParserTest, ComplexLabelTest) {
    storm::parser::FormulaParser formulaParser;

    std::string input = "!(\"a\" & \"b\") | \"a\" & !\"c\"";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isInFragment(storm::logic::propositional()));
    EXPECT_TRUE(formula->isBinaryBooleanStateFormula());
}

TEST(FormulaParserTest, ExpressionTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    manager->declareBooleanVariable("x");
    manager->declareIntegerVariable("y");

    storm::parser::FormulaParser formulaParser(manager);

    std::string input = "!(x | y > 3)";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isInFragment(storm::logic::propositional()));
    EXPECT_TRUE(formula->isAtomicExpressionFormula());
}

TEST(FormulaParserTest, ExpressionTest2) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::parser::FormulaParser formulaParser(manager);

    std::string input = "(false)=false";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isInFragment(storm::logic::propositional()));
    EXPECT_TRUE(formula->isAtomicExpressionFormula());
}

TEST(FormulaParserTest, ExpressionTest3) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());

    storm::parser::FormulaParser formulaParser(manager);

    std::string input = "1 & false";
    STORM_SILENT_ASSERT_THROW(formulaParser.parseSingleFormulaFromString(input), storm::exceptions::WrongFormatException);
}

TEST(FormulaParserTest, LabelAndExpressionTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    manager->declareBooleanVariable("x");
    manager->declareIntegerVariable("y");

    storm::parser::FormulaParser formulaParser(manager);

    std::string input = "!\"a\" | x | y > 3";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isInFragment(storm::logic::propositional()));

    input = "x | y > 3 | !\"a\"";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    EXPECT_TRUE(formula->isInFragment(storm::logic::propositional()));
}

TEST(FormulaParserTest, ProbabilityOperatorTest) {
    storm::parser::FormulaParser formulaParser;

    std::string input = "P<0.9 [\"a\" U \"b\"]";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isProbabilityOperatorFormula());
    EXPECT_TRUE(formula->asProbabilityOperatorFormula().hasBound());
    EXPECT_FALSE(formula->asProbabilityOperatorFormula().hasOptimalityType());
}

TEST(FormulaParserTest, ProbabilityOperatorTest2) {
    storm::parser::FormulaParser formulaParser;

    std::string input = "1 = 1 & 2 = 2 & true & P<0.9 [\"a\" U \"b\"]";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    ASSERT_TRUE(formula->isBinaryBooleanStateFormula());
    EXPECT_TRUE(formula->asBinaryBooleanStateFormula().isAnd());
    EXPECT_TRUE(formula->asBinaryBooleanStateFormula().getLeftSubformula().isAtomicExpressionFormula());
    EXPECT_TRUE(formula->asBinaryBooleanStateFormula().getRightSubformula().isProbabilityOperatorFormula());
}

TEST(FormulaParserTest, UntilOperatorTest) {
    // API does not allow direct test of until, so we have to pack it in a probability operator.
    storm::parser::FormulaParser formulaParser;

    std::string input = "P<0.9 [\"a\" U \"b\"]";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested1 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested1.isUntilFormula());
    EXPECT_FALSE(nested1.isBoundedUntilFormula());

    input = "P<0.9 [\"a\" U<=3 \"b\"]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested2 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested2.isBoundedUntilFormula());
    EXPECT_TRUE(nested2.asBoundedUntilFormula().getTimeBoundReference().isTimeBound());
    EXPECT_EQ(3, nested2.asBoundedUntilFormula().getUpperBound().evaluateAsInt());

    input = "P<0.9 [\"a\" U{\"rewardname\"}<=3 \"b\"]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested3 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested3.isBoundedUntilFormula());
    EXPECT_TRUE(nested3.asBoundedUntilFormula().getTimeBoundReference().isRewardBound());
    EXPECT_TRUE(nested3.asBoundedUntilFormula().getTimeBoundReference().hasRewardModelName());
    EXPECT_EQ("rewardname", nested3.asBoundedUntilFormula().getTimeBoundReference().getRewardName());
    EXPECT_EQ(3, nested3.asBoundedUntilFormula().getUpperBound().evaluateAsInt());

    input = "P<0.9 [\"a\" Urew{\"rewardname\"}<=3 \"b\"]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested4 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested4.isBoundedUntilFormula());
    EXPECT_TRUE(nested4.asBoundedUntilFormula().getTimeBoundReference().isRewardBound());
    EXPECT_TRUE(nested4.asBoundedUntilFormula().getTimeBoundReference().hasRewardModelName());
    EXPECT_EQ("rewardname", nested4.asBoundedUntilFormula().getTimeBoundReference().getRewardName());
    EXPECT_EQ(3, nested4.asBoundedUntilFormula().getUpperBound().evaluateAsInt());

    input = "P<0.9 [\"a\" Urew<=3 \"b\"]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested5 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested5.isBoundedUntilFormula());
    EXPECT_TRUE(nested5.asBoundedUntilFormula().getTimeBoundReference().isRewardBound());
    EXPECT_FALSE(nested5.asBoundedUntilFormula().getTimeBoundReference().hasRewardModelName());
    EXPECT_EQ(3, nested5.asBoundedUntilFormula().getUpperBound().evaluateAsInt());
}

TEST(FormulaParserTest, RewardOperatorTest) {
    storm::parser::FormulaParser formulaParser;

    std::string input = "Rmin<0.9 [F \"a\"]";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isRewardOperatorFormula());
    EXPECT_TRUE(formula->asRewardOperatorFormula().hasBound());
    EXPECT_TRUE(formula->asRewardOperatorFormula().hasOptimalityType());

    input = "R=? [I=10]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isRewardOperatorFormula());
    EXPECT_FALSE(formula->asRewardOperatorFormula().hasBound());
    EXPECT_FALSE(formula->asRewardOperatorFormula().hasOptimalityType());
    EXPECT_TRUE(formula->asRewardOperatorFormula().getSubformula().isInstantaneousRewardFormula());
}

TEST(FormulaParserTest, ConditionalProbabilityTest) {
    storm::parser::FormulaParser formulaParser;

    std::string input = "P<0.9 [F \"a\" || F \"b\"]";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isProbabilityOperatorFormula());
    storm::logic::ProbabilityOperatorFormula const &probFormula = formula->asProbabilityOperatorFormula();
    EXPECT_TRUE(probFormula.getSubformula().isConditionalProbabilityFormula());
}

TEST(FormulaParserTest, NestedPathFormulaTest) {
    storm::parser::FormulaParser formulaParser;

    std::string input = "P<0.9 [F X \"a\"]";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isProbabilityOperatorFormula());
    ASSERT_TRUE(formula->asProbabilityOperatorFormula().getSubformula().isEventuallyFormula());
    ASSERT_TRUE(formula->asProbabilityOperatorFormula().getSubformula().asEventuallyFormula().getSubformula().isNextFormula());
}

TEST(FormulaParserTest, DiscountedFormulaTest) {
    storm::parser::FormulaParser formulaParser;

    std::string input = "Rmax=? [Cdiscount=0.9]";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);

    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isRewardOperatorFormula());
    ASSERT_TRUE(formula->asRewardOperatorFormula().getSubformula().isDiscountedTotalRewardFormula());
    ASSERT_FALSE(formula->asRewardOperatorFormula().getSubformula().isTotalRewardFormula());
    static const std::string discountFactorString1 = "9/10";
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(discountFactorString1),
              formula->asRewardOperatorFormula().getSubformula().asDiscountedTotalRewardFormula().getDiscountFactor<storm::RationalNumber>());
    EXPECT_FLOAT_EQ(0.9, formula->asRewardOperatorFormula().getSubformula().asDiscountedTotalRewardFormula().getDiscountFactor<double>());

    input = "Rmin=? [C<=5discount=(0.95)]";

    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isRewardOperatorFormula());
    ASSERT_TRUE(formula->asRewardOperatorFormula().getSubformula().isDiscountedCumulativeRewardFormula());
    static const std::string discountFactorString2 = "19/20";
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(discountFactorString2),
              formula->asRewardOperatorFormula().getSubformula().asDiscountedCumulativeRewardFormula().getDiscountFactor().evaluateAsRational());
    EXPECT_FLOAT_EQ(0.95, formula->asRewardOperatorFormula().getSubformula().asDiscountedCumulativeRewardFormula().getDiscountFactor().evaluateAsDouble());
    EXPECT_TRUE(formula->asRewardOperatorFormula().getSubformula().asDiscountedCumulativeRewardFormula().getTimeBoundReference().isTimeBound());
    EXPECT_EQ(5, formula->asRewardOperatorFormula().getSubformula().asDiscountedCumulativeRewardFormula().getBound().evaluateAsInt());

    input = "Rmax=? [Crew<8discount=(0.5)]";

    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));

    EXPECT_TRUE(formula->isRewardOperatorFormula());
    ASSERT_TRUE(formula->asRewardOperatorFormula().getSubformula().isDiscountedCumulativeRewardFormula());
    static const std::string discountFactorString3 = "1/2";
    EXPECT_EQ(storm::utility::convertNumber<storm::RationalNumber>(discountFactorString3),
              formula->asRewardOperatorFormula().getSubformula().asDiscountedCumulativeRewardFormula().getDiscountFactor().evaluateAsRational());
    EXPECT_FLOAT_EQ(0.5, formula->asRewardOperatorFormula().getSubformula().asDiscountedCumulativeRewardFormula().getDiscountFactor().evaluateAsDouble());
    EXPECT_FALSE(formula->asRewardOperatorFormula().getSubformula().asDiscountedCumulativeRewardFormula().getTimeBoundReference().isTimeBound());
    EXPECT_TRUE(formula->asRewardOperatorFormula().getSubformula().asDiscountedCumulativeRewardFormula().getTimeBoundReference().isRewardBound());
    EXPECT_EQ(8, formula->asRewardOperatorFormula().getSubformula().asDiscountedCumulativeRewardFormula().getBound().evaluateAsInt());
}

TEST(FormulaParserTest, CommentTest) {
    storm::parser::FormulaParser formulaParser;

    std::string input =
        "// This is a comment. And this is a commented out formula: P<=0.5 [ F \"a\" ] The next line contains the actual formula. \n P<=0.5 [ X \"b\" ] // "
        "Another comment \n // And again: another comment.";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    EXPECT_TRUE(formula->isProbabilityOperatorFormula());
    ASSERT_TRUE(formula->asProbabilityOperatorFormula().getSubformula().isNextFormula());
    ASSERT_TRUE(formula->asProbabilityOperatorFormula().getSubformula().asNextFormula().getSubformula().isAtomicLabelFormula());
}

TEST(FormulaParserTest, WrongFormatTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    manager->declareBooleanVariable("x");
    manager->declareIntegerVariable("y");

    storm::parser::FormulaParser formulaParser(manager);
    std::string input = "P>0.5 [ a ]";
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);
    STORM_SILENT_EXPECT_THROW(formula = formulaParser.parseSingleFormulaFromString(input), storm::exceptions::WrongFormatException);

    input = "P=0.5 [F \"a\"]";
    STORM_SILENT_EXPECT_THROW(formula = formulaParser.parseSingleFormulaFromString(input), storm::exceptions::WrongFormatException);

    input = "P>0.5 [F !(x = 0)]";
    STORM_SILENT_EXPECT_THROW(formula = formulaParser.parseSingleFormulaFromString(input), storm::exceptions::WrongFormatException);

    input = "P>0.5 [F !y]";
    STORM_SILENT_EXPECT_THROW(formula = formulaParser.parseSingleFormulaFromString(input), storm::exceptions::WrongFormatException);

    input = "P>0.5 [F y!=0)]";
    STORM_SILENT_EXPECT_THROW(formula = formulaParser.parseSingleFormulaFromString(input), storm::exceptions::WrongFormatException);

    input = "P<0.9 [G F]";
    STORM_SILENT_EXPECT_THROW(formula = formulaParser.parseSingleFormulaFromString(input), storm::exceptions::WrongFormatException);

    input = "P<0.9 [\"a\" U \"b\" U \"c\"]";
    STORM_SILENT_EXPECT_THROW(formula = formulaParser.parseSingleFormulaFromString(input), storm::exceptions::WrongFormatException);

    input = "P<0.9 [X \"a\" U G \"b\" U X \"c\"]";
    STORM_SILENT_EXPECT_THROW(formula = formulaParser.parseSingleFormulaFromString(input), storm::exceptions::WrongFormatException);
}

TEST(FormulaParserTest, MultiObjectiveFormulaTest) {
    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula;

    auto checkSubformulas = [](storm::logic::MultiObjectiveFormula const &mof) {
        ASSERT_EQ(3ull, mof.getNumberOfSubformulas());

        ASSERT_TRUE(mof.getSubformula(0).isProbabilityOperatorFormula());
        ASSERT_TRUE(mof.getSubformula(0).asProbabilityOperatorFormula().getSubformula().isEventuallyFormula());
        ASSERT_TRUE(mof.getSubformula(0).asProbabilityOperatorFormula().getSubformula().asEventuallyFormula().getSubformula().isAtomicLabelFormula());
        ASSERT_TRUE(mof.getSubformula(0).asProbabilityOperatorFormula().hasBound());

        ASSERT_TRUE(mof.getSubformula(1).isRewardOperatorFormula());
        ASSERT_TRUE(mof.getSubformula(1).asRewardOperatorFormula().getSubformula().isEventuallyFormula());
        ASSERT_TRUE(mof.getSubformula(1).asRewardOperatorFormula().getSubformula().asEventuallyFormula().getSubformula().isAtomicLabelFormula());
        ASSERT_TRUE(mof.getSubformula(1).asRewardOperatorFormula().hasBound());

        ASSERT_TRUE(mof.getSubformula(2).isProbabilityOperatorFormula());
        ASSERT_TRUE(mof.getSubformula(2).asProbabilityOperatorFormula().getSubformula().isEventuallyFormula());
        ASSERT_TRUE(mof.getSubformula(2).asProbabilityOperatorFormula().getSubformula().asEventuallyFormula().getSubformula().isAtomicLabelFormula());
        ASSERT_TRUE(mof.getSubformula(2).asProbabilityOperatorFormula().hasOptimalityType());
        ASSERT_TRUE(storm::solver::minimize(mof.getSubformula(2).asProbabilityOperatorFormula().getOptimalityType()));
    };

    std::string input = "multi(P<0.9 [ F \"a\" ], R<42 [ F \"b\" ], Pmin=? [ F\"c\" ])";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    ASSERT_TRUE(formula->isMultiObjectiveFormula());
    storm::logic::MultiObjectiveFormula const &mof = formula->asMultiObjectiveFormula();
    ASSERT_TRUE(mof.isTradeoff());
    checkSubformulas(mof);

    input = "multilex(P<0.9 [ F \"a\" ], R<42 [ F \"b\" ], Pmin=? [ F\"c\" ])";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    ASSERT_TRUE(formula->isMultiObjectiveFormula());
    storm::logic::MultiObjectiveFormula const &mlof = formula->asMultiObjectiveFormula();
    ASSERT_TRUE(mlof.isLexicographic());
    checkSubformulas(mlof);
}

TEST(FormulaParserTest, LogicalPrecedenceTest) {
    // Test precedence of logical operators over temporal operators, etc.
    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);

    std::string input = "P=? [ !\"a\" & \"b\" U ! \"c\" | \"b\" ]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    EXPECT_TRUE(formula->isProbabilityOperatorFormula());

    auto const &nested1 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested1.isUntilFormula());
    ASSERT_TRUE(nested1.asUntilFormula().getLeftSubformula().isBinaryBooleanStateFormula());
    ASSERT_TRUE(nested1.asUntilFormula().getLeftSubformula().asBinaryBooleanStateFormula().getLeftSubformula().isUnaryBooleanStateFormula());
    ASSERT_TRUE(nested1.asUntilFormula().getRightSubformula().isBinaryBooleanStateFormula());
    ASSERT_TRUE(nested1.asUntilFormula().getRightSubformula().asBinaryBooleanStateFormula().getLeftSubformula().isUnaryBooleanStateFormula());

    input = "P<0.9 [ F G !\"a\" | \"b\" ] ";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested2 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested2.asEventuallyFormula().getSubformula().isGloballyFormula());
    auto const &nested2Subformula = nested2.asEventuallyFormula().getSubformula().asGloballyFormula();
    EXPECT_TRUE(nested2Subformula.getSubformula().isBinaryBooleanStateFormula());
    ASSERT_TRUE(nested2Subformula.getSubformula().asBinaryBooleanStateFormula().getLeftSubformula().isUnaryBooleanStateFormula());

    input = "P<0.9 [ X G \"a\" | !\"b\" | \"c\"] ";  // from left to right
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested3 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested3.asNextFormula().getSubformula().isGloballyFormula());
    EXPECT_TRUE(nested3.asNextFormula().getSubformula().asGloballyFormula().getSubformula().isBinaryBooleanStateFormula());
    auto const &nested3Subformula = nested3.asNextFormula().getSubformula().asGloballyFormula().getSubformula().asBinaryBooleanStateFormula();
    ASSERT_TRUE(nested3Subformula.getLeftSubformula().isBinaryBooleanStateFormula());
    ASSERT_TRUE(nested3Subformula.asBinaryBooleanStateFormula().getRightSubformula().isAtomicLabelFormula());

    input = "P<0.9 [ X F \"a\" | ! \"b\" & \"c\"] ";  // & has precedence over | and ! over &
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested4 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested4.asNextFormula().getSubformula().isEventuallyFormula());
    EXPECT_TRUE(nested4.asNextFormula().getSubformula().asEventuallyFormula().getSubformula().isBinaryBooleanStateFormula());
    auto const &nested4Subformula = nested4.asNextFormula().getSubformula().asEventuallyFormula().getSubformula().asBinaryBooleanStateFormula();
    ASSERT_TRUE(nested4Subformula.getLeftSubformula().isAtomicLabelFormula());
    ASSERT_TRUE(nested4Subformula.getRightSubformula().isBinaryBooleanStateFormula());

    input = "P<0.9 [X \"a\" | F \"b\"]";  // X (a | F b)
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested5 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested5.isNextFormula());
    EXPECT_TRUE(nested5.asNextFormula().getSubformula().isBinaryBooleanPathFormula());
    EXPECT_TRUE(nested5.asNextFormula().getSubformula().asBinaryPathFormula().getLeftSubformula().isAtomicLabelFormula());
    EXPECT_TRUE(nested5.asNextFormula().getSubformula().asBinaryPathFormula().getRightSubformula().isEventuallyFormula());

    input = "P<0.9 [F \"a\" | G \"b\" | X \"c\" ]";  // F (a | G (b | X c))
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested6 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested6.isEventuallyFormula());
    EXPECT_TRUE(nested6.asEventuallyFormula().getSubformula().isBinaryBooleanPathFormula());
    EXPECT_TRUE(nested6.asEventuallyFormula().getSubformula().asBinaryPathFormula().getLeftSubformula().isAtomicLabelFormula());
    EXPECT_TRUE(nested6.asEventuallyFormula().getSubformula().asBinaryPathFormula().getRightSubformula().isGloballyFormula());
    auto const &nested6Subformula = nested6.asEventuallyFormula().getSubformula().asBinaryPathFormula().getRightSubformula().asGloballyFormula();
    EXPECT_TRUE(nested6Subformula.getSubformula().isBinaryBooleanPathFormula());
    EXPECT_TRUE(nested6Subformula.getSubformula().asBinaryPathFormula().getLeftSubformula().isAtomicLabelFormula());
    EXPECT_TRUE(nested6Subformula.getSubformula().asBinaryPathFormula().getRightSubformula().isNextFormula());
    EXPECT_TRUE(nested6Subformula.getSubformula().asBinaryPathFormula().getRightSubformula().asNextFormula().getSubformula().isAtomicLabelFormula());
}

TEST(FormulaParserTest, TemporalPrecedenceTest) {
    // Unary operators (F, G and X) have precedence over binary operators (U).
    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);

    std::string input = "P=? [ F \"a\" U G \"b\" ]";  // (F a) U (G b)
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested1 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested1.isUntilFormula());
    ASSERT_TRUE(nested1.asUntilFormula().getLeftSubformula().isEventuallyFormula());
    ASSERT_TRUE(nested1.asUntilFormula().getRightSubformula().isGloballyFormula());

    input = "P=? [ X ( F \"a\" U G \"b\") U G \"c\"]";  // X((F a) U (G b)) U (G c)
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested2 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested2.isUntilFormula());
    EXPECT_TRUE(nested2.asUntilFormula().getLeftSubformula().isNextFormula());
    EXPECT_TRUE(nested2.asUntilFormula().getLeftSubformula().asNextFormula().getSubformula().isUntilFormula());
    EXPECT_TRUE(nested2.asUntilFormula().getRightSubformula().isGloballyFormula());

    input = "P=? [ X F \"a\" U (G \"b\" U G \"c\")]";  // (XF a) U ((G b) U (G c))
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested3 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested3.isUntilFormula());
    EXPECT_TRUE(nested3.asUntilFormula().getLeftSubformula().isNextFormula());
    EXPECT_TRUE(nested3.asUntilFormula().getLeftSubformula().asNextFormula().getSubformula().isEventuallyFormula());
    EXPECT_TRUE(nested3.asUntilFormula().getRightSubformula().isUntilFormula());
    EXPECT_TRUE(nested3.asUntilFormula().getRightSubformula().asUntilFormula().getLeftSubformula().isGloballyFormula());
    EXPECT_TRUE(nested3.asUntilFormula().getRightSubformula().asUntilFormula().getRightSubformula().isGloballyFormula());
}

TEST(FormulaParserTest, TemporalNegationTest) {
    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);

    std::string input = "P<0.9 [ ! X \"a\" | \"b\"]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested1 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested1.isUnaryBooleanPathFormula());
    EXPECT_TRUE(nested1.asUnaryPathFormula().getSubformula().isNextFormula());
    EXPECT_TRUE(nested1.asUnaryPathFormula().getSubformula().asNextFormula().getSubformula().isBinaryBooleanStateFormula());

    input = "P<0.9 [! F ! G \"b\"]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested2 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested2.isUnaryBooleanPathFormula());
    EXPECT_TRUE(nested2.asUnaryPathFormula().getSubformula().isEventuallyFormula());
    EXPECT_TRUE(nested2.asUnaryPathFormula().getSubformula().asEventuallyFormula().getSubformula().isUnaryBooleanPathFormula());
    EXPECT_TRUE(nested2.asUnaryPathFormula().getSubformula().asEventuallyFormula().getSubformula().asUnaryPathFormula().getSubformula().isGloballyFormula());

    input = "P<0.9 [! (\"a\" U \"b\")]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested3 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested3.isUnaryBooleanPathFormula());
    EXPECT_TRUE(nested3.asUnaryPathFormula().getSubformula().isUntilFormula());
}

TEST(FormulaParserTest, ComplexPathFormulaTest) {
    storm::parser::FormulaParser formulaParser;
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);

    std::string input = "P<0.9 [(X !\"a\") & (F ( X \"b\" U G \"c\" & \"d\"))]";  // ((X ! a) & (F ( (X b) U (G (c & d)))))
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested1 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested1.asBinaryPathFormula().getRightSubformula().asEventuallyFormula().getSubformula().isUntilFormula());
    auto const nested1Subformula = nested1.asBinaryPathFormula().getRightSubformula().asEventuallyFormula().getSubformula().asUntilFormula();
    EXPECT_TRUE(nested1Subformula.getLeftSubformula().isNextFormula());
    EXPECT_TRUE(nested1Subformula.getRightSubformula().asGloballyFormula().getSubformula().isBinaryBooleanStateFormula());

    input = "P<0.9 [(F \"a\") & (G \"b\") | (! \"a\" U (F X ! \"b\"))]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested2 = formula->asProbabilityOperatorFormula().getSubformula();
    ASSERT_TRUE(nested2.asBinaryPathFormula().getLeftSubformula().isBinaryPathFormula());
    ASSERT_TRUE(nested2.asBinaryPathFormula().getRightSubformula().isUntilFormula());

    input = "P=? [(X \"a\") U ( \"b\"& \"c\")]";
    formula = formulaParser.parseSingleFormulaFromString(input);
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested3 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested3.asBinaryPathFormula().isUntilFormula());
    EXPECT_TRUE(nested3.asBinaryPathFormula().getLeftSubformula().isNextFormula());
}

TEST(FormulaParserTest, HOAPathFormulaTest) {
    std::shared_ptr<storm::expressions::ExpressionManager> manager(new storm::expressions::ExpressionManager());
    manager->declareIntegerVariable("x");
    manager->declareIntegerVariable("y");
    storm::parser::FormulaParser formulaParser(manager);
    std::shared_ptr<storm::logic::Formula const> formula(nullptr);

    std::string input = "P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_Fandp0Xp1.hoa\", \"p0\" -> !\"a\", \"p1\" -> \"b\" | \"c\" }]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested1 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested1.isHOAPathFormula());
    EXPECT_TRUE(nested1.isPathFormula());

    ASSERT_NO_THROW(std::string af = nested1.asHOAPathFormula().getAutomatonFile());
    storm::automata::DeterministicAutomaton::ptr da1;
    ASSERT_NO_THROW(da1 = nested1.asHOAPathFormula().readAutomaton());
    EXPECT_EQ(3ul, da1->getNumberOfStates());
    EXPECT_EQ(4ul, da1->getNumberOfEdgesPerState());

    std::map<std::string, std::shared_ptr<storm::logic::Formula const>> apFormulaMap1 = nested1.asHOAPathFormula().getAPMapping();
    EXPECT_TRUE(apFormulaMap1["p0"]->isUnaryBooleanStateFormula());
    EXPECT_TRUE(apFormulaMap1["p1"]->isBinaryBooleanStateFormula());

    input = "P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_UXp0p1.hoa\", \"p0\" -> (x < 7) & !(y = 2), \"p1\" -> (x > 0) }]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested2 = formula->asProbabilityOperatorFormula().getSubformula();
    EXPECT_TRUE(nested2.isHOAPathFormula());
    EXPECT_TRUE(nested2.isPathFormula());

    ASSERT_NO_THROW(std::string af = nested2.asHOAPathFormula().getAutomatonFile());
    storm::automata::DeterministicAutomaton::ptr da2;
    ASSERT_NO_THROW(da2 = nested2.asHOAPathFormula().readAutomaton());
    EXPECT_EQ(4ul, da2->getNumberOfStates());
    EXPECT_EQ(4ul, da2->getNumberOfEdgesPerState());

    std::map<std::string, std::shared_ptr<storm::logic::Formula const>> apFormulaMap2 = nested2.asHOAPathFormula().getAPMapping();
    EXPECT_TRUE(apFormulaMap2["p0"]->isAtomicExpressionFormula());
    EXPECT_TRUE(apFormulaMap2["p1"]->isAtomicExpressionFormula());

    // Wrong format: p1 -> path formula
    input = "P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_UXp0p1.hoa\", \"p0\" -> (x > 0), \"p1\" -> (x < 7) & (X y = 2) }]";
    STORM_SILENT_EXPECT_THROW(formula = formulaParser.parseSingleFormulaFromString(input), storm::exceptions::WrongFormatException);

    // Exception: p1 not assigned
    input = "P=?[HOA: {\"" STORM_TEST_RESOURCES_DIR "/hoa/automaton_UXp0p1.hoa\", \"p0\" -> (x > 0)}]";
    ASSERT_NO_THROW(formula = formulaParser.parseSingleFormulaFromString(input));
    auto const &nested3 = formula->asProbabilityOperatorFormula().getSubformula();
    storm::automata::DeterministicAutomaton::ptr da3;
    STORM_SILENT_EXPECT_THROW(da3 = nested3.asHOAPathFormula().readAutomaton(), storm::exceptions::ExpressionEvaluationException);
}
