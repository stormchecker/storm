#include "storm/utility/string.h"

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace storm {
namespace utility {
namespace string {

SimilarStrings::SimilarStrings(std::string reference, double similarityFactor, bool caseSensitive)
    : reference(reference), similarityFactor(similarityFactor), caseSensitive(caseSensitive) {
    // intentionally left empty.
}

bool SimilarStrings::isSimilar(std::string const& string) const {
    double distance = levenshteinDistance(reference, string, caseSensitive);
    return distance <= static_cast<double>(std::max(reference.size(), string.size())) * (1.0 - similarityFactor);
}

bool SimilarStrings::add(std::string const& string) {
    if (isSimilar(string)) {
        distances.emplace(storm::utility::string::levenshteinDistance(reference, string, caseSensitive), string);
        return true;
    }
    return false;
}

std::vector<std::string> SimilarStrings::toList() const {
    std::vector<std::string> result;
    for (auto const& dist : distances) {
        result.push_back(dist.second);
    }
    return result;
}

std::string SimilarStrings::toDidYouMeanString() const {
    uint64_t size = distances.size();
    std::string result = boost::algorithm::join(toList(), ", ");
    if (size == 0) {
        return "";
    } else if (size == 1) {
        return "Did you mean '" + result + "'?";
    } else {
        return "Did you mean any of [" + result + "] ?";
    }
}

uint64_t levenshteinDistance(std::string const& lhs, std::string const& rhs, bool caseSensitive) {
    std::vector<std::vector<uint64_t>> d(lhs.size() + 1, std::vector<uint64_t>(rhs.size() + 1, 0ull));
    for (uint64_t row = 1; row < d.size(); ++row) {
        d[row].front() = row;
    }
    for (uint64_t col = 1; col < d.front().size(); ++col) {
        d.front()[col] = col;
    }

    for (uint64_t row = 1; row < d.size(); ++row) {
        for (uint64_t col = 1; col < d[row].size(); ++col) {
            uint64_t cost = 1;
            if (caseSensitive) {
                if (lhs[row - 1] == rhs[col - 1]) {
                    cost = 0;
                }
            } else {
                if (tolower(lhs[row - 1]) == tolower(rhs[col - 1])) {
                    cost = 0;
                }
            }
            d[row][col] = std::min({d[row - 1][col] + 1, d[row][col - 1] + 1, d[row - 1][col - 1] + cost});
        }
    }
    return d.back().back();
}

std::vector<std::string> parseCommaSeparatedStrings(std::string const& input) {
    std::vector<std::string> result;
    if (!input.empty()) {
        boost::split(result, input, boost::is_any_of(","));
        for (auto& entry : result) {
            boost::trim(entry);
        }
    }
    return result;
}
}  // namespace string
}  // namespace utility
}  // namespace storm
