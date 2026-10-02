#include "storm-config.h"
#include "test/storm_gtest.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>

#include "storm/utility/matching.h"

namespace {

/*!
 * @return a predicate for the bipartite graph in which the domain vertex v is adjacent to the image vertices listed in adjacency[v].
 */
auto edges(std::vector<std::vector<uint64_t>> const& adjacency) {
    return [&adjacency](uint64_t const v, uint64_t const u) { return std::find(adjacency[v].begin(), adjacency[v].end(), u) != adjacency[v].end(); };
}

}  // namespace

TEST(MatchingTest, PerfectMatchingIsFoundIfOneExists) {
    // Every vertex has two options here, so the (unique) bijection cannot be found by matching every domain vertex with its first option.
    std::vector<std::vector<uint64_t>> const adjacency{{0, 1}, {1, 2}, {2, 0}};
    auto const matching = storm::utility::findPerfectMatching(3, edges(adjacency));
    ASSERT_TRUE(matching.has_value());
    ASSERT_EQ(3ull, matching->size());
    EXPECT_EQ(std::set<uint64_t>({0, 1, 2}), std::set<uint64_t>(matching->begin(), matching->end())) << "not a bijection";
    for (uint64_t v = 0; v < 3; ++v) {
        EXPECT_TRUE(edges(adjacency)(v, (*matching)[v])) << "domain vertex " << v << " is not adjacent to the image vertex " << (*matching)[v];
    }
}

TEST(MatchingTest, NoPerfectMatchingIfTwoVerticesShareTheirOnlyOption) {
    // The domain vertices 0 and 1 both only fit the image vertex 0, so there is no bijection, although every vertex has an option.
    std::vector<std::vector<uint64_t>> const adjacency{{0}, {0}, {1, 2}};
    EXPECT_FALSE(storm::utility::findPerfectMatching(3, edges(adjacency)).has_value());
}

TEST(MatchingTest, EmptyGraphHasPerfectMatching) {
    auto const matching = storm::utility::findPerfectMatching(0, [](uint64_t, uint64_t) { return false; });
    ASSERT_TRUE(matching.has_value());
    EXPECT_TRUE(matching->empty());
}
