#pragma once

#include "storm/adapters/JsonForward.h"

#include <boost/pfr.hpp>
#include <ostream>
#include <variant>
#include "storm/exceptions/WrongFormatException.h"
#include "storm/utility/macros.h"

namespace storm {
/// Helper struct to enable json serialization
struct JsonSerialization {};

template<typename EnumDecl>
    requires std::is_enum_v<typename EnumDecl::Values>
class SerializedEnum {
   public:
    using EnumDeclaration = EnumDecl;
    using E = typename EnumDeclaration::Values;
    auto static constexpr Keys = EnumDeclaration::Keys;
    E static constexpr Uninitialized = static_cast<E>(std::numeric_limits<std::underlying_type_t<E>>::max());

    SerializedEnum() = default;
    SerializedEnum(E value) : value(value) {}
    SerializedEnum(std::string_view str) {
        auto findRes = std::find(std::begin(Keys), std::end(Keys), str);
        STORM_LOG_THROW(findRes != std::end(Keys), storm::exceptions::WrongFormatException, "Invalid enum value '" << str << "'.");
        value = static_cast<E>(std::distance(std::begin(Keys), findRes));
    }

    bool isInitialized() const {
        return value != Uninitialized;
    }

    operator E() const {
        STORM_LOG_ASSERT(isInitialized(), "Enum value not initialized.");
        return value;
    }

    bool operator==(E other) const {
        return value == other;
    }

    std::string_view toString() const {
        auto const index = static_cast<std::underlying_type_t<E>>(value);
        STORM_LOG_ASSERT(isInitialized(), "Enum value not initialized.");
        STORM_LOG_ASSERT(std::cmp_less(index, Keys.size()), "Enum value with index " << index << " does not have a key.");
        return *(std::begin(Keys) + index);
    }

    friend std::ostream& operator<<(std::ostream& os, SerializedEnum const& val) {
        os << val.toString();
        return os;
    }

   private:
    E value{Uninitialized};
};
}  // namespace storm

NLOHMANN_JSON_NAMESPACE_BEGIN

template<typename T>
concept StormEnableSerializationConcept = std::is_same_v<typename T::JsonSerialization, ::storm::JsonSerialization>;

template<typename T>
concept StormIsOptional = std::same_as<T, std::optional<typename T::value_type>>;

template<StormEnableSerializationConcept T>
struct adl_serializer<T> {
    static_assert(T::JsonKeys.size() == boost::pfr::tuple_size_v<T>, "Number of JsonKeys does not match number of fields in struct.");

    template<typename JsonType>
    static void to_json(JsonType& json, T const& val) {
        to_json_i<0>(json, val);
        if (json.is_null()) {
            json = typename JsonType::object_t();
        }
    }

    template<typename JsonType>
    static void from_json(JsonType const& json, T& val) {
        STORM_LOG_THROW(json.is_object(), ::storm::exceptions::WrongFormatException, "Expected an object, got something else.");
        from_json_i<0>(json, val);
        // TODO: check if additional fields are present
    }

   private:
    static auto get_key(std::size_t i) {
        return std::data(T::JsonKeys)[i];
    }

    template<std::size_t I, typename JsonType>
    static void to_json_i(JsonType& json, T const& val) {
        if constexpr (I < boost::pfr::tuple_size_v<T>) {
            if constexpr (StormIsOptional<boost::pfr::tuple_element_t<I, T>>) {
                if (boost::pfr::get<I>(val).has_value()) {
                    json[get_key(I)] = boost::pfr::get<I>(val).value();
                }
            } else {
                json[get_key(I)] = boost::pfr::get<I>(val);
            }
            to_json_i<I + 1>(json, val);
        }
    }

    template<std::size_t I, typename JsonType>
    static void from_json_i(JsonType const& json, T& val) {
        if constexpr (I < boost::pfr::tuple_size_v<T>) {
            if constexpr (StormIsOptional<boost::pfr::tuple_element_t<I, T>>) {
                if (json.contains(get_key(I))) {
                    boost::pfr::get<I>(val) = json.at(get_key(I)).template get<typename boost::pfr::tuple_element_t<I, T>::value_type>();
                } else {
                    boost::pfr::get<I>(val) = std::nullopt;
                }
            } else {
                json.at(get_key(I)).get_to(boost::pfr::get<I>(val));
            }
            from_json_i<I + 1>(json, val);
        }
    }
};

template<typename T>
concept StormIsSerializedEnum = std::same_as<T, ::storm::SerializedEnum<typename T::EnumDeclaration>>;

template<StormIsSerializedEnum T>
struct adl_serializer<T> {
    template<typename JsonType>
    static void to_json(JsonType& json, T const& val) {
        json = val.toString();
    }

    template<typename JsonType>
    static void from_json(JsonType const& json, T& val) {
        STORM_LOG_THROW(json.is_string(), ::storm::exceptions::WrongFormatException, "Expected a string, got something else.");
        val = T(json.template get<std::string>());
    }
};

template<typename... Ts>
struct adl_serializer<std::variant<Ts...>> {
    template<typename JsonType>
    static void to_json(JsonType& json, std::variant<Ts...> const& val) {
        std::visit([&json](auto const& v) { adl_serializer<std::decay_t<decltype(v)>>::to_json(json, v); }, val);
    }

    template<std::size_t I, typename JsonType>
    static void from_json_i(JsonType const& json, std::variant<Ts...>& val) {
        static_assert(I < std::variant_size_v<std::variant<Ts...>>, "Index out of bounds.");
        using Ti = std::variant_alternative_t<I, std::variant<Ts...>>;
        if constexpr (I + 1 == std::variant_size_v<std::variant<Ts...>>) {
            // Last alternative, try to deserialize directly (potentially throwing some error if it fails)
            val = json.template get<Ti>();
        } else {
            // There are more alternatives left, so check if the current type matches
            if (json.is_object()) {
                if constexpr (StormEnableSerializationConcept<Ti>) {
                    if (std::all_of(json.items().begin(), json.items().end(),
                                    [](auto const& item) { return std::find(Ti::JsonKeys.begin(), Ti::JsonKeys.end(), item.key()) != Ti::JsonKeys.end(); })) {
                        // All keys in the json can be found in the struct
                        val = json.template get<Ti>();
                        return;
                    }
                }
            } else if ((json.is_boolean() && std::is_same_v<Ti, bool>) || (json.is_string() && std::is_same_v<Ti, std::string>) ||
                       (json.is_number_integer() && std::is_integral_v<Ti>) ||
                       (json.is_number_float() && (std::is_same_v<Ti, double> || std::is_same_v<Ti, typename JsonType::number_float_t>))) {
                // Type matches
                val = json.template get<Ti>();
                return;
            }
            // If we reach this point, we try the next alternative
            from_json_i<I + 1>(json, val);
        }
    }

    template<typename JsonType>
    static void from_json(JsonType const& json, std::variant<Ts...>& val) {
        from_json_i<0>(json, val);
    }
};

NLOHMANN_JSON_NAMESPACE_END