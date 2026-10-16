#include "storm/modelchecker/certificates/utility/CertificateJsonUtility.h"

#include <sstream>

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/BitVector.h"
#include "storm/utility/ExtendedNumber.h"
#include "storm/utility/NumberTraits.h"

namespace storm::modelchecker::detail {

template<typename ValueType, typename ElementType>
storm::json<ValueType> valuesToJson(std::vector<ElementType> const& values) {
    auto result = storm::json<ValueType>::array();
    for (auto const& value : values) {
        if (storm::utility::isInfinity(value)) {
            result.push_back("inf");
        } else if (storm::utility::isNegativeInfinity(value)) {
            result.push_back("-inf");
        } else if constexpr (storm::NumberTraits<ValueType>::IsExact) {
            std::stringstream ss;
            ss << storm::utility::getFinite(value);
            result.push_back(ss.str());
        } else {
            result.push_back(storm::utility::getFinite(value));
        }
    }
    return result;
}

template<typename ValueType>
storm::json<ValueType> ranksToJson(std::vector<RankingType> const& ranks) {
    auto result = storm::json<ValueType>::array();
    for (auto const rank : ranks) {
        if (rank == InfRank) {
            result.push_back("inf");
        } else {
            result.push_back(rank);
        }
    }
    return result;
}

template<typename ValueType>
storm::json<ValueType> stateSetToJson(std::string const& label, storm::storage::BitVector const& states) {
    storm::json<ValueType> result;
    result["label"] = label;
    auto stateArray = storm::json<ValueType>::array();
    for (auto const state : states) {
        stateArray.push_back(state);
    }
    result["states"] = std::move(stateArray);
    return result;
}

template storm::json<double> valuesToJson<double, double>(std::vector<double> const&);
template storm::json<storm::RationalNumber> valuesToJson<storm::RationalNumber, storm::RationalNumber>(std::vector<storm::RationalNumber> const&);
template storm::json<storm::RationalNumber> valuesToJson<storm::RationalNumber, storm::ExtendedRationalNumber>(
    std::vector<storm::ExtendedRationalNumber> const&);

template storm::json<double> ranksToJson<double>(std::vector<RankingType> const&);
template storm::json<storm::RationalNumber> ranksToJson<storm::RationalNumber>(std::vector<RankingType> const&);

template storm::json<double> stateSetToJson<double>(std::string const&, storm::storage::BitVector const&);
template storm::json<storm::RationalNumber> stateSetToJson<storm::RationalNumber>(std::string const&, storm::storage::BitVector const&);

}  // namespace storm::modelchecker::detail
