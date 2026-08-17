#pragma once

#include <filesystem>
#include <memory>

#include "storm/models/sparse/Model.h"
#include "storm/models/sparse/StandardRewardModel.h"

namespace storm::parser {

struct DirectEncodingParserOptions {
    bool buildChoiceLabeling = false;
};

enum class DirectEncodingValueType { Default, Double, DoubleInterval, Rational, RationalInterval, Parametric };

/*!
 * Parses the given file in DRN format.
 * The value type can be specified via the valueType parameter. If it is set to Default, the value type of the returned model is derived by the `@value_type`
 * section in the file.
 * @throw WrongFormatException if the specified value type is incompatible with the file content (e.g. parametric into double)
 * @throw WrongFormatException if the valueType parameter is `Default` and the DRN file does not have a `@value_type` section
 * @note if the provided file is an interval model, the value type is potentially promoted to the corresponding interval variant (double -> storm::Interval)
 * @param file path to DRN file
 * @param valueType Value type used for output model.
 * @param options Parsing options.
 */
std::shared_ptr<storm::models::ModelBase> parseDirectEncodingModel(std::filesystem::path const& file, DirectEncodingValueType valueType,
                                                                   DirectEncodingParserOptions const& options = DirectEncodingParserOptions());

/*!
 * Parses the given file in DRN format.
 * The value type is derived from the given template parameter.
 * @throw WrongFormatException if the specified value type is incompatible with the file content (e.g. parametric into double)
 * @throw WrongFormatException if the valueType parameter is `Default` and the DRN file does not have a `@value_type` section
 * @throw WrongFormatException if the provided file is an interval model but the ValueType is not an interval.
 * @param file path to DRN file
 * @param valueType Value type used for output model.
 * @param options Parsing options.
 */
template<typename ValueType, typename RewardModelType = models::sparse::StandardRewardModel<ValueType>>
std::shared_ptr<storm::models::sparse::Model<ValueType, RewardModelType>> parseDirectEncodingModel(
    std::filesystem::path const& file, DirectEncodingParserOptions const& options = DirectEncodingParserOptions());

}  // namespace storm::parser
