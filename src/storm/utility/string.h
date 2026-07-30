#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace storm {
namespace utility {
namespace string {

class SimilarStrings {
   public:
    /*!
     * Gathers strings that are similar to the given reference string
     * @param reference
     * @param similarityFactor controls how similar the strings need to be (0 means any string is similar, 1 means only the reference string is similar)
     * @param caseSensitive if false, lower/upper case is ignored
     */
    SimilarStrings(std::string reference, double similarityFactor = 0.6, bool caseSensitive = true);

    /*!
     * @return true, if the given string is considered similar.
     */
    bool isSimilar(std::string const& string) const;

    /*!
     * Adds the given string to the set of similar strings (if it is similar)
     * @return true, if the given string is considered similar.
     */
    bool add(std::string const& string);

    /*!
     * Gets a list of all added strings that are similar to the reference string.
     */
    std::vector<std::string> toList() const;

    /*!
     * Returns a "Did you mean abc?" string
     * @return
     */
    std::string toDidYouMeanString() const;

   private:
    std::string reference;
    double similarityFactor;
    bool caseSensitive;

    struct comp {
        bool operator()(std::pair<uint64_t, std::string> const& lhs, std::pair<uint64_t, std::string> const& rhs) const {
            return lhs.first < rhs.first ? true : (lhs.first == rhs.first && lhs.second < rhs.second);
        }
    };
    std::set<std::pair<uint64_t, std::string>, comp> distances;
};

/*!
 * Levenstein distance to find similar strings
 */
uint64_t levenshteinDistance(std::string const& lhs, std::string const& rhs, bool caseSensitive = true);

/*!
 * Splits the input string on commas and trims whitespace from each element.
 */
std::vector<std::string> parseCommaSeparatedStrings(std::string const& input);
}  // namespace string
}  // namespace utility
}  // namespace storm
