#pragma once

#include <ranges>
#include <variant>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/exceptions/UnexpectedException.h"
#include "storm/storage/BitVector.h"
#include "storm/storage/umb/model/FileTypes.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

namespace storm::umb {

class GenericVector {
   public:
    template<typename T>
    using Vec = storm::umb::VectorType<T>;

    template<typename T>
    void set(Vec<T>&& v) {
        data = std::move(v);
    }

    template<typename T>
    void set(Vec<T> const& v) {
        data = v;
    }

    void unset();

    template<typename T>
    Vec<T>& get() {
        STORM_LOG_ASSERT(std::holds_alternative<Vec<T>>(data), "GenericVector does not hold a value of requested type.");
        return std::get<Vec<T>>(data);
    }

    template<typename T>
    Vec<T> const& get() const {
        STORM_LOG_ASSERT(std::holds_alternative<Vec<T>>(data), "GenericVector does not hold a value of requested type.");
        return std::get<Vec<T>>(data);
    }

    template<typename T>
    bool isType() const {
        return std::holds_alternative<Vec<T>>(data);
    }

    /*!
     * @return true if this holds some vector of any type.
     */
    bool hasValue() const;

    /*!
     * @return the size of the held vector. 0 if no vector is held.
     */
    uint64_t size() const;

    template<typename FromType, typename ToType>
    auto convertFromTo() const {
        if constexpr (std::is_same_v<FromType, ToType>) {
            return get<ToType>();
        } else {
            return get<FromType>() |
                   std::ranges::views::transform([](FromType const& value) -> ToType { return storm::utility::convertNumber<ToType, FromType>(value); });
        }
    }

    template<typename T>
    std::vector<T> asVector() const {
        if (isType<T>()) {
            return std::vector<T>(get<T>().begin(), get<T>().end());
        } else {
            if constexpr (!std::is_same_v<T, bool>) {
                if (isType<uint64_t>()) {
                    auto convertedView = convertFromTo<uint64_t, T>();
                    return std::vector<T>(convertedView.begin(), convertedView.end());
                } else if (isType<int64_t>()) {
                    auto convertedView = convertFromTo<int64_t, T>();
                    return std::vector<T>(convertedView.begin(), convertedView.end());
                } else if (isType<double>()) {
                    auto convertedView = convertFromTo<double, T>();
                    return std::vector<T>(convertedView.begin(), convertedView.end());
                } else if (isType<storm::RationalNumber>()) {
                    auto convertedView = convertFromTo<storm::RationalNumber, T>();
                    return std::vector<T>(convertedView.begin(), convertedView.end());
                } else if (isType<storm::Interval>()) {
                    auto convertedView = convertFromTo<storm::Interval, T>();
                    return std::vector<T>(convertedView.begin(), convertedView.end());
                } else if (isType<storm::RationalInterval>()) {
                    auto convertedView = convertFromTo<storm::RationalInterval, T>();
                    return std::vector<T>(convertedView.begin(), convertedView.end());
                }
            }
            STORM_LOG_THROW(false, storm::exceptions::UnexpectedException, "Unexpected type.");
        }
    }

    template<typename ValueType>
    ValueType at(uint64_t index) const {
        STORM_LOG_ASSERT(isType<double>() || isType<storm::RationalNumber>(), "unexpected type");
        if (isType<double>()) {
            return storm::utility::convertNumber<ValueType>(get<double>()[index]);
        } else {
            return storm::utility::convertNumber<ValueType>(get<storm::RationalNumber>()[index]);
        }
    }

   private:
    std::variant<std::monostate, Vec<bool>, Vec<uint64_t>, Vec<int64_t>, Vec<double>, Vec<storm::RationalNumber>, Vec<storm::Interval>,
                 Vec<storm::RationalInterval>>
        data;
};
}  // namespace storm::umb