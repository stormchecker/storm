#include "storm-config.h"
#include "test/storm_gtest.h"

#include <set>
#include <sstream>
#include <vector>

#include "storm-parsers/parser/FormulaParser.h"
#include "storm/exceptions/WrongFormatException.h"
#include "storm/logic/Formulas.h"
#include "storm/modelchecker/results/FilterType.h"
#include "storm/storage/jani/Property.h"

namespace {

std::vector<storm::modelchecker::FilterType> const allFilterTypes = {storm::modelchecker::FilterType::MIN,    storm::modelchecker::FilterType::MAX,
                                                                     storm::modelchecker::FilterType::SUM,    storm::modelchecker::FilterType::AVG,
                                                                     storm::modelchecker::FilterType::COUNT,  storm::modelchecker::FilterType::FORALL,
                                                                     storm::modelchecker::FilterType::EXISTS, storm::modelchecker::FilterType::ARGMIN,
                                                                     storm::modelchecker::FilterType::ARGMAX, storm::modelchecker::FilterType::VALUES};

storm::jani::Property filteredProperty(std::string const& name, std::string const& formula, storm::modelchecker::FilterType ft,
                                       std::string const& statesFormula) {
    storm::parser::FormulaParser parser;
    storm::jani::FilterExpression fe(parser.parseSingleFormulaFromString(formula), ft, parser.parseSingleFormulaFromString(statesFormula));
    return storm::jani::Property(name, fe, {});
}

storm::jani::Property plainProperty(std::string const& name, std::string const& formula, std::string const& comment = "") {
    storm::parser::FormulaParser parser;
    return storm::jani::Property(name, parser.parseSingleFormulaFromString(formula), {}, comment);
}

}  // namespace

TEST(PropertyTest, UnnamedProperty) {
    std::string const formula = "P=? [F \"target\"]";
    std::string const expected = formula + ";";

    auto const property = plainProperty("", formula);

    EXPECT_EQ(expected, property.asPrismSyntax());
}

TEST(PropertyTest, NamedProperty) {
    std::string const name = "p";
    std::string const formula = "P=? [F \"target\"]";
    std::string const expected = "\"" + name + "\": " + formula + ";";

    auto const property = plainProperty(name, formula);

    EXPECT_EQ(expected, property.asPrismSyntax());
}

TEST(PropertyTest, NamedPropertyWithFilter) {
    std::string const name = "p";
    std::string const formula = "P=? [F \"target\"]";
    std::string const statesFormula = "\"a\"";
    std::string const expected = "\"" + name + "\": filter(max, " + formula + ", " + statesFormula + ");";

    auto const property = filteredProperty(name, formula, storm::modelchecker::FilterType::MAX, statesFormula);

    EXPECT_EQ(expected, property.asPrismSyntax());
}

TEST(PropertyTest, NamedPropertyWithFilterInitStates) {
    std::string const name = "p";
    std::string const formula = "P=? [F \"target\"]";
    std::string const statesFormula = "\"init\"";
    std::string const expected = "\"" + name + "\": filter(max, " + formula + ", " + statesFormula + ");";

    auto const property = filteredProperty(name, formula, storm::modelchecker::FilterType::MAX, statesFormula);

    EXPECT_EQ(expected, property.asPrismSyntax());

    storm::parser::FormulaParser parser;
    auto const reparsed = parser.parseFromString(expected);
    ASSERT_EQ(1ull, reparsed.size());
    EXPECT_EQ(expected, reparsed.front().asPrismSyntax());
}

TEST(PropertyTest, FilterTypeKeywords) {
    std::set<std::string> keywords;

    for (auto const& ft : allFilterTypes) {
        std::string const keyword = storm::modelchecker::toPrismSyntax(ft);
        EXPECT_FALSE(keyword.empty());
        keywords.insert(keyword);
    }
    EXPECT_EQ(allFilterTypes.size(), keywords.size());
}

TEST(PropertyTest, FilterTypeKeywordsRoundTrip) {
    std::string const name = "p";
    std::string const formula = "P=? [F \"a\"]";
    std::string const statesFormula = "\"b\"";

    storm::parser::FormulaParser parser;

    for (auto const& ft : allFilterTypes) {
        std::string const expected = "\"" + name + "\": filter(" + storm::modelchecker::toPrismSyntax(ft) + ", " + formula + ", " + statesFormula + ");";

        auto const properties = parser.parseFromString(expected);
        ASSERT_EQ(1ull, properties.size()) << expected;
        EXPECT_EQ(ft, properties.front().getFilter().getFilterType()) << expected;
        EXPECT_EQ(expected, properties.front().asPrismSyntax());
    }
}

// 'values' is not a valid PRISM filter keyword (see 'printall')
TEST(PropertyTest, FilterKeywordNonExistent) {
    std::string const name = "p";
    std::string const formula = "P=? [F \"a\"]";
    std::string const statesFormula = "\"b\"";
    std::string const input = "\"" + name + "\": filter(values, " + formula + ", " + statesFormula + ");";

    storm::parser::FormulaParser parser;
    STORM_SILENT_EXPECT_THROW(parser.parseFromString(input), storm::exceptions::WrongFormatException);
}

TEST(PropertyTest, CommentIsSeparate) {
    std::string const name = "p";
    std::string const formula = "P=? [F \"target\"]";
    std::string const comment = "very important property";
    std::string const expected = "\"" + name + "\": " + formula + ";";

    auto const property = plainProperty(name, formula, comment);

    EXPECT_EQ(expected, property.asPrismSyntax());
    EXPECT_EQ(comment, property.getComment());
}

TEST(PropertyTest, StreamAndStringEqual) {
    std::string const name = "p";
    std::string const formula = "P=? [F \"target\"]";
    std::string const statesFormula = "\"a\"";

    auto const property = filteredProperty(name, formula, storm::modelchecker::FilterType::MAX, statesFormula);

    std::stringstream stream;
    property.asPrismSyntax(stream);

    EXPECT_EQ(property.asPrismSyntax(), stream.str());
}
