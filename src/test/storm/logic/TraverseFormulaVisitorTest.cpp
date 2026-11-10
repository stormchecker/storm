#include "storm-config.h"
#include "test/storm_gtest.h"

#include <string>
#include <vector>

#include "storm-parsers/parser/FormulaParser.h"
#include "storm/logic/Formula.h"
#include "storm/logic/FragmentSpecification.h"
#include "storm/logic/TraverseFormulaVisitor.h"

TEST(TraverseFormulaVisitorTest, VisitsAllSubformulas) {
    storm::parser::FormulaParser formulaParser;
    auto const formula = formulaParser.parseSingleFormulaFromString("P=? [\"a\" U (\"b\" & !\"c\")]");
    std::vector<std::string> visited;
    storm::logic::TraverseFormulaVisitor const visitor([&visited](storm::logic::Formula const& f) {
        visited.push_back(f.toString());
        return true;
    });
    visitor.traverse(*formula);
    // The operator, the until formula, "a", the conjunction, "b", the negation and "c" (in this order).
    ASSERT_EQ(7ull, visited.size());
    EXPECT_EQ(formula->toString(), visited.front());
    EXPECT_EQ("\"c\"", visited.back());
}

TEST(TraverseFormulaVisitorTest, MultiDimensionalBoundedUntil) {
    storm::parser::FormulaParser formulaParser;
    // The two dimensions of this bounded until share their subformulas, so each of them is visited only once.
    auto const formula = formulaParser.parseSingleFormulaFromString("P=? [F{\"rewA\"}<=3,{\"rewB\"}<=5 \"a\"]");
    uint64_t visitedLabelFormulas = 0;
    formula->traverse([&visitedLabelFormulas](storm::logic::Formula const& f) {
        if (f.isAtomicLabelFormula()) {
            ++visitedLabelFormulas;
        }
        return true;
    });
    EXPECT_EQ(1ull, visitedLabelFormulas);
}

TEST(TraverseFormulaVisitorTest, MaximalPropositionalSubformulas) {
    storm::parser::FormulaParser formulaParser;
    auto const formula = formulaParser.parseSingleFormulaFromString("P=? [(\"a\" | !\"b\") U (P>0.5 [F \"c\" & \"d\"] & \"e\")]");
    auto const propositionalFragment = storm::logic::propositional();
    std::vector<std::string> propositionalSubformulas;
    // Formula::traverse creates the visitor for us.
    formula->traverse([&propositionalFragment, &propositionalSubformulas](storm::logic::Formula const& f) {
        if (f.isInFragment(propositionalFragment)) {
            propositionalSubformulas.push_back(f.toString());
            return false;  // Do not visit the subformulas of a propositional formula.
        }
        return true;
    });

    std::vector<std::string> expected;
    for (std::string const subformula : {"\"a\" | !\"b\"", "\"c\" & \"d\"", "\"e\""}) {
        expected.push_back(formulaParser.parseSingleFormulaFromString(subformula)->toString());
    }
    EXPECT_EQ(expected, propositionalSubformulas);
}
