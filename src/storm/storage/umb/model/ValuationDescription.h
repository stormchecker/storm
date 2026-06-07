#pragma once

#include <cstdint>
#include <optional>
#include <variant>

#include "storm/adapters/JsonAdapter.h"
#include "storm/adapters/JsonSerializationAdapter.h"
#include "storm/storage/umb/model/Type.h"

namespace storm::umb {
struct ValuationClassDescription {
    struct Padding {
        uint64_t padding{0};
        static auto constexpr JsonKeys = {"padding"};
        using JsonSerialization = storm::JsonSerialization;
    };
    struct Variable {
        std::string name;
        std::optional<bool> isOptional;
        SizedType type;
        std::optional<int64_t> lower, upper, offset;
        static auto constexpr JsonKeys = {"name", "is-optional", "type", "lower", "upper", "offset"};
        using JsonSerialization = storm::JsonSerialization;
    };
    std::vector<std::variant<Padding, Variable>> variables;
    static auto constexpr JsonKeys = {"variables"};
    using JsonSerialization = storm::JsonSerialization;

    /*!
     * Computes the size in bits of a valuation.
     * The size is the sum of all padding and size values plus 1 for each variable where isOptional is true.
     */
    uint64_t sizeInBits() const;

    /*!
     * @return true iff there is at least one variable of type String in this class description.
     */
    bool hasStringVariable() const;
};

struct ValuationDescription {
    bool unique{false};
    std::optional<uint64_t> numStrings;
    std::vector<ValuationClassDescription> classes;
    static auto constexpr JsonKeys = {"unique", "#strings", "classes"};
    using JsonSerialization = storm::JsonSerialization;
};
}  // namespace storm::umb
