#pragma once
#include <ranges>
#include <span>
#include <vector>

#include "storm/adapters/IntervalAdapter.h"
#include "storm/adapters/RationalNumberAdapter.h"
#include "storm/storage/umb/model/FileTypes.h"
#include "storm/storage/umb/model/GenericVector.h"
#include "storm/storage/umb/model/Type.h"
#include "storm/utility/constants.h"
#include "storm/utility/macros.h"

#include "storm/exceptions/NotSupportedException.h"

namespace storm::umb {

class ValueEncoding {
   public:
    template<bool Signed, std::ranges::input_range InputRange>
        requires std::same_as<std::ranges::range_value_t<InputRange>, uint64_t>
    static storm::NumberTraits<storm::RationalNumber>::IntegerType decodeArbitraryPrecisionInteger(InputRange&& input) {
        using IntegerType = typename storm::NumberTraits<storm::RationalNumber>::IntegerType;
        auto const twoTo64 = storm::utility::pow<IntegerType>(2, 64);

        STORM_LOG_ASSERT(std::ranges::size(input) > 0, "Input range must not be empty.");
        // We assume a little endian representation, so we reverse the input to start with the most significant bits.
        auto reverse_input = std::ranges::reverse_view(std::forward<InputRange>(input));

        // Helper function to decode from an unsigned range (with the most significant bits first).
        auto decodeFromUnsignedRange = [](auto&& input) -> IntegerType {
            auto const twoTo64 = storm::utility::pow<IntegerType>(2, 64);
            auto inputIt = std::ranges::begin(input);
            auto const inputEnd = std::ranges::end(input);
            auto result = storm::utility::convertNumber<IntegerType>(*inputIt);
            for (++inputIt; inputIt != inputEnd; ++inputIt) {
                result *= twoTo64;
                result += storm::utility::convertNumber<IntegerType>(*inputIt);
            }
            return result;
        };

        if constexpr (Signed) {
            // Find out if the number is negative, in which case we would compute the two's complement.
            uint64_t constexpr mostSignificantBitMask = 1ull << 63;
            if (*std::ranges::begin(reverse_input) & mostSignificantBitMask) {
                // Two's complement (e.g. 1111...1101 is -3)
                return -decodeFromUnsignedRange(reverse_input | std::ranges::views::transform([](uint64_t value) { return ~value; })) - 1;
            }
        }
        // Reaching this point means that the number is positive (i.e. signed and unsigned representations coincide).
        return decodeFromUnsignedRange(reverse_input);
    }

    template<std::ranges::input_range InputRange>
        requires std::same_as<std::ranges::range_value_t<InputRange>, uint64_t>
    static auto uint64ToRationalRangeView(InputRange&& input, uint64_t const numberSize) {
        STORM_LOG_ASSERT(numberSize % 128ull == 0ull && numberSize > 0ull, "Type size must be a positive multiple of 128 for rational representation.");
        auto const uint64ValuesPerNumber = numberSize / 64ull;
        auto const size = std::ranges::size(input) / uint64ValuesPerNumber;
        return std::ranges::iota_view(0ull, size) | std::ranges::views::transform([&input, uint64ValuesPerNumber](auto i) -> storm::RationalNumber {
                   auto const left = uint64ValuesPerNumber * i;
                   auto const right = left + uint64ValuesPerNumber;
                   auto mid = left + uint64ValuesPerNumber / 2;

                   std::ranges::subrange numeratorRange{std::ranges::begin(input) + left, std::ranges::begin(input) + mid};
                   storm::RationalNumber numerator = decodeArbitraryPrecisionInteger<true>(numeratorRange);  // signed

                   std::ranges::subrange denominatorRange{std::ranges::begin(input) + mid, std::ranges::begin(input) + right};
                   storm::RationalNumber denominator = decodeArbitraryPrecisionInteger<false>(denominatorRange);  // unsigned

                   return numerator / denominator;
               });
    }

    template<std::ranges::input_range InputRange>
        requires std::same_as<std::ranges::range_value_t<InputRange>, uint64_t>
    static auto uint64ToRationalIntervalRangeView(InputRange&& input, uint64_t const numberSize) {
        STORM_LOG_ASSERT(numberSize % 256ull == 0ull && numberSize > 0ull,
                         "Type size must be a positive multiple of 256 for rational interval representation.");
        auto rationalView = uint64ToRationalRangeView(input, numberSize / 2);
        return std::ranges::iota_view(0ull, std::ranges::size(rationalView) / 2) | std::views::transform([rationalView](auto i) -> storm::RationalInterval {
                   return storm::RationalInterval{rationalView[2 * i], rationalView[2 * i + 1]};
               });
    }

    template<bool Signed>
    static uint64_t getSizeOfIntegerEncoding(typename storm::NumberTraits<storm::RationalNumber>::IntegerType const& value) {
        // The bitsize method returns the smallest n with -2^n <= x < 2^n.
        if constexpr (Signed) {
            // For signed integers, we need one additional bit for the sign.
            // the only exception is when value is equal to -2^n, in which case we don't need that extra bit.
            if (value < 0) {
                return storm::utility::bitsize(typename storm::NumberTraits<storm::RationalNumber>::IntegerType(value + 1)) + 1;
            }
            return storm::utility::bitsize(value) + 1;
        } else {
            // For unsigned integers, we get n with 2^{n-1} <= x < 2^n.
            return storm::utility::bitsize(value);
        }
    }

    template<std::ranges::input_range InputRange>
        requires std::same_as<std::ranges::range_value_t<InputRange>, storm::RationalNumber>
    static uint64_t getMinimalRationalSize(InputRange&& input, bool multiplesOf64) {
        // We may assume that the denominator is always positive as this is a requirement for both GMP and CLN
        static_assert(storm::RationalNumberDenominatorAlwaysPositive);
        uint64_t minimalIntegerSize = 1;
        for (auto const& r : input) {
            minimalIntegerSize = std::max(minimalIntegerSize, getSizeOfIntegerEncoding<true>(storm::utility::numerator(r)));
            minimalIntegerSize = std::max(minimalIntegerSize, getSizeOfIntegerEncoding<false>(storm::utility::denominator(r)));
        }
        if (multiplesOf64) {
            // Round up to the next multiple of 64
            minimalIntegerSize = ((minimalIntegerSize + 63ull) / 64ull) * 64ull;
        }
        // Each rational number consists of two integers (numerator and denominator)
        return minimalIntegerSize * 2ull;
    }

    template<std::ranges::input_range InputRange>
        requires std::same_as<std::ranges::range_value_t<InputRange>, storm::RationalInterval>
    static uint64_t getMinimalRationalIntervalSize(InputRange&& input, bool multiplesOf64) {
        return getMinimalRationalSize(intervalToBaseRangeView<storm::RationalNumber>(input), multiplesOf64) * 2ull;
    }

    template<bool Signed>
    static void appendEncodedInteger(std::vector<uint64_t>& result, typename storm::NumberTraits<storm::RationalNumber>::IntegerType const& value,
                                     uint64_t uint64BucketsPerInteger) {
        using IntegerType = typename storm::NumberTraits<storm::RationalNumber>::IntegerType;
        if (uint64BucketsPerInteger == 0) {
            STORM_LOG_ASSERT(value == storm::utility::zero<IntegerType>(), "Unexpected non-zero value for zero bucket size.");
            // nothing to do
        } else if constexpr (Signed) {
            if (value < 0) {
                // Two's complement representation (e.g. -3 is 1111...1101)
                // We encode the corresponding positive value and then invert the bits.
                appendEncodedInteger<true>(result, -(value + 1), uint64BucketsPerInteger);
                // Invert the bits
                for (auto& v : std::span<uint64_t>(result.end() - uint64BucketsPerInteger, result.end())) {
                    v = ~v;
                }
            } else {
                // We encode the non-negative value as if it were unsigned.
                appendEncodedInteger<false>(result, value, uint64BucketsPerInteger);
                // Special case: the most significant bit must not be set. Otherwise, it would indicate a negative number in two's complement.
                STORM_LOG_ASSERT((result.back() & (1ull << 63)) == 0ull,
                                 "Encoding error for positive signed integer: most significant bit is set. Not enough uint64 buckets allocated?");
            }
        } else {
            auto const twoTo64 = storm::utility::pow<IntegerType>(2, 64);
            // We assume a little endian representation, so we start with the least significant bits.

            STORM_LOG_ASSERT(value >= 0, "Value must be non-negative for unsigned encoding.");
            auto divisionResult = storm::utility::divide<IntegerType>(value, twoTo64);
            result.push_back(storm::utility::convertNumber<uint64_t, storm::RationalNumber>(divisionResult.second));
            uint64_t buckets = 1;
            while (divisionResult.first != 0) {
                divisionResult = storm::utility::divide<IntegerType>(divisionResult.first, twoTo64);
                result.push_back(storm::utility::convertNumber<uint64_t, storm::RationalNumber>(divisionResult.second));
                ++buckets;
            }
            // fill remaining buckets with zeros
            STORM_LOG_ASSERT(uint64BucketsPerInteger >= buckets, "Not enough uint64 buckets allocated for encoding integer value "
                                                                     << value << " (" << uint64BucketsPerInteger << " buckets allocated but " << buckets
                                                                     << " needed).");
            result.resize(result.size() + (uint64BucketsPerInteger - buckets), 0ull);
        }
    }

    static void appendEncodedRational(std::vector<uint64_t>& result, storm::RationalNumber const& value, uint64_t uint64BucketsPerInteger) {
        // We may assume that the denominator is always positive as this is a requirement for both GMP and CLN
        static_assert(storm::RationalNumberDenominatorAlwaysPositive);
        appendEncodedInteger<true>(result, storm::utility::numerator(value), uint64BucketsPerInteger);     // signed
        appendEncodedInteger<false>(result, storm::utility::denominator(value), uint64BucketsPerInteger);  // unsigned
    }

    template<std::ranges::input_range InputRange>
        requires std::same_as<std::ranges::range_value_t<InputRange>, storm::RationalNumber>
    static std::vector<uint64_t> createUint64FromRationalRange(InputRange&& input, uint64_t const numberSize) {
        STORM_LOG_ASSERT(numberSize % 128ull == 0ull && numberSize > 0ull, "Type size must be a positive multiple of 128 for rational representation.");
        auto const uint64BucketsPerNumber = numberSize / 64ull;
        auto const size = std::ranges::size(input) * uint64BucketsPerNumber;

        std::vector<uint64_t> values;
        values.reserve(size);
        for (auto const& r : input) {
            appendEncodedRational(values, r, numberSize / 128ull);
        }
        values.shrink_to_fit();
        STORM_LOG_ASSERT(values.size() == size, "Unexpected size of encoded rational values: " << values.size() << " vs. " << size);
        return values;
    }

    template<std::ranges::input_range InputRange>
        requires storm::IsIntervalType<std::ranges::range_value_t<InputRange>>
    static std::vector<uint64_t> createUint64FromRationalIntervalRange(InputRange&& input, uint64_t const numberSize) {
        STORM_LOG_ASSERT(numberSize % 256ull == 0ull && numberSize > 0ull,
                         "Type size must be a positive multiple of 256 for rational interval representation.");
        return createUint64FromRationalRange(intervalToBaseRangeView<storm::RationalNumber>(input), numberSize / 2);
    }

    template<std::ranges::input_range InputRange>
        requires std::same_as<std::ranges::range_value_t<InputRange>, double>
    static auto doubleToIntervalRangeView(InputRange&& input) {
        STORM_LOG_ASSERT(std::ranges::size(input) % 2 == 0, "Input size is not even: " << std::ranges::size(input));
        return std::ranges::iota_view(0ull, std::ranges::size(input) / 2) |
               std::views::transform([&input](auto i) -> storm::Interval { return storm::Interval{input[2 * i], input[2 * i + 1]}; });
    }

    template<typename BaseType, std::ranges::input_range InputRange>
        requires storm::IsIntervalType<std::ranges::range_value_t<InputRange>>
    static auto intervalToBaseRangeView(InputRange&& input) {
        return std::ranges::iota_view(0ull, std::ranges::size(input) * 2) | std::views::transform([&input](auto i) -> BaseType {
                   if (i % 2 == 0) {
                       return storm::utility::convertNumber<BaseType>(input[i / 2].lower());
                   } else {
                       return storm::utility::convertNumber<BaseType>(input[i / 2].upper());
                   }
               });
    }

    /*!
     * returns func(<decoded_input>) where <decoded_input> is a range that (if necessary) decodes and converts the given input.
     * @param func a function that takes a range of value type and returns the result
     * @param input the input vector
     * @param sourceType The type that the input vector represents
     * @param sourceTypeSize the number of bits of the encoding for the source type. This is necessary for non-trivially encoded source types (like rational).
     * @return
     */
    template<typename ValueType>
    static auto applyDecodedVector(auto&& func, storm::umb::GenericVector const& input, storm::umb::SizedType const& sourceType) {
        STORM_LOG_ASSERT(input.hasValue(), "Input vector is not set.");
        auto conversionView = [](auto&& input) {
            using SourceType = std::ranges::range_value_t<decltype(input)>;
            if constexpr (std::same_as<ValueType, SourceType>) {
                return input;
            } else {
                return input | std::ranges::views::transform(
                                   [](SourceType const& value) -> ValueType { return storm::utility::convertNumber<ValueType, SourceType>(value); });
            }
        };

        using enum storm::umb::Type;

        // find out how to interpret the input values
        switch (sourceType.type) {
            case Double:
                STORM_LOG_WARN_COND(
                    !storm::NumberTraits<ValueType>::IsExact,
                    "Some values are given in type double but will be converted to an exact (arbitrary precision) type. Rounding errors may occur.");
                STORM_LOG_ASSERT(input.template isType<double>(), "Unexpected type for values. Expected double.");
                STORM_LOG_ASSERT(sourceType.bitSize() == 64, "Unexpected source type size for double representation. Expected 64.");
                return func(conversionView(input.template get<double>()));
            case Rational:
                STORM_LOG_WARN_COND(storm::NumberTraits<ValueType>::IsExact,
                                    "Some values are given in an exact type but converted to an inexact type. Rounding errors may occur.");
                if (input.template isType<storm::RationalNumber>()) {
                    return func(conversionView(input.template get<storm::RationalNumber>()));
                } else {
                    STORM_LOG_ASSERT(input.template isType<uint64_t>(), "Unexpected type for rational representation. Expected uint64.");
                    return func(conversionView(uint64ToRationalRangeView(input.template get<uint64_t>(), sourceType.bitSize())));
                }
            case DoubleInterval:
                // For intervals, there is no suitable value conversion to non-interval types since we would drop the uncertainty
                if constexpr (!storm::IsIntervalType<ValueType>) {
                    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                    "Some values are given as double intervals but a model with a non-interval type is requested.");
                    return func(std::ranges::empty_view<ValueType>{});
                } else {
                    STORM_LOG_WARN_COND(
                        !storm::NumberTraits<ValueType>::IsExact,
                        "Some values are given in type double but will be converted to an exact (arbitrary precision) type. Rounding errors may occur.");
                    STORM_LOG_ASSERT(sourceType.bitSize() == 128ull, "Unexpected source type size for double interval representation. Expected 128.");
                    if (input.template isType<storm::Interval>()) {
                        return func(conversionView(input.template get<storm::Interval>()));
                    } else {
                        STORM_LOG_ASSERT(input.template isType<double>(), "Unexpected type for double interval representation. Expected double.");
                        return func(conversionView(doubleToIntervalRangeView(input.template get<double>())));
                    }
                }
            case RationalInterval:
                // For intervals, there is no suitable value conversion to non-interval types since we would drop the uncertainty
                if constexpr (!storm::IsIntervalType<ValueType>) {
                    STORM_LOG_THROW(false, storm::exceptions::NotSupportedException,
                                    "Some values are given as rational intervals but a model with a non-interval type is requested.");
                    return func(std::ranges::empty_view<ValueType>{});
                } else {
                    STORM_LOG_WARN_COND(storm::NumberTraits<ValueType>::IsExact,
                                        "Some values are given in an exact type but converted to an inexact type. Rounding errors may occur.");
                    if (input.template isType<storm::RationalInterval>()) {
                        return func(conversionView(input.template get<storm::RationalInterval>()));
                    } else {
                        STORM_LOG_ASSERT(input.template isType<uint64_t>(), "Unexpected type for rational interval representation. Expected uint64.");
                        return func(conversionView(uint64ToRationalIntervalRangeView(input.template get<uint64_t>(), sourceType.bitSize())));
                    }
                }
            default:
                STORM_LOG_THROW(false, storm::exceptions::NotSupportedException, "Values have unsupported type " << sourceType.toString() << ".");
        }
    }

    template<typename ValueType>
    static std::vector<ValueType> createDecodedVector(storm::umb::GenericVector const& input, storm::umb::SizedType const& sourceType) {
        return applyDecodedVector<ValueType>(
            [](auto&& decodedInput) {
                std::vector<ValueType> v;
                v.reserve(std::ranges::size(decodedInput));
                for (auto&& value : decodedInput) {
                    v.push_back(value);
                }
                return v;
            },
            input, sourceType);
    }
};
}  // namespace storm::umb