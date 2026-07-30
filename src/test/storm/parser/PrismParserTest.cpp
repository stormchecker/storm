#include "storm-config.h"
#include "test/storm_gtest.h"

#include "storm-parsers/parser/PrismParser.h"
#include "storm/exceptions/InvalidArgumentException.h"
#include "storm/exceptions/WrongFormatException.h"

TEST(PrismParser, StandardModelTest) {
    storm::prism::Program result;
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/coin2.nm"));
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/crowds5_5.pm"));
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/csma2_2.nm"));
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/die.pm"));
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/firewire.nm"));
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/leader3.nm"));
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/dtmc/leader3_5.pm"));
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/two_dice.nm"));
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/wlan0_collide.nm"));
}

TEST(PrismParser, SimpleTest) {
    std::string testInput =
        R"(dtmc
    module mod1
        b : bool;
        [a] true -> 1: (b'=true != false = b => false);
    endmodule)";

    storm::prism::Program result;
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));
    EXPECT_EQ(1ul, result.getNumberOfModules());
    EXPECT_EQ(storm::prism::Program::ModelType::DTMC, result.getModelType());
    EXPECT_FALSE(result.hasUnboundedVariables());

    testInput =
        R"(mdp
    
    module main
        x : [1..5] init 1;
        [] x=1 -> 1:(x'=2);
        [] x=2 -> 1:(x'=3);
        [] x=3 -> 1:(x'=1);
        [] x=3 -> 1:(x'=4);
        [] x=4 -> 1:(x'=5);
        [] x=5 -> 1: true;
    endmodule)";
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));
    EXPECT_EQ(1ul, result.getNumberOfModules());
    EXPECT_EQ(storm::prism::Program::ModelType::MDP, result.getModelType());
    EXPECT_FALSE(result.hasUnboundedVariables());
}

TEST(PrismParser, expressionTest) {
    std::string testInput = R"(dtmc

        module test
	        c : [0..1] init 0;
	        [] c = ceil(log(1,2)) -> (c'=1);
        endmodule)";

    storm::prism::Program result;
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));
    EXPECT_EQ(storm::prism::Program::ModelType::DTMC, result.getModelType());
    EXPECT_FALSE(result.hasUnboundedVariables());
    EXPECT_EQ(1ul, result.getNumberOfModules());
    ASSERT_TRUE(result.hasModule("test"));
    EXPECT_EQ(1ul, result.getModule("test").getNumberOfCommands());
    auto const& guard = result.getModule("test").getCommands().front().getGuardExpression().getBaseExpression();
    ASSERT_TRUE(guard.isBinaryRelationExpression());
    EXPECT_EQ(0ll, guard.asBinaryRelationExpression().getSecondOperand()->evaluateAsInt());
}

TEST(PrismParser, FormulaOrderTest) {
    // Regression test for #313: a boolean formula referencing an int-typed formula declared afterwards
    // must not raise a spurious type error while resolving the formula dependency order.
    std::string testInput =
        R"(mdp

        formula leader_set = ((A_mark & A_leader = current_min_leader_calc | A_mark = false)?true:false) & ((B_mark & B_leader = current_min_leader_calc | B_mark = false)?true:false);

        formula current_min_leader_calc = min((A_mark?A_leader:8), (B_mark?B_leader:8));

        module whatever
            A_mark : bool init true;
            B_mark : bool init true;

            test1 : bool init false;

            A_leader : [0..10] init 1;
            B_leader : [0..10] init 1;

            [] A_mark = true & leader_set & test1 = false -> (test1'=true);
        endmodule)";

    storm::prism::Program result;
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));
    ASSERT_TRUE(result.hasModule("whatever"));
    EXPECT_EQ(1ul, result.getModule("whatever").getNumberOfCommands());
}

TEST(PrismParser, NonReservedPlayerKeywordTest) {
    // Regression test for #335: "player"/"endplayer" are only reserved for SMGs.
    std::string testInput =
        R"(mdp

        module test
            player : [0..5] init 0;
            [] player < 5 -> (player'=player+1);
        endmodule)";

    storm::prism::Program result;
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));
    ASSERT_TRUE(result.hasModule("test"));
    ASSERT_EQ(1ul, result.getModule("test").getNumberOfIntegerVariables());
    EXPECT_EQ("player", result.getModule("test").getIntegerVariables().front().getName());
}

TEST(PrismParser, NonReservedInvariantKeywordTest) {
    // Regression test for #955 review feedback: "invariant"/"endinvariant" are only reserved for PTAs.
    std::string testInput =
        R"(mdp

        module test
            invariant : [0..5] init 0;
            [] invariant < 5 -> (invariant'=invariant+1);
        endmodule)";

    storm::prism::Program result;
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));
    ASSERT_TRUE(result.hasModule("test"));
    ASSERT_EQ(1ul, result.getModule("test").getNumberOfIntegerVariables());
    EXPECT_EQ("invariant", result.getModule("test").getIntegerVariables().front().getName());
}

TEST(PrismParser, ComplexTest) {
    std::string testInput =
        R"(ma
    
    const int a;
    const int b = 10;
    const bool c;
    const bool d = true | false;
    const double e;
    const double f = 9;
    
    formula test = a >= 10 & (max(a,b) > floor(e));
    formula test2 = a+b;
    formula test3 = (a + b > 10 ? floor(e) : h) + a;
    
    global g : bool init false;
    global h : [0 .. b];
    
    module mod1
        i : bool;
        j : bool init c;
        k : [125..a] init a;

        [a] test&false -> (i'=true)&(k'=1+1) + 1 : (k'=floor(e) + max(k, b) - 1 + k);
        [b] true -> (i'=i);
    endmodule
                                              
    module mod2
        [] (k > 3) & false & (min(a, 0) < max(h, k)) -> 1-e: (g'=(1-a) * 2 + floor(f) > 2);
    endmodule
    
    module mod3 = mod1 [ i = i1, j = j1, k = k1 ] endmodule
                                                               
    label "mal" = max(a, 10) > 0;
    
    rewards "testrewards"
        [a] true : a + 7;
        max(f, a) <= 8 : 2*b;
    endrewards
                                                               
    rewards "testrewards2"
        [b] true : a + 7;
        max(f, a) <= 8 : 2*b;
    endrewards)";

    storm::prism::Program result;
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));
    EXPECT_EQ(storm::prism::Program::ModelType::MA, result.getModelType());
    EXPECT_EQ(3ul, result.getNumberOfModules());
    EXPECT_EQ(2ul, result.getNumberOfRewardModels());
    EXPECT_EQ(1ul, result.getNumberOfLabels());
    EXPECT_FALSE(result.hasUnboundedVariables());
}

TEST(PrismParser, UnboundedTest) {
    std::string testInput =
        R"(mdp
    module main
        b : int;
        [a] true -> 1: (b'=b+1);
    endmodule)";

    storm::prism::Program result;
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));
    EXPECT_EQ(1ul, result.getNumberOfModules());
    EXPECT_EQ(storm::prism::Program::ModelType::MDP, result.getModelType());
    EXPECT_TRUE(result.hasUnboundedVariables());
}

TEST(PrismParser, POMDPInputTest) {
    std::string testInput =
        R"(pomdp

    observables
            i
    endobservables

    module example
    s : [0..4] init 0;
    i : bool init true;
    [] s=0 -> 0.5: (s'=1) & (i'=false) + 0.5: (s'=2) & (i'=false);
    [] s=1 | s=2 -> 1: (s'=3) & (i'=true);
    [r] s=1 -> 1: (s'=4) & (i'=true);
    [r] s=2 -> 1: (s'=3) & (i'=true);
    endmodule

    )";

    storm::prism::Program result;
    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));

    std::string testInput2 =
        R"(pomdp

        observable intermediate = s=1 | s=2;

        module example
        s : [0..4] init 0;
        [] s=0 -> 0.5: (s'=1) + 0.5: (s'=2);
        [l] s=1 -> 1: (s'=3);
        [l] s=2 -> 1: (s'=4);
        [r] s=1 -> 1: (s'=4);
        [r] s=2 -> 1: (s'=3);
        endmodule

    )";

    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput2, "testfile"));
}

TEST(PrismParser, NAryPredicates) {
    std::string testInput =
        R"(dtmc

    module example
    s : [0..4] init 0;
    i : bool init true;
    [] s=0 -> 0.5: (s'=1) & (i'=false) + 0.5: (s'=2) & (i'=false);
    [] s=1 | s=2 -> 1: (s'=3) & (i'=true);
    [r] s=1 -> 1: (s'=4) & (i'=true);
    [r] s=2 -> 1: (s'=3) & (i'=true);
    endmodule

    label "test" = atMostOneOf(s=0, s=3, s=4);
    label "test2" = exactlyOneOf(s=0, i, !i & s=3);
    )";
    storm::prism::Program result;

    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));
}

TEST(PrismParser, Intervals) {
    std::string testInput =
        R"(dtmc

    module example
    s : [0..4] init 0;
    i : bool init true;
    [] s=0 -> [0.5,0.9]: (s'=1) & (i'=false) + [0.4,0.6] : (s'=2) & (i'=false);
    [] s=1 -> 1: (s'=3) & (i'=true);
    [] s>0 -> [i?0.3:1/s,1]: (s'=4) & (i'=true) + 0.5 : (s'=3);
    [r] s=2 -> 1: (s'=3) & (i'=true);
    endmodule
    module example2 = example [ s = t, i = j ] endmodule

    )";
    storm::prism::Program result;

    EXPECT_NO_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"));
}

TEST(PrismParser, IllegalInputTest) {
    std::string testInput =
        R"(ctmc

    const int a;
    const bool a = true;
    
    module mod1
        c : [0 .. 8] init 1;
        [] c < 3 -> 2: (c' = c+1); 
    endmodule
    )";

    storm::prism::Program result;
    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    testInput =
        R"(dtmc
    
    const int a;
    
    module mod1
        a : [0 .. 8] init 1;
        [] a < 3 -> 1: (a' = a+1); 
    endmodule)";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    testInput =
        R"(dtmc
    
    const int a = 2;
    formula a = 41;
    
    module mod1
        c : [0 .. 8] init 1;
        [] c < 3 -> 1: (c' = c+1); 
    endmodule)";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    testInput =
        R"(dtmc
    
    const int a = 2;
    
    init
        c > 3
    endinit
    
    module mod1
        c : [0 .. 8] init 1;
        [] c < 3 -> 1: (c' = c+1); 
    endmodule
        
    init
        c > 3
    endinit

    )";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    testInput =
        R"(dtmc
    
    module mod1
        c : [0 .. 8] init 1;
        [] c < 3 -> 1: (c' = c+1);
    endmodule
                    
    module mod2
        [] c < 3 -> 1: (c' = c+1);
    endmodule)";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    testInput =
        R"(dtmc
                        
    module mod1
        c : [0 .. 8] init 1;
        [] c < 3 -> 1: (c' = c+1)&(c'=c-1);
    endmodule)";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    testInput =
        R"(dtmc
    
    module mod1
        c : [0 .. 8] init 1;
        [] c < 3 -> 1: (c' = true || false);
    endmodule)";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    testInput =
        R"(dtmc
    
    module mod1
        c : [0 .. 8] init 1;
        [] c + 3 -> 1: (c' = 1);
    endmodule)";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    testInput =
        R"(dtmc
    
    module mod1
        c : [0 .. 8] init 1;
        [] c + 3 -> 1: (c' = 1);
    endmodule
    
    label "test" = c + 1;
    
    )";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    // Regression test for #319: reserved keyword used as a variable name.
    testInput =
        R"(mdp

    module test
        ma : [0..5] init 0;
        [] ma < 5 -> (ma'=ma+1);
    endmodule)";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    // Regression test for #335: "player" remains reserved for SMGs.
    testInput =
        R"(smg

    module test
        player : [0..5] init 0;
        [] player < 5 -> (player'=player+1);
    endmodule)";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);

    // Regression test for #955 review feedback: "invariant" remains reserved for PTAs.
    testInput =
        R"(pta

    module test
        invariant : [0..5] init 0;
    endmodule)";

    STORM_SILENT_EXPECT_THROW(result = storm::parser::PrismParser::parseFromString(testInput, "testfile"), storm::exceptions::WrongFormatException);
}

TEST(PrismParser, IllegalSynchronizedWriteTest) {
    STORM_SILENT_EXPECT_THROW(storm::parser::PrismParser::parse(STORM_TEST_RESOURCES_DIR "/mdp/coin2-2-illegalSynchronizingWrite.nm"),
                              storm::exceptions::WrongFormatException);
}
