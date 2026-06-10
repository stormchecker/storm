#pragma once
#include <cstdint>
#include <ranges>
#include <string_view>

#include "storm/storage/umb/model/FileTypes.h"
#include "storm/utility/macros.h"

namespace storm::umb {

auto inline stringVectorView(SEQ<char> const& strings, CSR const& stringMapping) {
    STORM_LOG_ASSERT(!stringMapping.has_value() || std::ranges::size(stringMapping.value()) > 0, "stringMapping CSR must not be empty.");
    STORM_LOG_ASSERT(stringMapping.has_value() == strings.has_value(), "stringMapping must be present iff strings is present.");
    auto const numEntries = stringMapping.has_value() ? std::ranges::size(stringMapping.value()) - 1 : 0;
    return std::ranges::iota_view(0ull, numEntries) |
           std::ranges::views::transform([stringPtr = strings.has_value() ? strings.value().data() : nullptr, &stringMapping](auto i) -> std::string_view {
               // Note: this is only executed if numEntries is positive, i.e., if there actually are strings
               STORM_LOG_ASSERT(stringPtr != nullptr, "Expected strings to be present if there are entries in the string mapping.");
               return std::string_view(stringPtr + stringMapping.value()[i], stringMapping.value()[i + 1] - stringMapping.value()[i]);
           });
}

class StringsBuilder {
   public:
    StringsBuilder(typename SEQ<char>::value_type& strings, typename CSR::value_type& stringMapping) : strings(strings), stringMapping(stringMapping) {
        if (stringMapping.empty()) {
            stringMapping.push_back(0);
        }
        STORM_LOG_ASSERT(stringMapping.front() == 0, "String mapping CSR must start with 0.");
        STORM_LOG_ASSERT(stringMapping.back() == strings.size(), "String mapping CSR must end with the size of the strings.");
    }

    void reserve(uint64_t numStrings) {
        strings.reserve(strings.size() + numStrings * 10);  // assume average string length of 10
        stringMapping.reserve(stringMapping.size() + numStrings);
    }

    std::string_view at(uint64_t index) const {
        STORM_LOG_ASSERT(index + 1 < stringMapping.size(), "String index out of bounds.");
        return std::string_view(strings.data() + stringMapping[index], stringMapping[index + 1] - stringMapping[index]);
    }

    std::size_t size() const {
        return stringMapping.size() - 1;
    }

    /*!
     * inserts the string at the end of the vector and returns its index
     * @note does not check for duplicates
     */
    uint64_t push_back(std::string_view str) {
        strings.insert(strings.end(), str.begin(), str.end());
        stringMapping.push_back(strings.size());
        return size() - 1;
    }

    /*!
     * If the given string already exists, returns its index. Otherwise, inserts it at the end and returns the new index.
     */
    uint64_t findOrPushBack(std::string_view str) {
        // search for existing string (note: this is a linear search)
        auto const numStrings = stringMapping.size() - 1;
        for (uint64_t i = 0; i < numStrings; ++i) {
            if (at(i) == str) {
                return i;
            }
        }
        // not found, insert new string
        return push_back(str);
    }

    void finalize() {
        STORM_LOG_ASSERT(stringMapping.back() == strings.size(), "Final string mapping does not match string size.");
        strings.shrink_to_fit();
        stringMapping.shrink_to_fit();
    }

   private:
    typename SEQ<char>::value_type& strings;
    typename CSR::value_type& stringMapping;
};

}  // namespace storm::umb